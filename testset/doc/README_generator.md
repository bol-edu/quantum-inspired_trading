# PCAP generator
## Required packages
- `scapy`
- `pandas`

## Usage
```shell
>> python pcap_gen.py -h
# prints the help message
>> python pcap_gen.py -o data_gen.pcap r --csv ./data/data.csv
# generate data_gen.pcap from a given csv file
>> python pcap_gen.py g --req_arb --output_csv data_gen.csv
# generates a random data and output the csv file of generated data
```

## Format of csv file
- Timestamp: NANOSECONDS since 1970/1/1 00:00.000000000
- MDEntryType: 48 for `bid` and 49 for `ask`
- SecurityID: security id predefined on your platform
- MDEntryPx: Asset price*10000000
## Note
1. The generated data has information of 18 exchange pair which its exchange rate range from 0.0001 to 10000 randomly.
2. The `pcap_gen.py` takes `./data/cme_input_arb.pcap` as a template of pcap generation, hence this file is required during the generation process.

The Example data has an simple arbitrage opportunity:

This is part of configuration in `demo_setup.cfg`. The corresponding symbolIndex of Security 1024 is 0, 1 for Security 2048 and so on.
```
feedhandler add 1024
feedhandler add 2048
feedhandler add 3072
feedhandler add 4096
feedhandler add 5120
feedhandler add 6144
feedhandler add 7168
feedhandler add 8192
feedhandler add 9216
feedhandler add 10240
feedhandler add 305419896
```

This is part of c++ code on AAT defining relation of sybolIndex and currency pair.
```c++
// give each pair an unique id
// [id*2 / id*2 + 1][0/1]
//  id*2   => bid
//  id*2+1 => ask
const int exch_index2id[PHYSICAL_BITS][2] = {
    {1, 3}, {3, 1}, {0, 4}, {4, 0}, {3, 2}, {2, 3}, {1, 2}, {2, 1}, {0, 2},
    {2, 0}, {3, 0}, {0, 3}, {1, 0}, {0, 1}, {1, 4}, {4, 1}, {4, 2}, {2, 4}};

// given id, get currency
const char cur_id_lst[NUM_CURRENCIES][4] = {"USD", "EUR", "JPY", "GBP", "CHF"};
```

- securityID: 7168/ask (symbolIndex 6) => `{0,1}`(USD,EUR) with price `1.1`
- securityID: 4096/bid (symbolIndex 3) => `{1,2}`(EUR,JPY) with price `1.2`
- securityID: 5120/ask (symbolIndex 4) => `{2,0}`(JPY,USD) with price `1.3`
