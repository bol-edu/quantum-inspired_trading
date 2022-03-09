# SBM-accelerated currency arbitrage machine

## Description

This repository aims to provide an SBM-accelerated trading strategy for the Xilinx AAT platform.

### Implementation of Ising model problem formulation

The QUBO/Ising-model formulation of the currency arbitrage problem is generated real-time in the `ERM` function.

#### Optimization
1. Since the Ising formulation is a symmetric matrix, the function cuts computation in half.
2. Once the constraint function is initialized, `ERM` only updates matrix entries corresponding to the updated exchange rates.
### Simulated bifurcation algorithm overview

Simulated bifurcation is a heuristic algorithm inspired by quantum bifurcation machine (QbM), which is based on quantum adiabatic optimization using nonlinear oscillators exhibiting quantum-mechanical bifurcation phenomena [1].  Simulated bifurcation tries to solve the equations of motions of classical bifurcation machine (CbM), a classical mechanical analogy to QbM. The equations are simplified so that they can be solved by the symplectic Euler method.  The symplectic Euler method produces an approximate solution by iterating two equations, in which the time variable is discretized to time steps [4].
For the discrete simulated bifurcation (dSB) algorithm that we implemented, the two equations are eq.(11) and eq.(12) in [1].

We broke down one SBM update time step as the following:
1. update x vector
2. discretize x vector (turn x vector from float to bool type)
3. update y vector
4. bound x and y vectors (reset the value if out of bound)

### Optimizations of Simulated Bifurcation
The following optimizations enable each pricing process to be under 7 microseconds.
#### Dataflow and hls::stream
To enable optimization by the DATAFLOW pragma, the time step of an update is wrapped in the function `SBM_update`.  Multiple streams are instantiated to allow concurrent computation of different sub-functions in `SBM_update`.

#### Pipeline
Every for-loop in SBM is pipelined to its full extent, most of which has II=1.

#### Flipping sign bit instead of multiplying (-1)
In discretized simulated bifurcation, the matrix-vector multiplication can be simplified.  Instead of using `fmul` operations, a number that multiplies 1 or -1 is reduced to the value unchanged or with sign bit flipped.  The method reduces resource usage, improving area and latency.

#### Faster accumulation by building adder trees
Vitis HLS usually infers the `fadd` operation instead of `facc` or `fmacc` in our accumulation function, and it results in low parallelism.  Our template programming method helps HLS schedule the `fadd` operations in a binary tree shape, reducing latency for accumulation.

#### References:
1. Hayato Goto et al., "High-performance combinatorial optimization based on classical mechanics", SCIENCE ADVANCES, Feb. 2021, Vol 7, Issue 6, doi: 10.1126/sciadv.abe7953
2. K. Tatsumura, R. Hidaka, M. Yamasaki, Y. Sakai and H. Goto, "A Currency Arbitrage Machine Based on the Simulated Bifurcation Algorithm for Ultrafast Detection of Optimal Opportunity," 2020 IEEE International Symposium on Circuits and Systems (ISCAS), 2020, pp. 1-5, doi: 10.1109/ISCAS45731.2020.9181114.
3. M. Yamasaki et al., "Live Demonstration: Capturing Short-Lived Currency Arbitrage Opportunities with a Simulated Bifurcation Algorithm-Based Trading System," 2020 IEEE International Symposium on Circuits and Systems (ISCAS), 2020, pp. 1-1, doi: 10.1109/ISCAS45731.2020.9180679.
4. https://en.wikipedia.org/wiki/Semi-implicit_Euler_method

## Installation
### Build full project
1. Replace the files in Accelerated_Algorithmic_Trading/hw/pricingEngine with the files in src/hw/pricingEngine.
2. Follow the AAT original build flow and test flow (mentioned in https://github.com/kevinjantw/Xilinx_ACC_2021_Submission)

### Build pricingEngine only
1. Execute `make` command in the `src/hw/pricingEngine/test` folder.
2. ```vitis_hls -p prj``` opens the pricingEngine project.
3. Vitis HLS development functions (C simulation, synthesis, and co-sim) can be applied.

## Usage
### Testbench for pricingEngine

An example testbench file `src/hw/pricingEngine/test/ordBookResp.txt` prepares `orderBookResponse` data for test.  The comments in the file describes the file format.

## Support


## Roadmap


## Contributing


## License
