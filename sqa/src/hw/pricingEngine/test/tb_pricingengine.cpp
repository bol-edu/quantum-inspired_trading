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

#include <fstream>
#include <iomanip>
#include <iostream>

#include "pricingengine_kernels.hpp"

#define NUM_TEST_SAMPLE_PE (9)

int main() {
    pricingEngineRegControl_t regControl = {0};
    pricingEngineRegStatus_t regStatus = {0};
    ap_uint<1024> regCapture = 0x0;
    pricingEngineRegStrategy_t regStrategies[NUM_SYMBOL];

    mmInterface intf;
    orderBookResponseVerify_t responseVerify;
    orderBookResponse_t response;
    orderBookResponsePack_t responsePack;
    orderEntryOperation_t operation;
    orderEntryOperationPack_t operationPack;

    orderBookResponseStreamPack_t responseStreamPackFIFO(
        "responseStreamPackFIFO");
    orderEntryOperationStreamPack_t operationStreamPackFIFO(
        "operationStreamPackFIFO");
    clockTickGeneratorEventStream_t eventStreamFIFO("eventStreamFIFO");

    std::cout << "PricingEngine Test" << std::endl;
    std::cout << "------------------" << std::endl;

    memset(&regStrategies, 0, sizeof(regStrategies));

    orderBookResponseVerify_t orderBookResponses[NUM_TEST_SAMPLE_PE] = {
        // symbolIndex, bidCount[], bidPrice[], bidQuantity[], askCount[],
        // askPrice[], askQuantity[]
        // {0,{1,1,1,0,0},{5853300,5853200,5853100,0,0},{18,18,18,0,0},{8,0,0,0,0},{5859100,0,0,0,0},{18,0,0,0,0}},
        // {0,{1,1,1,0,0},{5853300,5853200,5853100,0,0},{18,18,18,0,0},{1,1,0,0,0},{5859100,5859200,0,0,0},{18,18,0,0,0}},
        // {0,{1,1,1,0,0},{5853300,5853200,5853100,0,0},{18,18,18,0,0},{1,1,1,0,0},{5859100,5859200,5859300,0,0},{18,18,18,0,0}},
        // {0,{1,1,1,0,0},{5853300,5853200,5853100,0,0},{18,18,18,0,0},{1,1,1,1,0},{5859100,5859200,5859300,5859300,0},{18,18,100,18,0}},
        {0,
         {1, 0, 0, 0, 0},
         {1066825438, 0, 0, 0, 0},
         {1, 0, 0, 0, 0},
         {1, 0, 0, 0, 0},
         {1062847203, 0, 0, 0, 0},
         {1, 0, 0, 0, 0}},
        {1,
         {1, 0, 0, 0, 0},
         {1065850845, 0, 0, 0, 0},
         {1, 0, 0, 0, 0},
         {1, 0, 0, 0, 0},
         {1064411175, 0, 0, 0, 0},
         {1, 0, 0, 0, 0}},
        {2,
         {1, 0, 0, 0, 0},
         {1003919622, 0, 0, 0, 0},
         {1, 0, 0, 0, 0},
         {1, 0, 0, 0, 0},
         {1125691033, 0, 0, 0, 0},
         {1, 0, 0, 0, 0}},
        {3,
         {1, 0, 0, 0, 0},
         {1006389094, 0, 0, 0, 0},
         {1, 0, 0, 0, 0},
         {1, 0, 0, 0, 0},
         {1124196875, 0, 0, 0, 0},
         {1, 0, 0, 0, 0}},
        {4,
         {1, 0, 0, 0, 0},
         {1007938489, 0, 0, 0, 0},
         {1, 0, 0, 0, 0},
         {1, 0, 0, 0, 0},
         {1121813269, 0, 0, 0, 0},
         {1, 0, 0, 0, 0}},
        {5,
         {1, 0, 0, 0, 0},
         {1060745396, 0, 0, 0, 0},
         {1, 0, 0, 0, 0},
         {1, 0, 0, 0, 0},
         {1068528472, 0, 0, 0, 0},
         {1, 0, 0, 0, 0}},
        {6,
         {1, 0, 0, 0, 0},
         {1062882120, 0, 0, 0, 0},
         {1, 0, 0, 0, 0},
         {1, 0, 0, 0, 0},
         {1066801845, 0, 0, 0, 0},
         {1, 0, 0, 0, 0}},
        {7,
         {1, 0, 0, 0, 0},
         {1063731570, 0, 0, 0, 0},
         {1, 0, 0, 0, 0},
         {1, 0, 0, 0, 0},
         {1066250042, 0, 0, 0, 0},
         {1, 0, 0, 0, 0}},
        {8,
         {1, 0, 0, 0, 0},
         {1007395120, 0, 0, 0, 0},
         {1, 0, 0, 0, 0},
         {1, 0, 0, 0, 0},
         {1122675063, 0, 0, 0, 0},
         {1, 0, 0, 0, 0}},

    };

    for (int i = 0; i < NUM_TEST_SAMPLE_PE; i++) {
        responseVerify = orderBookResponses[i];

        response.symbolIndex = responseVerify.symbolIndex;

        response.bidCount =
            (responseVerify.bidCount[4], responseVerify.bidCount[3],
             responseVerify.bidCount[2], responseVerify.bidCount[1],
             responseVerify.bidCount[0]);

        response.bidPrice =
            (responseVerify.bidPrice[4], responseVerify.bidPrice[3],
             responseVerify.bidPrice[2], responseVerify.bidPrice[1],
             responseVerify.bidPrice[0]);

        response.bidQuantity =
            (responseVerify.bidQuantity[4], responseVerify.bidQuantity[3],
             responseVerify.bidQuantity[2], responseVerify.bidQuantity[1],
             responseVerify.bidQuantity[0]);

        response.askCount =
            (responseVerify.askCount[4], responseVerify.askCount[3],
             responseVerify.askCount[2], responseVerify.askCount[1],
             responseVerify.askCount[0]);

        response.askPrice =
            (responseVerify.askPrice[4], responseVerify.askPrice[3],
             responseVerify.askPrice[2], responseVerify.askPrice[1],
             responseVerify.askPrice[0]);

        response.askQuantity =
            (responseVerify.askQuantity[4], responseVerify.askQuantity[3],
             responseVerify.askQuantity[2], responseVerify.askQuantity[1],
             responseVerify.askQuantity[0]);

        intf.orderBookResponsePack(&response, &responsePack);
        responseStreamPackFIFO.write(responsePack);
    }

    // configure
    regControl.control = 0x12345678;
    regControl.config = 0xdeadbeef;
    regControl.capture = 0x00000000;

    // strategy select (per symbol)
    regStrategies[0].select = STRATEGY_PEG;
    regStrategies[0].enable = 0xff;

    // strategy select (global override)
    regControl.strategy = 0x80000002;

    // initial paramter
    union {
        unsigned int uint_data;
        fp_t fp_data;
    } gamma_start, T;
    gamma_start.fp_data = 5.0f;
    T.fp_data = 0.5f;
    regControl.reserved04 = gamma_start.uint_data;
    regControl.reserved05 = T.uint_data;

    // kernel call to process operations
    while (!responseStreamPackFIFO.empty()) {
        pricingEngineTop(regControl, regStatus, regCapture, regStrategies,
                         responseStreamPackFIFO, operationStreamPackFIFO,
                         eventStreamFIFO);
    }

    // drain response stream
    while (!operationStreamPackFIFO.empty()) {
        operationPack = operationStreamPackFIFO.read();
        intf.orderEntryOperationUnpack(&operationPack, &operation);

        std::cout << "ORDER_ENTRY_OPERATION: {" << operation.opCode << ","
                  << operation.symbolIndex << "," << operation.orderId << ","
                  // make price float again using reinterpret cast
                  << operation.quantity << "," << reinterpret_cast<float& >(operation.price) << ","
                  << operation.direction << "}" << std::endl;
    }

    // log final status
    std::cout << "--" << std::hex << std::endl;
    std::cout << "STATUS: ";
    std::cout << "PE_STATUS=" << regStatus.status << " ";
    std::cout << "PE_RX_RESP=" << regStatus.rxResponse << " ";
    std::cout << "PE_PROC_RESP=" << regStatus.processResponse << " ";
    std::cout << "PE_TX_OP=" << regStatus.txOperation << " ";
    std::cout << "PE_STRATEGY_NONE=" << regStatus.strategyNone << " ";
    std::cout << "PE_STRATEGY_PEG=" << regStatus.strategyPeg << " ";
    std::cout << "PE_STRATEGY_LIMIT=" << regStatus.strategyLimit << " ";
    std::cout << "PE_STRATEGY_NA=" << regStatus.strategyUnknown << " ";
    std::cout << "PE_RX_EVENT=" << regStatus.rxEvent << " ";
    std::cout << "PE_DEBUG=" << regStatus.debug << " ";
    std::cout << "PE_RESV0=" << regStatus.reserved10 << " ";
    std::cout << "PE_RESV1=" << regStatus.reserved11 << " ";
    std::cout << "PE_RESV2=" << regStatus.reserved12 << " ";
    std::cout << "PE_RESV3=" << regStatus.reserved13 << " ";
    std::cout << "PE_RESV4=" << regStatus.reserved14 << " ";
    std::cout << "PE_RESV5=" << regStatus.reserved15 << " ";
    std::cout << std::endl;

    std::cout << std::endl;
    std::cout << "Done!" << std::endl;

    return 0;
}
