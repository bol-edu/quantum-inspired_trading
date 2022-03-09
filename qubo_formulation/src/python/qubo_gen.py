# %%
import numpy as np
from itertools import combinations


# %%
def build_log_xrate_dict(problem, cur_lst):
    log_xrate_dict = {}

    for fxdata in problem:
        log_xrate_dict[(cur_lst.index(fxdata[0][:3]), cur_lst.index(fxdata[0][4:]))] = -np.log(float(fxdata[-1])) # USD <- JPY
        log_xrate_dict[(cur_lst.index(fxdata[0][4:]), cur_lst.index(fxdata[0][:3]))] = np.log(float(fxdata[-2])) # USD -> JPY
    
    return log_xrate_dict


# %% [markdown]
# ## Build output array Q
# 
# Refer to the edge-based formulation in <https://1qbit.com/whitepaper/arbitrage/>
# Q is the (QUBO) problem Hamiltonian. Item variables are assumed  to be {0, 1}.
# Objective: Minimize Q.dot(state).dot(state)

# %%
def build_Q(problem, cur_lst, M1, M2, mode='QUBO'):
    """
    Build qubo problem matrix Q from problem, with penalty strengths M1 and M2
    
    Parameters:
        problem (List[List[str]]): List of lists that describes all the currency exchange rates
                                   Inner lists should contain four elements:
                                     1. Currency pair (in the format 'AAA/BBB')
                                     2. Time stamp (in the format 'YYYYMMDD HH:MM:SS.SSS')
                                     3. Sell price of AAA
                                     4. Buy price of AAA
        cur_lst (List[str]): List of currencies present in the problem
        M1, M2 (num): Penalty strengths
        mode (str): Output format, defaults to 'QUBO'. 'Ising' can also be specified
    """
    log_xrate_dict = build_log_xrate_dict(problem, cur_lst)

    var_lst = list(log_xrate_dict.keys())
    N = len(var_lst)

    pen1 = np.zeros(N**2).reshape((N, N))
    pen2 = np.zeros(N**2).reshape((N, N))

    for k in range(len(cur_lst)):
        v1 = np.array([(k == i) - (k == j) for i, j in var_lst], dtype=int)
        pen1 += np.outer(v1, v1)

        v2 = np.array([(k == i) for i, j in var_lst], dtype=int)
        pen2 += np.outer(v2, v2) - np.diag(v2)

    if mode == 'QUBO':
        return -np.diag([log_xrate_dict[var] for var in var_lst]) + M1 * pen1 + M2 * pen2
    if mode == 'Ising':
        return qubo2ising(-np.diag([log_xrate_dict[var] for var in var_lst]) + M1 * pen1 + M2 * pen2)


# %% [markdown]
# ## Convert to Ising
# Ising item variables: {-1, 1}
# Objective: Minimize J.dot(state).dot(state) + h.dot(state)

# %%
def qubo2ising(Q):
    """
    Convert a qubo problem matrix into an Ising problem
    """
    S = (Q + Q.T) / 2 # making sure Q is symmetric, not necessary in this file

    J = (S - np.diag(np.diag(S))) / 4 # coupling
    h = np.sum(S, axis=0) / 2 # local field
    offset = (np.sum(S) + np.sum(np.diag(S))) / 4 # offset energy, add this back to get the original QUBO energy

    return J, h, offset


# %% [markdown]
# ## Convert to QUBO
# QUBO item variables: {1, 0}
# Objective: Minimize Q.dot(state).dot(state)

# %%
def ising2qubo(J, h):
    """
    Convert a Ising problem into a QUBO problem matrix
    """
    S = (J + J.T) / 2 # making sure Q is symmetric, not necessary in this file

    Q = 4*J + np.diag(2*h - 4*np.sum(J, axis=0))
    offset = np.sum(J) - np.sum(h) # offset energy, add this back to get the original Ising energy

    return Q, offset


# %%
def main():
    cur_lst = ['USD', 'EUR', 'JPY', 'GBP', 'CHF'] # currencies considered in this problem
    problem = [['EUR/GBP', '20210401 00:00:00.596', '0.85062', '0.85069'], 
               ['EUR/JPY', '20210401 00:00:00.991', '129.877', '129.885'], 
               ['USD/CHF', '20210401 00:00:00.730', '0.94391', '0.94397'], 
               ['EUR/CHF', '20210401 00:00:00.590', '1.10691', '1.107'], 
               ['CHF/JPY', '20210401 00:00:00.989', '117.325', '117.337'], 
               ['GBP/JPY', '20210401 00:00:00.986', '152.673', '152.691'], 
               ['EUR/USD', '20210401 00:00:00.656', '1.17267', '1.17272'], 
               ['GBP/USD', '20210401 00:00:00.991', '1.37851', '1.37863'], 
               ['USD/JPY', '20210401 00:00:00.995', '110.753', '110.759']]
    print('Sample problem:\n', problem)

    M1 = 50
    M2 = 25
    Q = build_Q(problem, cur_lst, M1, M2)
    print('Q:\n', Q)


# %%
if __name__ == '__main__':
    main()
