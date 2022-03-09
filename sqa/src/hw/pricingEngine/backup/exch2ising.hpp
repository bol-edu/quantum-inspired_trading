#ifndef _EXCH2ISING_HPP_
#define _EXCH2ISING_HPP_

#define PHYSICAL_BITS 18
#define NUM_CURRENCIES 5

// give each pair an unique id
// [id*2 / id*2 + 1][0/1]
//  id*2   => bid 
//  id*2+1 => ask
const int exch_index2id[PHYSICAL_BITS][2] = {
    {1, 3}, {3, 1}, {0, 4}, {4, 0}, {3, 2}, {2, 3}, {1, 2}, {2, 1}, {0, 2},
    {2, 0}, {3, 0}, {0, 3}, {1, 0}, {0, 1}, {1, 4}, {4, 1}, {4, 2}, {2, 4}};

// given id, get currency
const char cur_id_lst[NUM_CURRENCIES][4] = {"USD", "EUR", "JPY", "GBP", "CHF"};

#endif