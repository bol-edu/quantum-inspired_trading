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

Since the required memory space of coefficients is too large, it's not reasonable to put all the data into the tiny on-chip SRAM. Thus, we need a cache to store these data. The original algorithm requires scanning all the coefficients multiple times, which produces a lot of cache miss.

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
