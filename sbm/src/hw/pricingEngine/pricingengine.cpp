/*
 * Copyright 2021 Xilinx, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "pricingengine.hpp"

#include <math.h>

#include <iostream>

#ifndef __SYNTHESIS__
#include <fstream>
void full_adder_plus_1(int length, bool *reg)
{
    bool one = 1, carry = 0;
    for (int i = 0; i < length; i++)
    {
        carry = reg[i] & one;
        reg[i] = reg[i] ^ one;

        if (carry == 0)
            break;
    }
}

// Print/output the whole array
template <class T, int size>
void print_whole_vec(T vec[size], std::ostream& os) {
    for (int i = 0; i < size; i++) {
        os << vec[i] << " ";
    }
    os << std::endl;
}

// Print/output the first n elements of the array
template <class T, int size>
void print_vec(T vec[size], int n, std::ostream& os) {
    assert(n <= size);
    for (int i = 0; i < n; i++) {
        os << vec[i] << " ";
    }
    os << std::endl;
}

// Calculate the energy of the Ising Hamiltonian under a spin state
template <class T, int size>
void calc_energy(T vec[size], float matrix[size][size], float& energy) {
    energy = 0;
    float tmp[size] = {0};
    for (int i = 0; i < size; i++) {
        for (int j = 0; j < size; j++) {
            tmp[i] += matrix[i][j] * ((vec[j] > 0) ? 1 : -1);
        }
    }
    for (int i = 0; i < size; i++) {
        energy += tmp[i] * ((vec[i] > 0) ? 1 : -1);
    }
    //std::cout << "energy: " << energy << std::endl;
}

// Print/output the update message
template <class T, int size>
void print_update_msg(T old_energy, T new_energy, int old_step, int new_step,
                      bool best_spin[size], std::ostream& os) {
    os << "Update best solution:\n";
    os << "best_energy: " << old_energy << " → " << new_energy << "\n";
    os << "best_step: " << old_step << " → " << new_step << "\n";
    os << "best_spin:\n";
    print_whole_vec<bool, physical_bits>(best_spin, os);
    os << " → \n";
}

#endif

/**
 * PricingEngine Core
 */

void PricingEngine::responsePull(
    ap_uint<32> &regRxResponse,
    orderBookResponseStreamPack_t &responseStreamPack,
    orderBookResponseStream_t &responseStream) {
#pragma HLS PIPELINE II = 1 style = flp

    mmInterface intf;
    orderBookResponsePack_t responsePack;
    orderBookResponse_t response;

    static ap_uint<32> countRxResponse = 0;

    if (!responseStreamPack.empty()) {
        responsePack = responseStreamPack.read();
        intf.orderBookResponseUnpack(&responsePack, &response);
        responseStream.write(response);
        ++countRxResponse;
    }

    regRxResponse = countRxResponse;

    return;
}

void PricingEngine::pricingProcess(
    ap_uint<32> &regStrategyControl, ap_uint<32> &regProcessResponse,
    ap_uint<32> &regStrategyNone, ap_uint<32> &regStrategyPeg,
    ap_uint<32> &regStrategyLimit, ap_uint<32> &regStrategyUnknown,
    pricingEngineRegStrategy_t *regStrategies,
    orderBookResponseStream_t &responseStream,
    orderEntryOperationStream_t &operationStream) {
#pragma HLS PIPELINE II = 1 style = flp

    mmInterface intf;
    orderBookResponse_t response;
    orderEntryOperation_t operation;
    ap_uint<8> symbolIndex = 0;
    ap_uint<8> strategySelect = 0;
    ap_uint<8> thresholdEnable = 0;
    ap_uint<8> thresholdPosition = 0;
    // bool orderExecute = false;

    static float exch_logged_rates[physical_bits - 1] = {0};
    static ap_uint<32> orderId = 0;
    static ap_uint<32> countProcessResponse = 0;
    // static ap_uint<32> countStrategyNone = 0;
    // static ap_uint<32> countStrategyPeg = 0;
    // static ap_uint<32> countStrategyLimit = 0;
    // static ap_uint<32> countStrategyUnknown = 0;
    /* ERM / SBM debug signals */
    static bool regERMInitConstr = false;
    static ap_uint<32> countAncillaFlip = 0;
    static ap_uint<32> regSBMExecStatus = 0;

    // Start of QUBO formulation parameters
    const float M1 = 10; // 50;
    const float M2 = 10; // 25;

    // For SQA ONLY
    // static J[physical_bits][physical_bits];
    // static h[physical_bits] = {0};

    // For SBM ONLY
    const int steps = 10; // dt * steps = 100 (0.1 * 1000) will converge
    const float dt = 0.5;
    const float a0 = 1.;
    //const float c0 = 0.000636366292;
    const float c0 = 0.033613;
    static float J[physical_bits][physical_bits] = {0};
    float spin_x[physical_bits] = {0};
    float spin_y[physical_bits] = {0};

    // randomly initialize spin_y
    for (unsigned int i = 0; i < physical_bits; i++) {
        spin_y[i] = 0.1;
    }
    // spin_y[physical_bits - 1] = -0.1;

    // Start of original AAT code
    if (!responseStream.empty()) {
        response = responseStream.read();
        ++countProcessResponse;

        symbolIndex = response.symbolIndex;
        thresholdEnable = regStrategies[symbolIndex].enable.range(7, 0);

        // global strategy select override (across all symbols) for debug
        if (0 == (0x80000000 & regStrategyControl)) {
            strategySelect = regStrategies[symbolIndex].select.range(7, 0);
        } else {
            strategySelect = regStrategyControl.range(7, 0);
        }

        // We don't care about strategies
        // switch (strategySelect) {
        //     case (STRATEGY_NONE):
        //         // reserved for unprogrammed strategy, i.e. no action
        //         ++countStrategyNone;
        //         orderExecute = false;
        //         break;
        //     case (STRATEGY_PEG):
        //         ++countStrategyPeg;
        //         orderExecute = pricingStrategyPeg(
        //             thresholdEnable, thresholdPosition, response, operation);
        //         break;
        //     case (STRATEGY_LIMIT):
        //         ++countStrategyLimit;
        //         orderExecute = pricingStrategyLimit(
        //             thresholdEnable, thresholdPosition, response, operation);
        //         break;
        //     default:
        //         ++countStrategyUnknown;
        //         orderExecute = false;
        //         break;
        // }
        // end of original aat code

        // *2 => bid *2+1 => ask
        int exch_id = ((int)response.symbolIndex) * 2;
        // NOTE: input data originally is uint and we static_cast it to float
        unsigned int bidprice = response.bidPrice.range(31, 0);
        unsigned int askprice = response.askPrice.range(31, 0);

        ERM(exch_id, log(reinterpret_cast<float &>(bidprice)), M1, M2, J, exch_logged_rates, regERMInitConstr);
        ERM(exch_id + 1, log(reinterpret_cast<float &>(askprice)), M1, M2, J, exch_logged_rates, regERMInitConstr);

#ifndef __SYNTHESIS__
        // Coefficient check
        checkSBMCoeff<float>(J, c0);

        // Brute-force the best solution
        std::cout << "Brute-force calculation\n";
        bool best_spin_bf[physical_bits] = {0};
        bool spin_bf[physical_bits] = {0};
        float best_energy_bf = MAXFLOAT;
        float energy_bf = MAXFLOAT;
        long long bf_iteration = pow(2, physical_bits);
        if (bf_iteration == 0) {
            std::cerr << "Too many spins (" << physical_bits << ")" << ";\n"
                      << "Skip brute-force solution calculation\n";
        } else {
            long long iteration = 1;
            while (iteration < bf_iteration) {
                ++iteration;
                full_adder_plus_1(physical_bits, spin_bf);
                if (spin_bf[physical_bits - 1] == 0) { continue; }
                calc_energy(spin_bf, J, energy_bf);
                if (energy_bf < best_energy_bf) {
                    best_energy_bf = energy_bf;
                    for (int i = 0; i < physical_bits; i++) {
                        best_spin_bf[i] = spin_bf[i];
                    }
                } else if (energy_bf == best_energy_bf) {
                    if (checkSBMSolution(spin_bf, exch_logged_rates)) {
                        for (int i = 0; i < physical_bits; i++) {
                            best_spin_bf[i] = spin_bf[i];
                        }
                    }
                }
            }
            if (best_spin_bf[physical_bits - 1] == 0) {
                for (unsigned int i = 0; i < physical_bits - 1; i++) {
                    best_spin_bf[i] = 1 - best_spin_bf[i];
                }
            }
            std::cout << "Best energy: " << best_energy_bf << "\n";
            std::cout << "Best spin  : ";
            print_vec<bool, physical_bits>(best_spin_bf, physical_bits - 1, std::cout);
            checkSBMSolution(best_spin_bf, exch_logged_rates);

            // A solution we found for the testbench
            bool spin_theo[physical_bits] = {0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 1};
            calc_energy(spin_theo, J, energy_bf);
            std::cout << "Theo energy: " << energy_bf << "\n";
            std::cout << "Theo spin  : ";
            print_vec<bool, physical_bits>(spin_theo, physical_bits - 1, std::cout);
            checkSBMSolution(spin_theo, exch_logged_rates);
        }
        std::cout << "End of brute-force calculation\n\n";
#endif
        // RUN SBM
#ifndef __SYNTHESIS__
        std::cout << "Start SBM execution\n";
#endif
        dcal_t best_energy = MAXFLOAT;
        int best_step = 0;
        bool best_spin[physical_bits] = {0};
        SBM(J, spin_y, spin_x, steps, dt, c0, best_energy, best_step, best_spin, regSBMExecStatus);

#ifndef __SYNTHESIS__
        calc_energy(best_spin, J, best_energy);
#endif

        // In this problem, if the SBM ancilla spin is -1,
        // we flip all the other spins
        if (best_spin[physical_bits - 1] == 0) {
            ++countAncillaFlip;
            for (unsigned int i = 0; i < physical_bits - 1; i++) {
                best_spin[i] = 1 - best_spin[i];
            }
        }

        regStrategyUnknown = 0;
        // Should assert(physical_bits <= 32);
        // but HLS can't assert
        for (unsigned int i = 0; i < physical_bits; i++) {
            if (best_spin[i]) {
                regStrategyUnknown.invert(i);
            }
        }

#ifndef __SYNTHESIS__
        std::cout << "Final energy: " << best_energy << "\n";
        std::cout << "Final spin  : ";
        print_vec<bool, physical_bits>(best_spin, physical_bits-1, std::cout);

        checkSBMSolution(best_spin, exch_logged_rates);
        std::cout << "End of SBM execution\n\n";
#endif

        // Write orderResponse if there are no empty price fields
        if (exch_logged_rates[physical_bits - 2]) {
            for (unsigned int i = 0; i < physical_bits - 1; i++) {
                if (best_spin[i] > 0) {
                    operation.orderId = ++orderId;
                    operation.timestamp = response.timestamp;
                    operation.opCode = ORDERENTRY_ADD;
                    operation.quantity = 1;  // change to 1
                    operation.symbolIndex = i / 2;
                    if ((i & 1) == 1) {  // direction ask
                        operation.price = response.askPrice.range(31, 0);
                        operation.direction = ORDER_ASK;
                    } else {  // direction bid
                        operation.price = (response.bidPrice.range(31, 0));
                        operation.direction = ORDER_BID;
                    }
                    operationStream.write(operation);
                }
            }
        }
        // if (orderExecute) {
        //     operation.orderId = ++orderId;
        //     operationStream.write(operation);
        // }
    }

    regProcessResponse = countProcessResponse;
    // regStrategyNone = countStrategyNone;
    // regStrategyPeg = countStrategyPeg;
    // regStrategyLimit = countStrategyLimit;
    // regStrategyUnknown = countStrategyUnknown;
    regStrategyNone = regERMInitConstr;
    regStrategyPeg = regSBMExecStatus;
    regStrategyLimit = countAncillaFlip;
    // regStrategyUnknown = 0;

    return;
}

bool PricingEngine::pricingStrategyPeg(ap_uint<8> thresholdEnable,
                                       ap_uint<32> thresholdPosition,
                                       orderBookResponse_t &response,
                                       orderEntryOperation_t &operation) {
#pragma HLS PIPELINE II = 1 style = flp

    ap_uint<8> symbolIndex = 0;
    bool executeOrder = false;

    symbolIndex = response.symbolIndex;

    // TODO: restore valid check when test data updated to trigger top of book
    // update
    // if(cache[symbolIndex].valid)
    {
        if (cache[symbolIndex].bidPrice != response.bidPrice.range(31, 0)) {
            // create an order, current best bid +100
            operation.timestamp = response.timestamp;
            operation.opCode = ORDERENTRY_ADD;
            operation.symbolIndex = symbolIndex;
            operation.quantity = 800;
            operation.price = (response.bidPrice.range(31, 0) + 100);
            operation.direction = ORDER_BID;
            executeOrder = true;
        }
    }

    // cache top of book prices (used as trigger on next delta if change
    // detected)
    cache[symbolIndex].bidPrice = response.bidPrice.range(31, 0);
    cache[symbolIndex].askPrice = response.askPrice.range(31, 0);
    cache[symbolIndex].valid = true;

    return executeOrder;
}

bool PricingEngine::pricingStrategyLimit(ap_uint<8> thresholdEnable,
                                         ap_uint<32> thresholdPosition,
                                         orderBookResponse_t &response,
                                         orderEntryOperation_t &operation) {
#pragma HLS PIPELINE II = 1 style = flp

    ap_uint<8> symbolIndex = 0;
    bool executeOrder = false;

    symbolIndex = response.symbolIndex;

    // TODO: restore valid check when test data updated to trigger top of book
    // update
    // if(cache[symbolIndex].valid)
    {
        if (cache[symbolIndex].bidPrice != response.bidPrice.range(31, 0)) {
            // create an order, current best bid +50
            operation.timestamp = response.timestamp;
            operation.opCode = ORDERENTRY_ADD;
            operation.symbolIndex = symbolIndex;
            operation.quantity = 800;
            operation.price = (response.bidPrice.range(31, 0) + 50);
            operation.direction = ORDER_BID;
            executeOrder = true;
        }
    }

    // cache top of book prices (used as trigger on next delta if change
    // detected)
    cache[symbolIndex].bidPrice = response.bidPrice.range(31, 0);
    cache[symbolIndex].askPrice = response.askPrice.range(31, 0);
    cache[symbolIndex].valid = true;

    return executeOrder;
}

void PricingEngine::operationPush(
    ap_uint<32> &regCaptureControl, ap_uint<32> &regTxOperation,
    ap_uint<1024> &regCaptureBuffer,
    orderEntryOperationStream_t &operationStream,
    orderEntryOperationStreamPack_t &operationStreamPack) {
#pragma HLS PIPELINE II = 1 style = flp

    mmInterface intf;
    orderEntryOperation_t operation;
    orderEntryOperationPack_t operationPack;

    static ap_uint<32> countTxOperation = 0;

    // Use `while` if there are multiple operations
    operationPush_label0:
    while (!operationStream.empty()) {
        operation = operationStream.read();

        intf.orderEntryOperationPack(&operation, &operationPack);
        operationStreamPack.write(operationPack);
        ++countTxOperation;

        // check if host has capture freeze control enabled before updating
        // TODO: filter capture by user supplied symbol
        if (0 == (0x80000000 & regCaptureControl)) {
            regCaptureBuffer = operationPack.data;
        }
    }

    regTxOperation = countTxOperation;

    return;
}

void PricingEngine::eventHandler(ap_uint<32> &regRxEvent,
                                 clockTickGeneratorEventStream_t &eventStream) {
#pragma HLS PIPELINE II = 1 style = flp

    clockTickGeneratorEvent_t tickEvent;

    static ap_uint<32> countRxEvent = 0;

    if (!eventStream.empty()) {
        eventStream.read(tickEvent);
        ++countRxEvent;

        // event notification has been received from programmable clock tick
        // generator, handling currently limited to incrementing a counter,
        // placeholder for user to extend with custom event handling code
    }

    regRxEvent = countRxEvent;

    return;
}

/***********************************************
 *
 *
 * Our Original Code
 * SBM/SQA/ERM
 *
 *
 * *********************************************/

// without local field
void PricingEngine::ERM(int index, float logged_price, float M1, float M2,
                        float J[physical_bits][physical_bits], float exch_logged_rates[physical_bits - 1], bool &regInitConstr) {

#pragma HLS ARRAY_PARTITION dim=1 type=complete variable=J
    // static float exch_logged_rates[physical_bits - 1] = {0};
    static bool init_constraint = false;
    if (!init_constraint) {
        regInitConstr = false;
        INIT_CONSTRAINT:
        for (int k = 0; k < currencies; k++) {
            float v1i_list[physical_bits - 1] = {0};
            bool v2i_list[physical_bits - 1] = {0};
            CHECK_ID:
            for (int i = 0; i < physical_bits - 1; i++) {
#pragma HLS PIPELINE
                // v1 vector's ith element value
                v1i_list[i] =
                    (exch_index2id[i][0] == k) - (exch_index2id[i][1] == k);
                v2i_list[i] = (exch_index2id[i][0] == k);
            }
            // penalty 1 without diagonal part (+1 at j-for loop)
            // penalty 2 has no diagonal part originally
            // physical_bits - 1 for not changing the ancilla bit
            // i is increasing in diagonal direction
            ERM_penalty_ij:
            for (int i = 0; i < physical_bits - 1; i++) {
                ERM_penalty_ij_in:
                for (int j = i + 1; j < physical_bits - 1; j++) {
                    // outer product
                    // also convert to ising model by divided by 4
                    float pen1 = v1i_list[i] * v1i_list[j] * M1 / 4;
                    float pen2 = v2i_list[i] * v2i_list[j] * M2 / 4;
                    float pen1_plus_pen2 = pen1 + pen2;
                    J[i][j] += pen1_plus_pen2;
                    // Transforming to ising model will remove the diagonal part
                    // and form local field h. The forming of local field h is
                    // replaced with ancilla bit which is the sum of the
                    // original row/col of matrix containing diagonal part
                    J[i][physical_bits - 1] += (pen1_plus_pen2);  // /2 /2 = /4
                    J[j][physical_bits - 1] += (pen1_plus_pen2);
                }
            }
            ERM_penalty_ii:
            for (int i = 0; i < physical_bits - 1; i++) {
                float v1i = v1i_list[i];
                // Adding the original diagonal part back to ancilla bit
                // vli is either +1, 0, or -1
                // vli * vli = (vli != 0)
                float v1i_square_pen = (v1i != 0) * M1 / 4;
                J[i][physical_bits - 1] += v1i_square_pen;
            }
            ERM_penalty_symmetric:
            for (int i = 0; i < physical_bits; i++) {
                for (int j = i + 1; j < physical_bits; j++) {
                    J[j][i] = J[i][j];
                }
            }
        }
        init_constraint = true;
        regInitConstr = true;
#ifndef __SYNTHESIS__
        for (int i = 0; i < physical_bits; i++) {
            print_whole_vec<float, physical_bits>(J[i], std::cout);
        }
        print_whole_vec<float, physical_bits - 1>(exch_logged_rates, std::cout);
#endif
    }

    // adding exchange rate to J matrix's ancilla bit and remove old exchange
    // rate
    float replace_new_rate_divide_4 =
        (exch_logged_rates[index] - logged_price) /
        4;  // -(-old rate) + (-net rate)
    J[index][physical_bits - 1] += replace_new_rate_divide_4;
    J[physical_bits - 1][index] += replace_new_rate_divide_4;
    exch_logged_rates[index] = logged_price;
    return;
}

// with local field h
void PricingEngine::ERM(int index, float logged_price, float M1, float M2,
                        float J[physical_bits][physical_bits],
                        float h[physical_bits]) {
// #pragma HLS INTERFACE m_axi port = J offset = slave bundle = gmem1
// #pragma HLS INTERFACE s_axilite port = J
// #pragma HLS INTERFACE s_axilite port = M1
// #pragma HLS INTERFACE s_axilite port = M2
// #pragma HLS INTERFACE s_axilite port = index
// #pragma HLS INTERFACE s_axilite port = logged_price
// #pragma HLS INTERFACE s_axilite port = return
#pragma HLS ARRAY_PARTITION variable = J type = complete
#pragma HLS ARRAY_PARTITION variable = h type = complete

    static float exch_logged_rates[physical_bits] = {0};
    static bool init_constraint = false;
    if (!init_constraint) {
        for (int k = 0; k < currencies; k++) {
#pragma HLS UNROLL
            // penalty 1 without diagnal part (+1 at j-for loop)
            // penalty 2 has no diagnal part originally
            // physical_bits - 1 for not changing the ancilla bit
            // i is increasing in diagnal direction
            for (int i = 0; i < physical_bits - 1; i++) {
                // v1 vector's ith element value
                float v1i =
                    (exch_index2id[i][0] == k) - (exch_index2id[i][1] == k);
                float v2i = (k == exch_index2id[i][0]);
                for (int j = i + 1; j < physical_bits - 1; j++) {
                    // v1 vector's jth element value
                    float v1j =
                        (exch_index2id[j][0] == k) - (exch_index2id[j][1] == k);
                    float v2j = (k == exch_index2id[j][0]);
                    // outer product
                    // also convert to ising model by divided by 4
                    float pen1 = v1i * v1j * M1 / 4;
                    float pen2 = v2i * v2j * M2 / 4;
                    float pen1_plus_pen2 = pen1 + pen2;
                    J[i][j] += pen1_plus_pen2;
                    J[j][i] += pen1_plus_pen2;
                    // Tranforming to ising model will remove the diagnal part
                    // and form local field h. The forming of local field h is
                    // replaced with ancilla bit which is the sum of the
                    // original row/col of matrix containing diagnal part
                    // J[i][physical_bits - 1] += (pen1_plus_pen2);  // /2 /2 =
                    // /4 J[j][physical_bits - 1] += (pen1_plus_pen2);
                    // J[physical_bits - 1][i] += (pen1_plus_pen2);
                    // J[physical_bits - 1][j] += (pen1_plus_pen2);
                    h[i] += (pen1_plus_pen2)*2;
                    h[j] += (pen1_plus_pen2)*2;
                }
                // Adding the original diagnal part back to ancilla bit
                float v1i_square_pen = v1i * v1i * M1 / 2;
                // J[physical_bits - 1][i] += v1i_square_pen;
                // J[i][physical_bits - 1] += v1i_square_pen;
                h[i] += v1i_square_pen;
            }
        }
        init_constraint = true;
    }

    // adding exchange rate to J matrix's ancilla bit and remove old exchange
    // rate
    float replace_new_rate_divide_2 =
        (exch_logged_rates[index] - logged_price) /
        2;  // -(-old rate) + (-net rate)
    // J[index][physical_bits - 1] += replace_new_rate_divide_4;
    // J[physical_bits - 1][index] += replace_new_rate_divide_4;
    h[index] += replace_new_rate_divide_2;
    exch_logged_rates[index] = logged_price;
    return;
}

#ifndef __SYNTHESIS__
template<class T>
void PricingEngine::checkSBMCoeff(T J[physical_bits][physical_bits], float c0) {
    std::cout << "Coefficient check\n";
    float c0_debug = 0;
    for (int i = 0; i < physical_bits; i++) {
        for (int j = 0; j < physical_bits; j++) {
            c0_debug += J[i][j] * J[i][j];
        }
    }
    c0_debug = 0.5 * sqrt(physical_bits / c0_debug);
    std::cout << "c0_theoretical: " << c0_debug <<"\n";
    std::cout << "c0_in_use     : " << c0 << "\n";
    std::cout << "End of coefficient check\n\n";
}
#endif

bool PricingEngine::checkExchCycle(bool spin[physical_bits]) {
    // Check for exchange rate cycle
    int lhs[currencies] = {0};
    int rhs[currencies] = {0};
    for (int i = 0; i < physical_bits - 1; i++) {
        if (spin[i] == 1) {
            lhs[exch_index2id[i][0]] += 1;
            rhs[exch_index2id[i][1]] += 1;
        }
    }
    bool hasCycle = 1;
    for (int i = 0; i < currencies; i++) {
        if (lhs[i] != rhs[i]) {
            hasCycle = 0;
            break;
        }
    }
    return hasCycle;
}

bool PricingEngine::checkProfitable(bool spin[physical_bits], float exch_logged_rates[physical_bits - 1]) {
    float logged_rate = 0;
    for (int i = 0; i < physical_bits - 1; i++) {
        if (spin[i] == 1) {
            logged_rate += exch_logged_rates[i];
        }
    }
    return (logged_rate > 0);
}

#ifndef __SYNTHESIS__
bool PricingEngine::checkSBMSolution(bool spin[physical_bits], float exch_logged_prices[physical_bits - 1]) {
    bool hasCycle = checkExchCycle(spin);
    if (hasCycle) {
        std::cout << "Cycle check passed!\n";
        if (checkProfitable(spin, exch_logged_prices)) {
            std::cout << "The solution is profitable!\n";
            return true;
        } else {
            std::cout << "The solution is not profitable!\n";
        }
    } else {
        std::cout << "Cycle check failed!\n";
    }
    return false;
}
#endif

template <int size>
void reduction_dot_buffer(float tmp[physical_bits]) {
    reduction_dot_buffer<size / 2>(tmp);
REDUCED_DOT:
    for (int i = 0; i < physical_bits; i += size) {
        tmp[i] += tmp[i + size / 2];
    }
}

template <>
void reduction_dot_buffer<1>(float tmp[physical_bits]) {
    ;
}

float flip_bit_if(float mi, dcal_t vi) {
    to_uint tmp = {0};
    tmp.f = mi;
    if (vi < 0) {
        ap_uint<32> tmpu = tmp.u;
        // flip the sign bit
        tmpu.invert(31);
        tmp.u = tmpu;
    }
    return tmp.f;
}

float flip_bit_if(float mi, bool vi) {
    to_uint tmp = {0};
    tmp.f = mi;
    if (vi == 0) {  // For stream version: < is changed to ==
        ap_uint<32> tmpu = tmp.u;
        // flip the sign bit
        tmpu.invert(31);
        tmp.u = tmpu;
    }
    return tmp.f;
}

constexpr int int_log_ceil(int a) {
    int b = 0;
    int set_cnt = 0;
    for (int i = 0; i < 32; i++) {
        // find the msb position
        if (((a >> i) & 1) == 1) {
            b = i;
            set_cnt++;
        }
    }
    return (set_cnt > 1) ? b + 1 : b;
}

void reduction_dot(float matrix[physical_bits][physical_bits],
                   bool vector[physical_bits], int row, dcal_t& res) {
    // need to be power of 2 in order to perform adder tree balance
    constexpr int buffer_size = 1 << int_log_ceil(physical_bits);
    dcal_t tmp[buffer_size] = {0};
SET_TMP:
    for (int i = 0; i < physical_bits; i++) {
        tmp[i] = (dcal_t)flip_bit_if(matrix[row][i], vector[i]);
    }
    reduction_dot_buffer<buffer_size>(tmp);
    res = tmp[0];
    return;
}

template <int size>
void naive_dot(float matrix[size][size], dcal_t vector[size], int row,
               dcal_t& res) {
    res = 0;
NAIVE_DOT:
    for (int j = 0; j < size; j++) {
        // dsb times the sign of vector
        res += (dcal_t)matrix[row][j] * (vector[j] > 0 ? 1 : -1);
    }
}

void load_Q(float Q[physical_bits][physical_bits], spinStream& sstream) {
// TODO: Better implementation
// Open an new array
// Stream part of the array when needed
LOAD_Q_MAIN:
    for (int i = 0; i < physical_bits; ++i) {
        for (int j = 0; j < physical_bits; ++j) {
            sstream << Q[i][j];
        }
    }
}

void load_spin(dcal_t spin[physical_bits], spinStream& sstream) {
LOAD_SPIN_MAIN:
    for (int i = 0; i < physical_bits; ++i) {
        sstream << spin[i];
    }
}

void load_x_y_stream(dcal_t x_cache_in[physical_bits],
                     dcal_t y_cache_in[physical_bits], spinStream& x_stream1,
                     spinStream& y_stream1) {
LOAD_X_Y_STRM:
    for (int i = 0; i < physical_bits; ++i) {
        x_stream1 << x_cache_in[i];
        y_stream1 << y_cache_in[i];
    }
}

void spin_to_bool(spinStream& sstream, boolStream& bstream) {
SPIN_TO_BOOL_MAIN:
    for (int i = 0; i < physical_bits; ++i) {
        bstream << ((sstream.read()) > 0);
    }
}

void output_bits(boolStream& bstream, bool bits[physical_bits]) {
OUTPUT_BITS_MAIN:
    for (int i = 0; i < physical_bits; ++i) {
#pragma HLS PIPELINE
        bits[i] = bstream.read();
    }
}

void update_x(dcal_t x_in[physical_bits], dcal_t x_out[physical_bits], dcal_t y[physical_bits], float dt) {
UPDATE_X_MAIN:
    for (int i = 0; i < physical_bits; i++) {
        x_out[i] = x_in[i] + y[i] * (dcal_t)dt;
    }
}

void update_x_stream(spinStream& x_in, spinStream& x_out, spinStream& y_in,
                     spinStream& y_out, boolStream& x_b_out, float dt) {
UPDATE_X_STRM_MAIN:
    for (int i = 0; i < physical_bits; i++) {
        dcal_t y = y_in.read();
        dcal_t x = x_in.read() + y * (dcal_t)dt;
        // reset_x_y here?
        x_out << x;
        x_b_out << (x > 0);
        y_out << y;
    }
}

void update_y(dcal_t x[physical_bits], bool x_bool[physical_bits],
              dcal_t y_in[physical_bits], dcal_t y_out[physical_bits],
              float Q_Matrix[physical_bits][physical_bits], float c1,
              float c2) {
    // c1 = c0 * dt
    // c2 = (1-a) * dt = (steps - i) * (1.0 / steps) * dt
    // TODO: RESOURCE pragma
    dcal_t Q_dot_sign_x[physical_bits];
UPDATE_Y_MAIN:
    for (int i = 0; i < physical_bits; i++) {
        reduction_dot(Q_Matrix, x_bool, i, Q_dot_sign_x[i]);
        y_out[i] = y_in[i] - (c2 * x[i]) - Q_dot_sign_x[i] * c1;
    }
}

void dot_stream(float Q_matrix[physical_bits][physical_bits],
                boolStream& x_b_in, spinStream& Q_dot_sign_x_stream) {
    // need to be power of 2 in order to perform adder tree balance
    constexpr int buffer_size = 1 << int_log_ceil(physical_bits);
    dcal_t tmp[buffer_size] = {0};
    bool spin[physical_bits] = {0};
    output_bits(x_b_in, spin);
DOT_STREAM_MAIN:
    for (int row = 0; row < physical_bits; row++) {
    SET_TMP_STRM:
        for (int i = 0; i < physical_bits; i++) {
            tmp[i] = (dcal_t)flip_bit_if(Q_matrix[row][i], spin[i]);
        }
        reduction_dot_buffer<buffer_size>(tmp);
        Q_dot_sign_x_stream << tmp[0];
    }
}

void update_y_stream(spinStream& x_in, spinStream& x_out, spinStream& y_in,
                     spinStream& y_out, boolStream& x_b_in,
                     float Q_Matrix[physical_bits][physical_bits], float c1,
                     float c2) {
    // c1 = c0 * dt
    // c2 = (1-a) * dt = (steps - i) * (1.0 / steps) * dt
    spinStream Q_dot_sign_x_stream;
    dot_stream(Q_Matrix, x_b_in, Q_dot_sign_x_stream);
UPDATE_Y_STRM_MAIN:
    for (int i = 0; i < physical_bits; i++) {
        dcal_t x = x_in.read();
        y_out << y_in.read() - (c2 * x) - Q_dot_sign_x_stream.read() * c1;
        x_out << x;
    }
}

void update_y_and_E(dcal_t x[physical_bits], dcal_t y[physical_bits],
                    float Q_Matrix[physical_bits][physical_bits], float c0,
                    float a, float dt, dcal_t& Q_energy) {
    dcal_t Q_dot_sign_x = 0;
    Q_energy = 0;
    dcal_t delta_a = (dcal_t)(1 - a);
    dcal_t da_x = 0;
    dcal_t c0_Q_dot_sign_x = 0;
UPDATE_Y_and_E_MAIN:
    for (int i = 0; i < physical_bits; i++) {
        // naive_dot<physical_bits>(Q_Matrix, x, i, Q_dot_sign_x);
        //reduction_dot(Q_Matrix, x, i, Q_dot_sign_x);
        da_x = delta_a * x[i];
        c0_Q_dot_sign_x = (dcal_t)c0 * Q_dot_sign_x;
        y[i] -= (da_x + 2 * c0_Q_dot_sign_x) * (dcal_t)dt;
        // Reuse the Q_dot_sign_x to calculate energy
        Q_energy += Q_dot_sign_x * (x[i] > 0 ? 1 : -1);
    }
}

// make sure x and y is in the boundary

void reset_x_y(dcal_t x[physical_bits], dcal_t y[physical_bits]) {
RESET_X_and_Y_MAIN:
    for (int i = 0; i < physical_bits; i++) {
        if (x[i] > 1) {
            x[i] = 1;
            y[i] = 0;
        } else if (x[i] < -1) {
            x[i] = -1;
            y[i] = 0;
        }
    }
}

void reset_x_y_stream(spinStream& x_stream_in, spinStream& x_stream_out,
                      spinStream& y_stream_in, spinStream& y_stream_out) {
RESET_X_and_Y_STRM_MAIN:
    for (int i = 0; i < physical_bits; i++) {
        dcal_t x = x_stream_in.read();
        dcal_t y = y_stream_in.read();
        if (x > 1) {
            x = 1;
            y = 0;
        } else if (x < -1) {
            x = -1;
            y = 0;
        }
        x_stream_out << x;
        y_stream_out << y;
    }
}

void set_spin(dcal_t vec[physical_bits], bool vec2[physical_bits]) {
SET_SPIN_MAIN:
    for (int i = 0; i < physical_bits; i++) {
        vec2[i] = (vec[i] > 0);  // ? 1 : -1 later by software
    }
}

void set_spin_stream(spinStream& sstream, bool vec2[physical_bits]) {
SET_SPIN_STRM_MAIN:
    for (int i = 0; i < physical_bits; i++) {
        vec2[i] = (sstream.read() > 0);
    }
}

void SBM_update(dcal_t Q_matrix_cache[physical_bits][physical_bits],
                dcal_t x_in[physical_bits], dcal_t y_in[physical_bits],
                dcal_t x_out[physical_bits],dcal_t y_out[physical_bits],
                bool x_out_bool[physical_bits], float c1,
                float c2, float dt) {
#pragma HLS DATAFLOW
    // c1 = c0 * dt
    // c2 = (1-a) * dt = (steps - i) * (1.0 / steps) * dt
    update_x(x_in, x_out, y_in, dt);
    set_spin(x_out, x_out_bool);
//#define __CALC_ENERGY__
#ifdef __CALC_ENERGY__
    update_y_and_E(x, y, Q_Matrix, c0, a, dt, energy);
    if (energy < best_energy) {
#ifndef __SYNTHESIS__
        print_update_msg(best_energy, energy, best_step, i, best_spin,
                         std::cout);
#endif
        best_energy = energy;
        best_step = i;
        set_spin(x, best_spin);
#ifndef __SYNTHESIS__
        print_whole_vec<bool, physical_bits>(best_spin, std::cout);
#endif
    }
#else
    // update_y(x, y, Q_Matrix, c0, a, dt);
    update_y(x_out, x_out_bool, y_in, y_out,
                    Q_matrix_cache, c1, c2);
#endif
    reset_x_y(x_out, y_out);
    // Put the updated data back to x_stream1 and y_stream1
    //reset_x_y_stream(x_stream3, x_stream_out, y_stream3, y_stream_out);
}

void PricingEngine::SBM(float Q_Matrix[physical_bits][physical_bits], dcal_t y[physical_bits],
         dcal_t x[physical_bits], int steps, float dt, float c0, dcal_t& best_energy, int& best_step,
         bool best_spin[physical_bits], ap_uint<32> &regSBMExecStatus) {
#ifndef __SYNTHESIS__
    // Init debug file
    std::fstream f("out.txt", std::ios::out);
#endif
// #pragma HLS ARRAY_PARTITION variable = x type = complete dim = 1
// #pragma HLS ARRAY_PARTITION variable = y type = complete dim = 1
// #pragma HLS ARRAY_PARTITION variable = Q_Matrix type = complete dim=2
    // TODO: SBMStatus enum
    regSBMExecStatus = 0; // SBM start
    dcal_t energy = 0;
    // float a = 0;
    float dat = dt / steps;  // a0 = 1.0 // dat = da * dt
    float c1 = 2 * c0 * dt;

    float x_updated[physical_bits] = {0};
    float y_updated[physical_bits] = {0};
    bool x_updated_bool[physical_bits] = {0};
    //load_x_y_stream(x_cache_in, y_cache_in, x_stream_in, y_stream_in);
SBM_MAIN:
    for (int i = 0; i < steps; i++) {
#pragma HLS LOOP_TRIPCOUNT min = 100 max = 2000
#ifndef __SYNTHESIS__
        calc_energy<dcal_t, physical_bits>(x, Q_Matrix, energy);
#ifndef __DEBUG__
        f << "step: " << i << std::endl;
        f << "x:" << std::endl;
        //print_whole_vec<dcal_t, physical_bits>(x, std::cout);
        print_whole_vec<dcal_t, physical_bits>(x, f);
        f << "y:" << std::endl;
        print_whole_vec<dcal_t, physical_bits>(y, f);
        f << "energy: " << energy << "\n";
#endif
#endif
        float c2 = (steps - i) * dat;
        SBM_update(Q_Matrix, x, y, x_updated, y_updated, x_updated_bool, c1, c2, dt);
    RETURN_X_Y:
        for (int i = 0; i < physical_bits; ++i) {
#pragma HLS PIPELINE
            x[i] = x_updated[i];
            y[i] = y_updated[i];
        }
    }
    regSBMExecStatus = 1; // SBM done
#ifndef __CALC_ENERGY__
    // set_spin(x_cache_in, best_spin);
    // set_spin_stream(x_stream1, best_spin);
    // TODO: Dataflow for best_spin
    // TODO: Pack spins to integers to burst write
    //spin_to_bool(x_stream_in, x_bstream);
    //output_bits(x_bstream, best_spin);
    for (int i = 0; i < physical_bits; ++i) {
        best_spin[i] = x_updated_bool[i];
    }
    regSBMExecStatus = 2; // SBM idle
#endif
}
