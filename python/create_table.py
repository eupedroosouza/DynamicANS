import argparse
import csv
import os.path
import struct
from pathlib import Path

import numpy as np

class TableResult:
    def __init__(self, states: dict[int, list[int]], bitstreams: dict[int, list[tuple[int, int]]]):
        self.states = states
        self.bitstreams = bitstreams

class CreateResult:
    def __init__(self, range: int, tables: list[TableResult]):
        self.range = range
        self.tables = tables

def create_tables(m: int) -> CreateResult:

    tables: list[TableResult] = []
    for i in range(0, (m - 1), 1):
        freqA: int = i + 1
        freqB: int = m - freqA
        alphabet: dict[int, int] = {0: freqA, 1: freqB}
        cumulative = np.insert(np.cumsum(list(alphabet.values())), 0, 0).astype(np.int32).tolist()
        output_states, output_bitstreams = create_encoder_table(m, alphabet, cumulative)
        tables.append(TableResult(output_states, output_bitstreams))
        print(f"Generated table {i}.")
        pass

    return CreateResult(m, tables)
    

def save_tables_as_binary(createdTable: CreateResult, bin_file: Path | str):
    results = createdTable.tables
    path = Path(bin_file)
    if path.exists():
        raise FileExistsError(f"File {bin_file} already exists")

    with open(path, "wb") as file:

        file.write(struct.pack("<I", createdTable.range))
        file.write(struct.pack("<I", len(results)))

        for table in results:
            if not table.states or not table.bitstreams:
                raise RuntimeError(f"Table is empty.")

            first_state_key = next(iter(table.states))
            symbols_size = len(table.states[first_state_key])
            file.write(struct.pack("<B", symbols_size))

            for state, next_states in table.states.items():
                bitstream = table.bitstreams[state]
                for i, next_state in enumerate(next_states):
                    file.write(struct.pack("<H", next_state))
                    bs = bitstream[i]
                    file.write(struct.pack("<B", bs[0]))
                    file.write(struct.pack("<B", bs[1]))

    print(f"Saved binary tables on {bin_file}.")


# tANS
def create_encoder_table(total: int, alphabet: dict[int, int], cumulative: list[int]) -> tuple[
    dict[int, list[int]], dict[int, list[tuple[int, int]]]]:
    output_states: dict[int, list[int]] = {}
    output_bitstreams: dict[int, list[tuple[int, int]]] = {}
    r_max = (2 * total)
    for i_state in range(total, r_max, 1):
        symbols_states: list[int] = []
        symbols_bitstreams: list[tuple[int, int]] = []
        for s, s_freq in alphabet.items():

            state = i_state
            bitstream_size: int = 0
            bitstream: int = 0

            while state >= (2 * alphabet[s]):
                rem = state % 2
                bitstream = bitstream | (rem << bitstream_size)
                bitstream_size += 1
                state = int(state / 2)

            state = encode_rANS(total, cumulative, alphabet, s, state)

            symbols_states.append(state)
            symbols_bitstreams.append((bitstream_size, bitstream))
        output_states[i_state] = symbols_states
        output_bitstreams[i_state] = symbols_bitstreams

    return output_states, output_bitstreams



# Streaming-rANS

# That function encode all symbols input
def streaming_encode_rANS(total: int, cumulative: list[int], alphabet: dict[int, int], s_input: list[int]) -> tuple[
    int, list[int]]:
    state = total
    bitstream = []

    for s in s_input:
        while state >= (2 * alphabet[s]):
            bitstream.append(state % 2)
            state = int(state / 2)

        state = encode_rANS(total, cumulative, alphabet, s, state)  # The rANS encoding step

    return state, bitstream


# That function decode one symbol input ant return state to decode others
def streaming_decode_rANS(total: int, cumulative: list[int], alphabet: dict[int, int], final_state: int,
                          bitstream: list[int], ):
    s_decoded, state = decode_rANS(total, cumulative, alphabet, final_state)

    while state < total:
        bit = bitstream.pop()
        state = (state * 2) + bit

    return s_decoded, state


# rANS
def encode_rANS(total: int, cumulative: list[int], alphabet: dict[int, int], s: int, state: int) -> int:
    s_freq = alphabet[s]  # current symbol count/frequency
    next_state = (int((state // s_freq)) * total) + cumulative[s] + (state % s_freq)  # get rANS next state
    return next_state


def cumulative_inverse(cumulative: list[int], pos: int) -> int | None:
    # use that to optimize the search of cumulative based on position with binary search
    idx = np.searchsorted(cumulative, pos, side="right").astype(np.int32) - 1
    if 0 <= idx < len(cumulative):
        return idx
    return None


def decode_rANS(total: int, cumulative: list[int], alphabet: dict[int, int], state: int) -> tuple[int, int]:
    pos = state % total
    s = cumulative_inverse(cumulative, pos)
    s_freq = alphabet[s]
    prev_state = (int((state / total)) * s_freq) + pos - cumulative[s]  # use int(x) to avoid a float

    return s, prev_state


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--range", "-r", required=True, help="Range (M)")
    parser.add_argument("--bin", "-b", required=True, help="Binary (.bin) save file")

    args = parser.parse_args()
   

    createdTable = create_tables(int(args.range))
    bin_file = Path(args.bin)
    save_tables_as_binary(createdTable, bin_file)
        
    print("Done.")

    pass


if __name__ == "__main__":
    main()