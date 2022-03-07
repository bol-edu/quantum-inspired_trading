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

#ifndef PRICINGENGINE_H
#define PRICINGENGINE_H

#include "aat_defines.hpp"
#include "aat_interfaces.hpp"
#include "ap_int.h"
#include "exch2ising.hpp"
#include "hls_stream.h"

#define PE_CAPTURE_FREEZE (1 << 31)

typedef struct pricingEngineRegControl_t {
    ap_uint<32> control;
    ap_uint<32> config;
    ap_uint<32> capture;
    ap_uint<32> strategy;
    ap_uint<32> reserved04;
    ap_uint<32> reserved05;
    ap_uint<32> reserved06;
    ap_uint<32> reserved07;
} pricingEngineRegControl_t;

typedef struct pricingEngineRegStatus_t {
    ap_uint<32> status;
    ap_uint<32> rxResponse;
    ap_uint<32> processResponse;
    ap_uint<32> txOperation;
    ap_uint<32> strategyNone;
    ap_uint<32> strategyPeg;
    ap_uint<32> strategyLimit;
    ap_uint<32> strategyUnknown;
    ap_uint<32> rxEvent;
    ap_uint<32> debug;
    ap_uint<32> reserved10;
    ap_uint<32> reserved11;
    ap_uint<32> reserved12;
    ap_uint<32> reserved13;
    ap_uint<32> reserved14;
    ap_uint<32> reserved15;
} pricingEngineRegStatus_t;

typedef struct pricingEngineRegStrategy_t {
    // 32b registers are wider than required here for some fields (e.g. select
    // and enable) but not sure what XRT does in terms of packing, potential to
    // get messy in terms of register map decoding from host
    // TODO: width appropriate fields when XRT register map packing mechanism is
    // understood
    ap_uint<32> select;  // 8b
    ap_uint<32> enable;  // 8b
    ap_uint<32> totalBid;
    ap_uint<32> totalAsk;
} pricingEngineRegStrategy_t;

typedef struct pricingEngineRegThresholds_t {
    ap_uint<32> threshold0;
    ap_uint<32> threshold1;
    ap_uint<32> threshold2;
    ap_uint<32> threshold3;
    ap_uint<32> threshold4;
    ap_uint<32> threshold5;
    ap_uint<32> threshold6;
    ap_uint<32> threshold7;
} pricingEngineRegThresholds_t;

typedef struct pricingEngineCacheEntry_t {
    ap_uint<32> bidPrice;
    ap_uint<32> askPrice;
    ap_uint<32> valid;
} pricingEngineCacheEntry_t;

/**
 * PricingEngine Core
 */
class PricingEngine {
   public:
    void responsePull(ap_uint<32> &regRxResponse,
                      orderBookResponseStreamPack_t &responseStreamPack,
                      orderBookResponseStream_t &responseStream);

    void pricingProcess(ap_uint<32> &regStrategyControl,
                        ap_uint<32> &regProcessResponse,
                        ap_uint<32> &regStrategyNone,
                        ap_uint<32> &regStrategyPeg,
                        ap_uint<32> &regStrategyLimit,
                        ap_uint<32> &regStrategyUnknown,
                        pricingEngineRegStrategy_t *regStrategies,
                        orderBookResponseStream_t &responseStream,
                        orderEntryOperationStream_t &operationStream);

    // ERM formulation validation
    // SBM parameter validation
#ifndef __SYNTHESIS__
    template<class T> void checkSBMCoeff(T J[physical_bits][physical_bits], float c0);
#endif
    // SBM solution validation
    bool checkExchCycle(bool spin[physical_bits]);
    bool checkProfitable(bool spin[physical_bits], float exch_logged_prices[physical_bits - 1]);
#ifndef __SYNTHESIS__
    bool checkSBMSolution(bool spin[physical_bits], float exch_logged_prices[physical_bits - 1]);
#endif

    // For SBM
    void ERM(int index, float logged_price, float M1, float M2,
             float J[physical_bits][physical_bits], float exch_logged_rates[physical_bits - 1], bool &regInitConstr);

    // For SQA
    void ERM(int index, float logged_price, float M1, float M2,
             float J[physical_bits][physical_bits], float h[physical_bits]);
    void SBM(float Q_Matrix[physical_bits][physical_bits],
            dcal_t y[physical_bits], dcal_t x[physical_bits],
            int steps, float dt, float c0, dcal_t& best_energy,
            int& best_step, bool best_spin[physical_bits], ap_uint<32> &regSBMExecStatus);

    bool pricingStrategyPeg(ap_uint<8> thresholdEnable,
                            ap_uint<32> thresholdPosition,
                            orderBookResponse_t &response,
                            orderEntryOperation_t &operation);

    bool pricingStrategyLimit(ap_uint<8> thresholdEnable,
                              ap_uint<32> thresholdPosition,
                              orderBookResponse_t &response,
                              orderEntryOperation_t &operation);

    void operationPush(ap_uint<32> &regCaptureControl,
                       ap_uint<32> &regTxOperation,
                       ap_uint<1024> &regCaptureBuffer,
                       orderEntryOperationStream_t &operationStream,
                       orderEntryOperationStreamPack_t &operationStreamPack);

    void eventHandler(ap_uint<32> &regRxEvent,
                      clockTickGeneratorEventStream_t &eventStream);

   private:
    // pricingEngineRegThresholds_t thresholds[NUM_SYMBOL];
    pricingEngineCacheEntry_t cache[NUM_SYMBOL];
};

#endif
