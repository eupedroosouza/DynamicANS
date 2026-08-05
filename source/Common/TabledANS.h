#pragma once
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <list>

#include "Utils/Bitstream.h"


class StateBitstream {
public:
    uint32_t size = 0;
    uint32_t bitstream = 0;

    StateBitstream() = default;

    explicit StateBitstream(const uint32_t size, const uint32_t bitstream) : size(size), bitstream(bitstream) {
    }

    ~StateBitstream() = default;
};

class State {
public:
    uint32_t state = 0;
    std::vector<uint32_t> nextStates = {};
    std::vector<StateBitstream> bitstreams = {};

    State() = default;

    explicit State(const uint32_t state, const std::vector<uint32_t> &nextStates,
                   const std::vector<StateBitstream> &bitstreams) : state(state), nextStates(nextStates),
                                                                    bitstreams(bitstreams) {
    }

    ~State() = default;
};

class DecodeState {
public:
    std::uint8_t symbol = 255; //
    std::uint32_t N = 0;
    std::uint32_t base = 0;

    DecodeState() = default;

    explicit DecodeState(const std::int32_t symbol, const std::uint32_t N, const std::uint32_t base) : symbol(symbol),
        N(N),
        base(base) {
    }

    ~DecodeState() = default;
};


class Table {
    uint32_t L = 0;

    std::vector<State> states = {};
    std::vector<DecodeState> decodeStates = {};

public:
    std::vector<uint32_t> alphabet;

    Table() = default;

    explicit Table(const uint32_t L, const std::vector<uint32_t> &alphabet, const std::list<State> &states) {
        this->L = L;
        this->alphabet = alphabet;

        this->states.resize(L);
        for (const State &state: states) {
            this->states.at(state.state) = state;
        }

        this->decodeStates.resize(L);
        for (const auto &state: this->states) {
            for (uint8_t symbol = 0; symbol < static_cast<uint8_t>(state.nextStates.size()); ++symbol) {
                const uint32_t &nextState = state.nextStates[symbol];
                if (state.bitstreams.empty() && this->decodeStates[nextState].symbol == 255) {
                    this->decodeStates.at(nextState) = DecodeState(symbol, 0, state.state);
                    continue;
                }
                const StateBitstream &stateBitstream = state.bitstreams.at(symbol);
                if (this->decodeStates.at(nextState).symbol == 255) {
                    this->decodeStates.at(nextState) = DecodeState(symbol, stateBitstream.size, state.state);
                } else {
                    DecodeState &decodeState = this->decodeStates.at(nextState);
                    decodeState.base = std::min(decodeState.base, state.state);
                }
            }
        }
    }

    ~Table() = default;

    /**
     * The first state to that table (initialize you state variable with that value)
     * @return the first state
     */
    [[nodiscard]] uint32_t getFirstState() const {
        return 0;
    }

    /**
     * Encoded symbol and write on bitstream
     * @param currentState the current state
     * @param symbol the symbol will be encoded
     * @param writer the bitstream writer
     * @return new state (as unsigned 16 bits)
     */
    void encode(uint32_t &currentState, const uint8_t symbol, BitstreamWriter &writer) const {
        const State &state = states[currentState];
        const uint32_t &nextState = state.nextStates.at(symbol);
        const StateBitstream &bitstream = state.bitstreams.at(symbol);
        writer.write(bitstream.size, bitstream.bitstream);
        currentState = nextState;
    }

    /**
     * Decode the simbol and write on bitstream
     * @param currentState the current state
     * @param reader the bitstream reader
     * @return a pair of previous state and symbol value
     */
    uint8_t decode(uint32_t &currentState, BitstreamReader &reader) const {
        const DecodeState &decodeState = this->decodeStates[currentState];

        // A state without a bitstream
        if (decodeState.N == 0) {
            currentState = decodeState.base;
            return decodeState.symbol;
        }

        const auto readBitstreams = reader.read(decodeState.N);
        currentState = decodeState.base + readBitstreams;
        return decodeState.symbol;
    }
};