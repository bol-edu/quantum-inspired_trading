# SQA-accelerated currency arbitrage machine

## Description

This repository aims to provide an SQA-accelerated trading strategy for the Xilinx AAT platform.

### Implementation of Simulated Quantum Annealing

The original algorithm of simulated quantum annealing is not suitable for FPGA implementation. Low parallelism, large memory space for coefficients, and high frequency of the IO request are the main obstacles. The time and space complexities are O(M\*N\*N) and O(N*N), where N and M are the numbers of spins and trotters. We apply some optimization techniques to the algorithm to boost the performance of our FPGA implementation.

#### Parallel Processing Engine and Optimized IO schedule

In [1], they proposed an innovative processing schedule that improves parallelism and significantly reduces the frequency of IO requests. After analyzing the data dependency of the processing loop, we find out that it's possible to increase the parallelism. By introducing different initial latency to the processing engine of each trotter, we can run multiple processing engines simultaneously without breaking the data dependency. Thus, the overall time complexity now is O(N*N) which means the number of spins now is the only factor that will affect the processing time.

Another benefit, lesser IO request to the coefficients, make us easier to design the cache mechanism. We will discuss in the section, Cache Mechanism, later.

#### Parallel Reduction

To further boost the performance, we introduce parallel reduction to cut the latency of the processing engine. The mechanism is like divide-and-conquer with a binary-tree-like flow. To accomplish this feature with HLS, we use template programming to perform the recursive operation. The time complexity becomes O(N*ln(N)) now.

There are two factors to support parallel reduction. One is to place multiple floating-point adders in one processing engine. Another is fetching multiple input data once, that is, higher bandwidth. In the section, Data Aggregation and Prefetching, we will discuss how to increase the bandwidth.

#### Cache Mechanism

Since the required memory space of coefficients is too large, it's not reasonable to put all the data into the tiny on-chip SRAM. Thus, we need a cache to store these data. The original algorithm requires scanning all the coefficients multiple times, which produces a lot of cache misses.

Hopefully, the new processing schedule makes the reuse of one coefficient available. All the coefficients only need to be fetched once. Our cache memory is composed of multiple separated rows. Each row is connected to one processing engine only. A new-coming element will first be stored at the first row(row[0]) and sent to the first PE(PE[0]). Then it will be shifted to the next row(row[1]) and consumed by PE[1]. Following this rule, it ends up will be evicted after the processing of the last PE. The free cache slot will serve for the next incoming coefficient.

#### Prefetching

Processing IO requests and running the process engine are the major time-consuming tasks over the whole process. To furtherly boost the performance, we overlap two tasks in the same pipeline stage by prefetching. While the PEs are processing the data, we will simultaneously process the IO request to fetch the new cache element for the next run.

#### Negator of Floating-Point Number

Xilinx's HLS compiler negates a single-precision floating-point number by xor-ing it with a 32-bit value. However, an cost-effective method is just xor-ing the sign bit only with a 1-bit value. With the library of the arbitrary-precision number, we can accomplish this idea in HLS and end up saving lots of LUT and FF.

### References

1. H. Waidyasooriya and M. Hariyama, "Highly-Parallel FPGA Accelerator for Simulated Quantum Annealing," in IEEE Transactions on Emerging Topics in Computing, DOI: 10.1109/TETC.2019.2957177.
2. Liu, CY., Waidyasooriya, H.M. & Hariyama, M. Design space exploration for an FPGA-based quantum annealing simulator with interaction-coefficient-generators. J Supercomput (2021).
3. K. Tatsumura, R. Hidaka, M. Yamasaki, Y. Sakai and, H. Goto, "A Currency Arbitrage Machine Based on the Simulated Bifurcation Algorithm for Ultrafast Detection of Optimal Opportunity," 2020 IEEE International Symposium on Circuits and Systems (ISCAS), 2020, pp. 1-5, DOI: 10.1109/ISCAS45731.2020.9181114.
4. M. Yamasaki et al., "Live Demonstration: Capturing Short-Lived Currency Arbitrage Opportunities with a Simulated Bifurcation Algorithm-Based Trading System," 2020 IEEE International Symposium on Circuits and Systems (ISCAS), 2020, pp. 1-1, DOI: 10.1109/ISCAS45731.2020.9180679. 

## Installation

### Build full project

1. Replace the files in Accelerated_Algorithmic_Trading/hw/pricingEngine with the files in src/hw/pricingEngine.
2. Follow the AAT original build flow and test flow (mentioned in https://github.com/bol-edu/xilinx-acc-2021_submission)

### Build pricingEngine only

1. Execute `make` command in the `src/hw/pricingEngine/test` folder.
2. ```vitis_hls -p prj``` opens the pricingEngine project.
3. Vitis HLS development flow (C simulation, synthesis, and co-sim) can be applied.

## Usage

### Testbench for pricingEngine

Example data files `src/hw/pricingEngine/test/data/data[0-10].txt` prepare multiple sets of `orderBookResponse` data for test. The comment in the file describes the file format.

## Experimental Results

The following experiments were conducted to demonstrate the solution quality of the SQA-accelerated currency arbitrage machine (SQA-CAM).  We ran the executables built from the C++ source code.  The experiments can be reproduced without installing any FPGA card or the entire Vitis software.  However, some libraries of AAT(Q2) and Vitis HLS are required; for brevity, the file requirements are not listed here.  The compilation command may look like the following:

```g++ -I./test/include -I./ test/include/aat_interfaces.cpp test/tb_pricingengine.cpp pricingengine.cpp pricingengine_top.cpp -o tb_pricingEngine```

If you have installed the entire Vitis software, you can `Build pricingEngine only` and get the executable `test/prj/sol/csim/build/csim.exe`.

The following table shows the SQA-CAM can yield about 75% profitable solutions in only 10 iterations. The spin `1` denotes that the currency exchange order should be placed, and `0` otherwise. Execution time is not the main point of interest, so it is not recorded.

|                                     | 10000 cases, 10 iterations | 10000 cases, 25 iterations |
| ----------------------------------- | -------------------------- | -------------------------- |
| Profitable                          | 7443                       | 7442                       |
| Has cycle, Not profitable           | 1181                       | 1182                       |
| No  cycle                           | 1376                       | 1376                       |

### Experimental Setup

* Processor: Intel(R) Core(TM) i7-11700 @ 2.50GHz
* Operating System: Ubuntu 20.04
* Compiler: gcc version 9.4.0
* Test data generation:
  * `pcap_gen.py` (executed with Python 3.8.5)
  * 11 cases: `xilinx-acc-2021_submission/test_toolkit/arb_data_gen.sh`
  * 10000 cases: `xilinx-acc-2021_submission/test_toolkit/arb_data_gen10000.sh`
* Running test scripts:
  * The scripts and experiment logs are located under `xilinx-acc-2021_submission/sqa/src/hw/pricingEngine/exp`.
  * `test_pricingEngine_arb_10000_10.sh` runs the executable with 10000 different cases.
    * Run SQA with 10 iterations.
    * It will generate an experiment log named `test_pricingEngine_cur_arb_5_9_Log.10000.10.log`
  * `test_pricingEngine_arb_10000_25.sh` runs the executable with 10000 different cases.
    * Run SQA with 25 iterations.
    * It will generate an experiment log named `test_pricingEngine_cur_arb_5_9_Log.10000.25.log`