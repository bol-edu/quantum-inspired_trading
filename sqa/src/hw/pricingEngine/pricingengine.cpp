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

/**
 * Some Helper Functions
 */

void convertFloat2Byte(ap_uint<32> &dst, float src)
{
#pragma HLS INLINE
    union {
        unsigned int uint_data;
        fp_t fp_data;
    } tmp;
    tmp.fp_data = src;
    dst = tmp.uint_data;
}

void convertByte2Float(float &dst, ap_uint<32> src)
{
#pragma HLS INLINE
    union {
        unsigned int uint_data;
        fp_t fp_data;
    } tmp;
    tmp.uint_data = src.to_uint();
    dst = tmp.fp_data;
}

#if !__SYNTHESIS__
bool PricingEngine::checkExchCycle(spin_t spin[NUM_SPIN])
{
    // Check for exchange rate cycle
    int lhs[NUM_CURRENCIES] = {0};
    int rhs[NUM_CURRENCIES] = {0};
    for (int i = 0; i < PHYSICAL_BITS; i++) {
        if (spin[i] == 1) {
            lhs[exch_index2id[i][0]] += 1;
            rhs[exch_index2id[i][1]] += 1;
        }
    }

    bool hasCycle = 1;
    for (int i = 0; i < NUM_CURRENCIES; i++) {
        if (lhs[i] != rhs[i]) {
            hasCycle = 0;
            break;
        }
    }

    return hasCycle;
}

bool PricingEngine::checkProfitable(spin_t spin[NUM_SPIN])
{
    float logged_rate = 0;
    for (int i = 0; i < PHYSICAL_BITS; i++) {
        if (spin[i] == 1) {
            logged_rate += exch_logged_rates[i];
        }
    }
    return (logged_rate > 0);
}

bool PricingEngine::checkIsAllZero(spin_t spin[NUM_SPIN])
{
    for (int i = 0; i < PHYSICAL_BITS; i++) {
        if (spin[i] == 1) {
            return false;
        }
    }
    return true;
}

void PricingEngine::checkSolution(spin_t spin[NUM_SPIN])
{
    if (checkIsAllZero(spin)) {
        std::cout << "[AllZero!]";
    } else {
        std::cout << "[NotAllZero!]";
    }

    if (checkExchCycle(spin)) {
        std::cout << "[FindCycle!]";
    } else {
        std::cout << "[NoCycle!]";
    }

    if (checkProfitable(spin)) {
        std::cout << "[Profitable!]";
    } else {
        std::cout << "[NotProfitable!]";
    }

    std::cout << std::endl;
    std::cout << std::endl;
}
#endif

/**
 * PricingEngine Core
 */

void PricingEngine::responsePull(ap_uint<32> &regRxResponse,
                                 orderBookResponseStreamPack_t &responseStreamPack,
                                 orderBookResponseStream_t &responseStream)
{
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

void PricingEngine::pricingProcess(ap_uint<32> &regStrategyControl, ap_uint<32> &regProcessResponse,
                                   ap_uint<32> &regStrategyNone, ap_uint<32> &regStrategyPeg,
                                   ap_uint<32> &regStrategyLimit, ap_uint<32> &regStrategyUnknown,
                                   pricingEngineRegStatus_t &regStatus,
                                   pricingEngineRegControl_t &regControl,
                                   pricingEngineRegStrategy_t *regStrategies,
                                   orderBookResponseStream_t &responseStream,
                                   orderEntryOperationStream_t &operationStream)
{
    // #pragma HLS PIPELINE II = 1 style = flp

    mmInterface intf;
    orderBookResponse_t response;
    orderEntryOperation_t operation;
    ap_uint<8> symbolIndex = 0;
    ap_uint<8> strategySelect = 0;
    ap_uint<8> thresholdEnable = 0;
    ap_uint<8> thresholdPosition = 0;
    bool orderExecute = false;

    static ap_uint<32> orderId = 0;
    static ap_uint<32> countProcessResponse = 0;
    static ap_uint<32> countStrategyNone = 0;
    static ap_uint<32> countStrategyPeg = 0;
    static ap_uint<32> countStrategyLimit = 0;
    static ap_uint<32> countStrategyUnknown = 0;

    // Start of QUBO formulation parameters
    const float M1 = 10;  // 50;
    const float M2 = 10;  // 25;

    // For SQA ONLY
    static fp_t J[NUM_SPIN][NUM_SPIN] = {0};
    static fp_t h[NUM_SPIN] = {0};
    static spin_t spins[NUM_SPIN];
#pragma HLS ARRAY_PARTITION dim = 1 type = cyclic factor = 4 variable = J
#pragma HLS ARRAY_RESHAPE dim = 2 type = complete variable = J
#pragma HLS ARRAY_PARTITION dim = 1 type = complete variable = h
#pragma HLS ARRAY_PARTITION dim = 1 type = complete variable = spins

    // Set Default Argument
    if (regControl.reserved04 == 0) convertFloat2Byte(regControl.reserved04, 5.0f);   // Gamma
    if (regControl.reserved05 == 0) convertFloat2Byte(regControl.reserved05, 0.05f);  // T

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

        // NOTE: input data originally is uint and we static_cast it to float
        unsigned int bidprice = response.bidPrice.range(31, 0);
        unsigned int askprice = response.askPrice.range(31, 0);
        int exch_id = ((int)response.symbolIndex) * 2;
        runERM(exch_id, log(reinterpret_cast<float &>(bidprice)), M1, M2, J, h);
        runERM(exch_id + 1, log(reinterpret_cast<float &>(askprice)), M1, M2, J, h);

        // Make sure there is no empty price fields
        if (this->exch_logged_rates[PHYSICAL_BITS - 1] != 0) {
            // RUN SQA
            runSQA(spins, J, h, regStatus, regControl);

#if !__SYNTHESIS__ && CHECK_SOLUTION
            // Check Profitable or Not
            checkSolution(spins);
#endif

            // Write out Operations based on SQA result
            for (unsigned int i = 0; i < PHYSICAL_BITS; i++) {
                if (spins[i]) {
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

            // if (orderExecute) {
            //     operation.orderId = ++orderId;
            //     operationStream.write(operation);
            // }
        }
    }

    regProcessResponse = countProcessResponse;
    regStrategyNone = countStrategyNone;
    regStrategyPeg = countStrategyPeg;
    regStrategyLimit = countStrategyLimit;
    regStrategyUnknown = countStrategyUnknown;

    return;
}

bool PricingEngine::pricingStrategyPeg(ap_uint<8> thresholdEnable, ap_uint<32> thresholdPosition,
                                       orderBookResponse_t &response,
                                       orderEntryOperation_t &operation)
{
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

bool PricingEngine::pricingStrategyLimit(ap_uint<8> thresholdEnable, ap_uint<32> thresholdPosition,
                                         orderBookResponse_t &response,
                                         orderEntryOperation_t &operation)
{
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

void PricingEngine::operationPush(ap_uint<32> &regCaptureControl, ap_uint<32> &regTxOperation,
                                  ap_uint<1024> &regCaptureBuffer,
                                  orderEntryOperationStream_t &operationStream,
                                  orderEntryOperationStreamPack_t &operationStreamPack)
{
#pragma HLS PIPELINE II = 1 style = flp

    mmInterface intf;
    orderEntryOperation_t operation;
    orderEntryOperationPack_t operationPack;

    static ap_uint<32> countTxOperation = 0;

    // if (!operationStream.empty()) {
    // change this since there may be multiple operations
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
                                 clockTickGeneratorEventStream_t &eventStream)
{
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
 *Our Original Code
 *SBM/SQA/ERM
 *
 *
 * *********************************************/

// with local field h
void PricingEngine::runERM(int index, float logged_price, float M1, float M2,
                           float J[NUM_SPIN][NUM_SPIN], float h[NUM_SPIN])
{
    if (!init_constraint) {
        for (int k = 0; k < NUM_CURRENCIES; k++) {
            // penalty 1 without diagnal part (+1 at j-for loop)
            // penalty 2 has no diagnal part originally
            // PHYSICAL_BITS - 1 for not changing the ancilla bit
            // i is increasing in diagnal direction
            for (int i = 0; i < PHYSICAL_BITS; i++) {
#pragma HLS PIPELINE
                // v1 vector's ith element value
                float v1i = (exch_index2id[i][0] == k) - (exch_index2id[i][1] == k);
                float v2i = (k == exch_index2id[i][0]);

                for (int j = i + 1; j < PHYSICAL_BITS; j++) {
                    // v1 vector's jth element value
                    float v1j = (exch_index2id[j][0] == k) - (exch_index2id[j][1] == k);
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
                    // J[i][PHYSICAL_BITS - 1] += (pen1_plus_pen2);  // /2 /2 =
                    // /4 J[j][PHYSICAL_BITS - 1] += (pen1_plus_pen2);
                    // J[PHYSICAL_BITS - 1][i] += (pen1_plus_pen2);
                    // J[PHYSICAL_BITS - 1][j] += (pen1_plus_pen2);
                    h[i] += (pen1_plus_pen2)*2;
                    h[j] += (pen1_plus_pen2)*2;
                }
                // Adding the original diagnal part back to ancilla bit
                float v1i_square_pen = v1i * v1i * M1 / 2;
                // J[PHYSICAL_BITS - 1][i] += v1i_square_pen;
                // J[i][PHYSICAL_BITS - 1] += v1i_square_pen;
                h[i] += v1i_square_pen;
            }
        }
        init_constraint = true;
    }

    // adding exchange rate to J matrix's ancilla bit and remove old exchange rate
    // -(-old rate) + (-net rate)
    h[index] += (exch_logged_rates[index] - logged_price) / 2;
    exch_logged_rates[index] = logged_price;
}

/*
 * Following are SQA Related Code
 */

/*
 .d88888b.  888b     d888  .d8888b.
d88P" "Y88b 8888b   d8888 d88P  Y88b
888     888 88888b.d88888 888    888
888     888 888Y88888P888 888
888     888 888 Y888P 888 888
888 Y8b 888 888  Y8P  888 888    888
Y88b.Y8b88P 888   "   888 Y88b  d88P
 "Y888888"  888       888  "Y8888P"
       Y8b
*/

/*
 * Run Multiple Runs of QMC
 * Return spins of first trotter
 */
void PricingEngine::runSQA(spin_t spins[NUM_SPIN], float J[NUM_SPIN][NUM_SPIN], float h[NUM_SPIN],
                           pricingEngineRegStatus_t &regStatus,
                           pricingEngineRegControl_t &regControl)
{
    // Internal Trotters
    static spin_t trotters[NUM_TROT][NUM_SPIN];
    static short run_count = 0;
#pragma HLS ARRAY_PARTITION dim = 1 type = complete variable = trotters
#pragma HLS ARRAY_PARTITION dim = 2 type = complete variable = trotters

    // Reset to All 1
    for (int m = 0; m < NUM_TROT; m++) {
        for (int s = 0; s < PHYSICAL_BITS; s++) {
#pragma HLS UNROLL
            trotters[m][s] = 1;
        }
    }

    // Iteration Parameters
    const int iter = 10;  // default 500
    // const int iter = 25;  // default 500
    fp_t gamma_start, T, beta;
    convertByte2Float(gamma_start, regControl.reserved04);
    convertByte2Float(T, regControl.reserved05);
    beta = 1.0f / T;

#if !__SYNTHESIS__ && DEBUG
    std::cout << std::endl;
    std::cout << "gamma_start   = " << gamma_start << std::endl;
    std::cout << "T             = " << T << std::endl;
    std::cout << "beta          = " << beta << std::endl;
#endif

    // Iteration
    for (int i = 0; i < iter; i++) {
#pragma HLS PIPELINE off

        // Get Jperp
        fp_t gamma = gamma_start;
        fp_t Jperp = -0.5 * T * log(tanh(gamma / (fp_t)NUM_TROT / T));
        gamma_start *= 0.25;  // Use geometric instead of Arithmatic
        // gamma_start *= 0.57435;  // Use geometric instead of Arithmatic

        // Run QMC
        this->runQMC(trotters, J, h, Jperp, beta);
    }

#if !__SYNTHESIS__ && DEBUG
    std::cout << "final gamma  = " << gamma_start << std::endl;
    std::cout << std::endl;
#endif

#if !__SYNTHESIS__ && DEBUG
    std::cout << "Final:" << std::endl;
    for (int m = 0; m < NUM_TROT; m++) {
        for (int s = 0; s < PHYSICAL_BITS; s++) {
            std::cout << trotters[m][s];
        }
        std::cout << std::endl;
    }
    std::cout << std::endl;
#endif

    // Write back the first trotter
    for (int i = 0; i < PHYSICAL_BITS; i++) {
        spins[i] = trotters[1][i];
    }

    // Debug Info
    for (int i = 0; i < PHYSICAL_BITS; i++) {
        regStatus.reserved10[i] = trotters[0][i];
        regStatus.reserved11[i] = trotters[1][i];
        regStatus.reserved12[i] = trotters[2][i];
        regStatus.reserved13[i] = trotters[3][i];
    }

    // More Debug Info
    run_count++;
    regStatus.reserved14 = run_count;
    regStatus.reserved15 = 0xdeadbeef;
}

/*
 * Negate
 * - Negate the sign of single-precision float-point
 * - To reduce the usage of LUT (replace the xor)
 */
inline float Negate(float input)
{
#pragma HLS INLINE
    union {
        float fp_data;
        uint32_t int_data;
    } converter;

    converter.fp_data = input;

    ap_uint<32> tmp = converter.int_data;
    tmp[31] = (~tmp[31]);

    converter.int_data = tmp;

    return converter.fp_data;
}

/*
 * Multiply
 * - Spin (boolean) times Jcoup
 */
inline fp_t Multiply(spin_t spin, fp_t jcoup)
{
#pragma HLS INLINE
    return ((!spin) ? (Negate(jcoup)) : (jcoup));
}

/*
 * ReduceIntra (TOP)(BUF_SIZE = NUM_SPIN)
 * - Recursion using template meta programming
 * - Reduce Intra-Buffer
 */
template <u32_t BUF_SIZE, u32_t GAP_SIZE>
inline void ReduceIntra(fp_t fp_buffer[BUF_SIZE])
{
#pragma HLS INLINE
    // Next call
    ReduceIntra<BUF_SIZE, GAP_SIZE / 2>(fp_buffer);

    // Reduce Intra
REDUCE_INTRA:
    for (u32_t i = 0; i < BUF_SIZE; i += GAP_SIZE) {
#pragma HLS UNROLL
        fp_buffer[i] += fp_buffer[i + GAP_SIZE / 2];
    }
}

/*
 * ReduceIntra (BOTTOM)(BUF_SIZE = NUM_SPIN)
 */
template <>
inline void ReduceIntra<NUM_SPIN, 1>(fp_t fp_buffer[NUM_SPIN])
{
    ;
}

/*
 * Trotter Unit
 * - UpdateOfTrotters      : Sum up spin[j] * Jcoup[i][j]
 * - UpdateOfTrottersFinal : Add other terms and do the flip
 */
fp_t UpdateOfTrotters(const spin_t trotters_local[NUM_SPIN], const fp_t jcoup_local[NUM_SPIN])
{
    // Pramgas: Pipeline and Confine the usage of fadd
    CTX_PRAGMA(HLS ALLOCATION operation instances = fadd limit = NUM_FADD)
    CTX_PRAGMA(HLS PIPELINE)

    // Buffer for source of adder
    fp_t fp_buffer[NUM_SPIN];

FILL_BUFFER:
    for (u32_t spin_ofst = 0; spin_ofst < NUM_SPIN; spin_ofst++) {
        // Multiply
        fp_buffer[spin_ofst] = Multiply(trotters_local[spin_ofst], jcoup_local[spin_ofst]);
    }

    // Reduce inside each fp_buffer
    ReduceIntra<NUM_SPIN, NUM_SPIN>(fp_buffer);

    // Write into de_tmp buffer
    return fp_buffer[0];
}

void UpdateOfTrottersFinal(const u32_t stage, const info_t info, const state_t state, const fp_t de,
                           spin_t trotters_local[NUM_SPIN])
{
#pragma HLS INLINE off

    bool inside = (stage >= info.m && stage < NUM_SPIN + info.m);
    if (inside) {
        // Cache
        fp_t de_tmp = de;
        spin_t this_spin = trotters_local[state.i_spin];

        // Add de_qefct
        bool same_dir = (state.up_spin == state.down_spin);
        if (same_dir) {
            de_tmp += (state.up_spin) ? info.neg_de_qefct : info.de_qefct;
        }

        // Times 2.0f then Add h_local
        de_tmp *= 2.0f;
        de_tmp += state.h_local;

        /*
         * Formula: - (-2) * spin(i) * deTmp > lrn / beta
         * EqualTo:          spin(i) * deTmp > lrn / Beta / 2
         */
        // Times this_spin
        if (!this_spin) {
            de_tmp = Negate(de_tmp);
        }

        // Flip and Return
        if ((de_tmp) > state.log_rand_local / info.beta * 0.5f) {
            trotters_local[state.i_spin] = (~this_spin);
        }
    }
}

/*
 * QMC
 */
void PricingEngine::runQMC(spin_t trotters[NUM_TROT][NUM_SPIN], fp_t jcoup[NUM_SPIN][NUM_SPIN],
                           fp_t h[NUM_SPIN], fp_t jperp, fp_t beta)
{
    // Force pipeline off
#pragma HLS INLINE off
#pragma HLS PIPELINE off

    // input state and de and fix info of trotter units
    state_t state[NUM_TROT];
    fp_t de[NUM_TROT];
    info_t info[NUM_TROT];
#pragma HLS ARRAY_PARTITION dim = 1 type = complete variable = state
#pragma HLS ARRAY_PARTITION dim = 1 type = complete variable = de
#pragma HLS ARRAY_PARTITION dim = 1 type = complete variable = info

    // Local jcoup
    fp_t jcoup_local[NUM_TROT][NUM_SPIN];
#pragma HLS ARRAY_PARTITION dim = 1 type = complete variable = jcoup_local
#pragma HLS ARRAY_RESHAPE dim = 2 type = complete variable = jcoup_local

    // qefct-Related Energy
    const fp_t de_qefct = jperp * ((fp_t)NUM_TROT);
    const fp_t neg_de_qefct = Negate(de_qefct);

    // Initialize infos
INIT_INFO:
    for (u32_t m = 0; m < NUM_TROT; m++) {
#pragma HLS UNROLL
        info[m].m = m;
        info[m].beta = beta;
        info[m].de_qefct = de_qefct;
        info[m].neg_de_qefct = neg_de_qefct;
        info[m].seed = m + 1;
    }

    // Prefetch jcoup, h, and log_rand
    fp_t jcoup_prefetch[NUM_SPIN];
    fp_t h_prefetch[NUM_TROT];
    fp_t log_rand_prefetch[NUM_TROT];
#pragma HLS ARRAY_RESHAPE dim = 1 type = complete variable = jcoup_prefetch
#pragma HLS ARRAY_PARTITION dim = 1 type = complete variable = h_prefetch
#pragma HLS ARRAY_PARTITION dim = 1 type = complete variable = log_rand_prefetch

    // Prefetch Jcoup before the loop of stages
PREFETCH_JCOUP:
    for (u32_t ofst = 0; ofst < NUM_SPIN; ofst++) {
#pragma HLS UNROLL
        jcoup_prefetch[ofst] = jcoup[0][ofst];
    }

    // Prefetch h and lr
    h_prefetch[0] = h[0];
    log_rand_prefetch[0] = this->generateRandomNumber(info[0].seed);

    // Loop of stage
LOOP_STAGE:
    for (u32_t stage = 0; stage < (NUM_SPIN + NUM_TROT - 1); stage++) {
        // Force pipeline off
#pragma HLS PIPELINE

        // Update offset, h_local, log_rand_local
    UPDATE_INPUT_STATE:
        for (u32_t m = 0; m < NUM_TROT; m++) {
#pragma HLS UNROLL
            u32_t ofst = ((stage + NUM_SPIN - m) & (NUM_SPIN - 1));
            u32_t up = (m == 0) ? (NUM_TROT - 1) : (m - 1);
            u32_t down = (m == NUM_TROT - 1) ? (0) : (m + 1);

            state[m].i_spin = (ofst & (NUM_SPIN - 1));
            state[m].up_spin = trotters[up][state[m].i_spin];
            state[m].down_spin = trotters[down][state[m].i_spin];
            state[m].h_local = h_prefetch[m];
            state[m].log_rand_local = log_rand_prefetch[m];
        }

        // Read h and log_rand
    READ_H:
        for (u32_t m = 0; m < NUM_TROT; m++) {
#pragma HLS UNROLL
            u32_t ofst = (((stage + 1) + NUM_SPIN - m) & (NUM_SPIN - 1));
            h_prefetch[m] = h[ofst];
        }

    GEN_RAND:
        for (u32_t m = 0; m < NUM_TROT; m++) {
#pragma HLS UNROLL
            log_rand_prefetch[m] = this->generateRandomNumber(info[m].seed);
        }

        // Shift down jcoup_local
    SHIFT_JCOUP:
        for (u32_t ofst = 0; ofst < NUM_SPIN; ofst++) {
#pragma HLS UNROLL
            for (i32_t m = NUM_TROT - 2; m >= 0; m--) {
#pragma HLS UNROLL
                jcoup_local[m + 1][ofst] = jcoup_local[m][ofst];
            }
            jcoup_local[0][ofst] = jcoup_prefetch[ofst];
        }

        // Read New Jcuop[0]
    READ_JCOUP:
        for (u32_t ofst = 0; ofst < NUM_SPIN; ofst++) {
// #pragma HLS PIPELINE
#pragma HLS UNROLL
            jcoup_prefetch[ofst] = jcoup[(stage + 1) & (NUM_SPIN - 1)][ofst];
        }

        // Run Trotter Units
    UPDATE_OF_TROTTERS:
        for (u32_t m = 0; m < NUM_TROT; m++) {
#pragma HLS UNROLL
            de[m] = UpdateOfTrotters(trotters[m], jcoup_local[m]);
        }

        // Run final step of Trotter Units
    UPDATE_OF_TROTTERS_FINAL:
        for (u32_t m = 0; m < NUM_TROT; m++) {
#pragma HLS UNROLL
            UpdateOfTrottersFinal(stage, info[m], state[m], de[m], trotters[m]);
        }
    }
}

/*
 * Generate Random Number
 */
float PricingEngine::generateRandomNumber(int &seed)
{
#pragma HLS INLINE

    // Seed can't be zero
    const int i4_huge = 2147483647;
    int k;
    float r;

    k = seed / 127773;
    seed = 16807 * (seed - k * 127773) - k * 2836;

    if (seed < 0) {
        seed = seed + i4_huge;
    }

    r = (float)(seed)*4.656612875E-10;

    /* SQA tunning */
    return log(r);
}
