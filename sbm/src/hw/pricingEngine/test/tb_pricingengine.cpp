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
#include <cstring>
#include <vector>

#include "pricingengine_kernels.hpp"

#define NUM_TEST_SAMPLE_PE (9)

ap_uint<32> float2Uint(float n)
{
    return (ap_uint<32>)(*(ap_uint<32> *)&n);
}

float Uint2Float(ap_uint<32> n)
{
    return (float)(*(float *)&n);
}

int main()
{
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

    // Read exchange rates
    std::string priceFilePath = "ordBookResp.txt";
    std::ifstream ifs(priceFilePath.c_str());
    if (!ifs)
    {
        std::cerr << "Error: \"" << priceFilePath << "\" does not exist!!\n";
        return false;
    }
    // '#' indicates that the line is a comment
    // The code assumes '#' is followed by at least one space
    // and uses getline to read the rest of the line
    std::string word;
    ifs >> word;
    while (word == "#")
    {
        std::getline(ifs, word);
        ifs >> word;
    }
    // The first line that is not a comment is an integer
    // indicating the number of orderBookResponses.
    int responseCount{};
    responseCount = std::stoi(word);
#define exchCast(x) (float2Uint(x))
    // std::cout << "float to uint check: exchCast(1 / 0.85073) = " << exchCast(1 / 0.85073) << "\n";
    // std::cout << "uint to float check: Uint2Float(exchCast(1 / 0.85073)) = " << Uint2Float(exchCast(1 / 0.85073)) << "\n";
    float bidPrice{};
    float askPrice{};
    std::vector<orderBookResponseVerify_t> orderBookResponses;
    for (int i = 0; i < responseCount; ++i)
    {
        ifs >> bidPrice >> askPrice;
        std::cout << bidPrice << " " << askPrice << "\n";
        // symbolIndex, bidCount[], bidPrice[], bidQuantity[], askCount[],
        // askPrice[], askQuantity[]
        // See exch2ising.hpp to see the currency mapping
        orderBookResponses.push_back({i, {1, 0, 0, 0, 0}, {exchCast(1 / bidPrice), 0, 0, 0, 0}, {1, 0, 0, 0, 0}, {1, 0, 0, 0, 0}, {exchCast(askPrice), 0, 0, 0, 0}, {1, 0, 0, 0, 0}});
    }
    // End of file reading

    for (int i = 0; i < responseCount; ++i)
    {
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

    // kernel call to process operations
    while (!responseStreamPackFIFO.empty())
    {
        pricingEngineTop(regControl, regStatus, regCapture, regStrategies,
                         responseStreamPackFIFO, operationStreamPackFIFO,
                         eventStreamFIFO);
    }

    // drain response stream
    while (!operationStreamPackFIFO.empty())
    {
        operationPack = operationStreamPackFIFO.read();
        intf.orderEntryOperationUnpack(&operationPack, &operation);

        std::cout << "ORDER_ENTRY_OPERATION: {" << operation.opCode << ","
                  << operation.symbolIndex << "," << operation.orderId << ","
                  // make price float again using reinterpret cast
                  << operation.quantity << "," << reinterpret_cast<float &>(operation.price) << ","
                  << operation.direction << "}" << std::endl;
    }

    // log final status
    std::cout << "--" << std::hex << std::endl;
    std::cout << "STATUS: ";
    std::cout << "PE_STATUS=" << regStatus.status << " ";
    std::cout << "PE_RX_RESP=" << regStatus.rxResponse << " ";
    std::cout << "PE_PROC_RESP=" << regStatus.processResponse << " ";
    std::cout << "PE_TX_OP=" << regStatus.txOperation << " ";
    // std::cout << "PE_STRATEGY_NONE=" << regStatus.strategyNone << " ";
    std::cout << "regERMInitConstr=" << regStatus.strategyNone << " ";
    std::cout << "regSBMExecStatus=" << regStatus.strategyPeg << " ";
    std::cout << "countAncillaFlip=" << regStatus.strategyLimit << " ";
    std::cout << "PE_STRATEGY_NA=" << regStatus.strategyUnknown << " ";
    std::cout << "PE_RX_EVENT=" << regStatus.rxEvent << " ";
    std::cout << "PE_DEBUG=" << regStatus.debug << " ";
    std::cout << std::endl;

    std::cout << std::endl;
    std::cout << "Done!" << std::endl;

    return 0;
}
