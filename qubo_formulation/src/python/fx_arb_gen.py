# To add a new cell, type '# %%'
# To add a new markdown cell, type '# %% [markdown]'
# %% [markdown]
# # Benchmark Problem Generator: FX Arbitrage
# 
# This notebook aims to build a FX arbitrage problem generator with exchange rate data from TrueFX.
# 
# The notebook is split into two parts. First extracting a problem from TrueFX data, and then build a QUBO problem matrix from the problem.

# %%
import numpy as np
from itertools import combinations

# %% [markdown]
# # Part 1: Extracting a problem from TrueFX data
# %% [markdown]
# ## Find all the available TrueFX data for selected currencies
# 
# DIR should contain the TrueFX data. For example, if `DIR = Path('D:/GitHub/c-sbm/data')`, the directories should look like:
# ```
# D: --- GitHub --- c-sbm --- data --- TrueFX -┬- AUDJPY-2021-04 --- AUDJPY-2021-04.csv
#                                              ├- AUDNZD-2021-04 --- AUDNZD-2021-04.csv
#                                              └- ...
# ```


# %%
def find_cur_pair(DIR, cur_lst):
    """
    Find all available currency pairs (both in cur_lst) under DIR directory

    Parameters:
        DIR (pathlib.Path object): The directory under which TrueFX data is stored
        cur_lst (list[str]): List containing currencies to be considered in this problem
    """
    cur_pair_lst = []

    for child in DIR.joinpath('TrueFX').iterdir():
        for cur in combinations(cur_lst, 2):
            if cur[0] in str(child) and cur[1] in str(child):
                cur_pair_lst.append(child.stem)

    return cur_pair_lst


# %%
class Problem_Generator():
    """
    Build a problem generator that generates problems from a list of opened TrueFX files.
    The gen_prob function can be run multiple times to generate different problems.

    Initialization parameters:
        DIR (pathlib.Path object): The directory under which TrueFX data is stored
        cur_lst (list[str]): List containing currencies to be considered in this problem
        t0 (str or scalar): Start time of the problem generator. If it is string, it should be formatted like 'YYYYMMDD hh:mm:ss.sss'
        dt (float, default=1): Time increment in seconds
    """
    def __init__(self, DIR, cur_lst, t0='YYYYMM01 00:00:00.000', dt=1):
        self.f_lst = self.open_fx_files(DIR, cur_lst)
        self.dt = dt
        self.next_fxdata_lst = [f.readline() for f in self.f_lst]
        self.fxdata_lst = [0] * len(self.f_lst) # initialization
        if isinstance(t0, str):
            self.time = self.date2sec(t0)
        elif isinstance(t0, float) or isinstance(t0, int):
            self.time = t0
    
    @staticmethod
    def open_fx_files(DIR, cur_lst):
        """
        Find all available currency pairs (both in cur_lst) under DIR directory, and return a list of opened files

        Parameters:
            DIR (pathlib.Path object): The directory under which TrueFX data is stored
            cur_lst (list[str]): List containing currencies to be considered in this problem
        """
        return [open(DIR.joinpath(f'TrueFX/{cur_pair}/{cur_pair}.csv'), 'r') for cur_pair in find_cur_pair(DIR, cur_lst)]

    @staticmethod
    def date2sec(s):
        """
        Utility function that turns time string into seconds (omitting year and month).
        """
        return 86400 * (int(s[6:8])-1) + 3600 * int(s[9:11]) + 60 * int(s[12:14]) + float(s[15:])

    def gen_prob(self, dt=None):
        """
        Generates a problem at current time (self.time).
        Can be run multiple times to generate different problems.

        Parameter dt can be set in function argument, or else it defaults to self.dt
        """
        if dt is None:
            dt = self.dt
        self.time += dt

        for i in range(len(self.next_fxdata_lst)): # parallelizable
            while self.date2sec(self.next_fxdata_lst[i][8:29]) <= self.time:
                self.fxdata_lst[i] = self.next_fxdata_lst[i]
                self.next_fxdata_lst[i] = self.f_lst[i].readline()
        
        return [fxdata.rstrip().split(',') for fxdata in self.fxdata_lst]


# %% [markdown]
# ---
# %% [markdown]
# # Part 2: Build a QUBO matrix from the problem

# %% [markdown]
# ## Build log-exchange rate dictionary
# 
# Dictionary entry convention:
# $$(i, j): \log(c_{ij})$$
# If you own one unit of asset i (e.g. USD), you can trade it for $c_{ij}$ units (e.g. USD to CAD exchange rate) of asset j (e.g. CAD).

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


# %% [markdown]
# ## Exact solver

# %%
def check_constraints(state, var_lst, n):
    # state is a spin state (+1, -1)
    broken_constraints = []
    for k in range(n):
        mask1 = [(k == i) for i, j in var_lst]
        mask2 = [(k == j) for i, j in var_lst]
        if np.sum(state[mask1]) != np.sum(state[mask2]):
            broken_constraints.append((k, 1))
        if np.sum(state[mask1]) > 2 - np.sum(mask1):
            broken_constraints.append((k, 2))
    
    return broken_constraints


# %%
def exact_solver(problem, cur_lst):
    """
    Exact solver by iterating through every possible combination of item variables
    """
    log_xrate_dict = build_log_xrate_dict(problem, cur_lst)

    var_lst = list(log_xrate_dict.keys())
    n = len(cur_lst)
    N = len(var_lst)
    c = -np.array([log_xrate_dict[var] for var in var_lst]) # local fields

    lowest_energy = 0
    ground_state = np.zeros(N)
    for i in range(1, 2**N):
        x = np.array([(i >> k) % 2 for k in range(N)]) # turn integer i into binary vector x, (i >> k) is the same as (i / 2**k)

        for k in range(n):
            mask1 = [(k == i) for i, j in var_lst]
            if np.sum(x[mask1]) > 1:
                break
            mask2 = [(k == j) for i, j in var_lst]
            if np.sum(x[mask1]) != np.sum(x[mask2]):
                break
        else:
            energy = c.dot(x)
            if energy < lowest_energy:
                lowest_energy = energy
                ground_state = x
    
    return lowest_energy, ground_state


# %%
def main():
    from pathlib import Path

    # Part 1
    #DIR = Path(__file__).parent
    DIR = Path('D:/GitHub/c-sbm/data') # put TrueFX data under this directory
    cur_lst = ['USD', 'EUR', 'JPY', 'GBP', 'CHF'] # currencies considered in this problem
    print('Currencies:\n', cur_lst)
    
    prob_gen_ins = Problem_Generator(DIR, cur_lst)
    problem = prob_gen_ins.gen_prob() # run this line multiple times to get different problems
    print('Problem:\n', problem)

    # Part 2
    M1 = 50
    M2 = 25
    Q = build_Q(problem, cur_lst, M1, M2)
    print('Q:\n', Q)


# %%
if __name__ == '__main__':
    main()


# %%
