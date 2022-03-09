import argparse
import pandas as pd
import numpy as np
# 18 total
exch_index2id = np.array([[1, 3], [3, 1], [0, 4], [4, 0], [3, 2], [2, 3], [1, 2], [2, 1], [
                         0, 2], [2, 0], [3, 0], [0, 3], [1, 0], [0, 1], [1, 4], [4, 1], [4, 2], [2, 4]], dtype=int)

rate2pair = {}

symbols = ["USD", "EUR", "JPY", "GBP", "CHF"]

def print_orders(raw):
    # input is a binary string
    pkt_num = len(raw) // 256 # 256 bytes per order entry
    for i in range(pkt_num):
        # split into orders
        order = raw[i*256:(i+1)*256]
        # extract rate information
        rate = int(order[157:167].decode())
        print(f"order{i}: {rate}")
    # exch_index_start, exch_index_to = rate2pair[rate]
    # print(exch_index_start, exch_index_to)


def construct_map(df):
    px = df["MDEntryPx"]
    exch_index = df["SecurityID"] // 1024 - 1  # get exch_index
    for i in range(df.shape[0]):
        rate2pair[px[i]] = exch_index[i]
    # print(rate2pair)


def parse_arg():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        '-c', '--csv', help="path of csv file", required=True)
    parser.add_argument(
        '-e', '--orderentry', help="path of orderentry output", required=True)
    args = parser.parse_args()
    return args


if __name__ == '__main__':
    args = parse_arg()
    f = open(args.orderentry, "rb")
    raw = f.read()
    f.close()
    df = pd.read_csv(args.csv)
    construct_map(df)
    print_orders(raw)

