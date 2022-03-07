## Currency Arbitrage in QUBO Form

The problem of finding the optimal arbitrage opportunity is to find the most profitable cycle in a digraph in which the nodes are the assets and the edges are the conversion rates. This problem can be cast into a quadratic unconstrained binary optimization (QUBO) form, of which the lowest cost solution is the optimal solution that we seek. In the original white paper by 1QBit [1], a QUBO solver of quantum annealing type is considered. We solve the QUBO problems with simulated quantum annealing (SQA) and simulated bifurcation (SB) algorithms implemented on FPGA instead.

Specifically, the cost function is of the following form:

$ C = -\sum_{(i, j) \in E} x_{ij} \log c_{ij} + M_1 \sum_{i \in V} \left(\sum_{j, (i, j) \in E} x_{ij} - \sum_{j, (j, i) \in E} x_{ij} \right)^2 \\+ M_2 \sum_{i \in V} \left(\sum_{j, (i, j) \in E} x_{ij} \right) \left(\sum_{j, (i, j) \in E} x_{ij} - 1 \right) $

in which the weighted digraph $G = (V, E)$ represents the foreign exchange market, the edge weights $c_{ij}$ corresponds to the exchange rates and the variables $x_{ij} \in \{0, 1\}$ are indicators whether we should take the transaction or not. The parameters $M_1$ and $M_2$ are penalty strengths that should be large enough to prevent illegal solutions (e.g., solutions that do not form a cycle). In this way, the lowest cost solution to this problem will be the most profitable simple cycle arbitrage route (or none if no profitable routes exist).

## Parameter Optimization Flow

### Preparation

- Currency exchange rate dataset with arbitrage opportunity solutions

### Optimization Flow

1. Define objective function
   1. Separate training dataset into two parts, problems with no transactions as solutions and problems with nontrivial solutions. Ratio of these two types should be the same as our intended use case.
   2. Randomly choose x problems from the dataset. Each problem gives (calculated profitability - optimal profitability) points of score to the objective function.
   3. x can be larger if we have more computational power at hand. 100 may be a good start.
2. Define parameter bounds (guess what is reasonable)
   1. $5 < M_1, M_2 < 20$
   2. $50 < t < 200$ (SB)
   3. $0.001 < dt < 1$ (SB)
   4. $0 < c_0 < 1$ (SB)
   5. linear $a(t)$ from 0 to 1 (include in advanced optimization) (SB)
   6. $0.001 < y(t=0) < 0.1$ (probably not that important) (SB)
   7. $4 \leq M \leq 64$ (SQA)
   8. $0 < T < 500$ (SQA)
   9. $\Gamma(t)$ : piecewise linear or parametric ($At^b$ or $Ae^{bt}$) (SQA)
3. Use Nelder-Mead or Powell method to conduct bounded optimization on the objective function
   1. Nelder-Mead: https://en.wikipedia.org/wiki/Nelder–Mead_method
   2. Powell: https://en.wikipedia.org/wiki/Powell's_method
   3. An implementation is available in scipy: https://docs.scipy.org/doc/scipy/reference/generated/scipy.optimize.minimize.html
4. Use validation dataset to validate the performance of the new parameters. Compare with previous results.
   1. If any parameter is optimized to near the bounds, consider going back to step 2. and loosen up the bounds.
   2. If the validation results are very similar multiple (e.g., 10) times in a row, terminate the optimization process

## Python Implementation

A python implementation of the above formulation process can be found at `fx_arb_gen.py`. The function `build_Q(problem, cur_lst, M1, M2)` takes in the exchange rates and penalty strengths $M_1$ and $M_2$, and returns a matrix $Q$ such that the cost function $C = \textbf{x}^T Q \textbf{x}$, where $\textbf{x} \in \{0, 1\}^N$ denotes the variable vector.

Additionally, the function can also output the cost function in Ising form if `mode='Ising'` is specified. In this case, matrix $J$ and vector $\textbf{h}$ are returned, so that $C = \textbf{s}^T J \textbf{s} + \textbf{h}^T \textbf{s}$, $\textbf{s} \in \{-1, 1\}^N$.

## References

[1] https://1qbit.com/whitepaper/arbitrage/
