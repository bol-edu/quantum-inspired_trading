#pragma once
#include "hls_stream.h"
#define physical_bits 19
#define currencies 5
typedef float dcal_t;
typedef hls::stream<dcal_t> spinStream;
typedef hls::stream<bool> boolStream;

typedef union {
	unsigned int u;
	float f;
} to_uint;

// give each pair an unique id
//[id*2/id*2+1][0/1]
//id*2 => bid id*2+1 => ask
const int exch_index2id[physical_bits - 1][2] = {
    {1, 3}, {3, 1}, {0, 4}, {4, 0}, {3, 2}, {2, 3}, {1, 2}, {2, 1}, {0, 2},
    {2, 0}, {3, 0}, {0, 3}, {1, 0}, {0, 1}, {1, 4}, {4, 1}, {4, 2}, {2, 4}};

// given id, get currency
const char cur_id_lst[][4] = {"USD", "EUR", "JPY", "GBP", "CHF"};

// extern "C" {
// void ERM(int index, float logged_price, float M1, float M2,
//          float J[physical_bits][physical_bits]);
// }
