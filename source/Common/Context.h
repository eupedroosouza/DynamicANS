#pragma once

#include <cmath>
#include <fstream>
#include <vector>
#include "TabledANS.h"
#include "Utils/Log.h"
#include "Utils/Util.h"


class Context {
public:
    std::vector<Table> tables;
    uint32_t L = 0;
    uint32_t rangeBits = 0;

    ~Context() = default;

    Context() = default;

    explicit Context(std::vector<Table> tables, const uint32_t L) : tables(
        std::move(tables)) {
        this->L = L;
        this->rangeBits = bit_length(L - 1); // remove - 1 to normalize index and save bits
    }

    static Context loadContextFromFile(const std::string &filename) {
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            throw std::runtime_error("Could not open file: " + filename);
        }

        uint32_t L;
        uint32_t size;
        file.read(reinterpret_cast<char *>(&L), sizeof(L));
        file.read(reinterpret_cast<char *>(&size), sizeof(size));

        std::vector<Table> ctxTables;

        for (uint32_t i = 0; i < size; i++) {
            uint8_t symbolsSize;
            file.read(reinterpret_cast<char *>(&symbolsSize), sizeof(symbolsSize));
            std::vector<uint32_t> alphabet;
            for (int s = 0; s < symbolsSize; s++) {
                uint32_t freq;
                file.read(reinterpret_cast<char *>(&freq), sizeof(freq));
                alphabet.push_back(freq);
            }
            std::list<State> states;
            //const uint32_t M = static_cast<uint16_t>(2 * L) - 1;
            for (uint32_t state = 0; state < L; state++) {
                std::vector<uint32_t> nextStates = {};
                nextStates.resize(symbolsSize);
                std::vector<StateBitstream> bitstreams = {};
                bitstreams.resize(symbolsSize);
                for (uint8_t symbol = 0; symbol < symbolsSize; symbol++) {
                    uint32_t nextState;
                    file.read(reinterpret_cast<char *>(&nextState), sizeof(nextState));
                    uint32_t bitstreamSize;
                    file.read(reinterpret_cast<char *>(&bitstreamSize), sizeof(bitstreamSize));
                    uint32_t bitstream;
                    file.read(reinterpret_cast<char *>(&bitstream), sizeof(bitstream));
                    const auto stateBitstream = StateBitstream(bitstreamSize, bitstream);
                    nextStates[symbol] = nextState;
                    bitstreams[symbol] = stateBitstream;
                }
                states.emplace_back(state, nextStates, bitstreams);
            }

            const auto table = Table(L, alphabet, states);
            ctxTables.push_back(table);
        }


        Logger::get().log("Loaded " + std::to_string(ctxTables.size()) + " tables on memory.");

        return Context(ctxTables, L);
    }
};
