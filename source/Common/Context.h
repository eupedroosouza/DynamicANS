#pragma once

#include <cmath>
#include <cstdint>
#include <fstream>
#include <vector>
#include "TabledANS.h"
#include "Utils/Log.h"
#include "Utils/Util.h"


class Context {
    uint32_t L = 0;
    std::vector<uint32_t> counts = std::vector<uint32_t>(2);

public:
    std::vector<Table> tables;
    uint32_t rangeBits = 0;
    uint32_t adaptativeBits = 0;
    //uint32_t stateBits = 0;
    uint32_t totalCount = 0;
    uint32_t adaptativeInterval = 0;

    size_t currentTableIdx = 0;

    ~Context() = default;

    Context() = default;

    explicit Context(std::vector<Table> tables, const uint32_t L, const uint32_t adaptativeInterval) : tables(
        std::move(tables)) {
        this->L = L;
        this->rangeBits = bit_length(L - 1); // remove - 1 to normalize index and save bits
        this->adaptativeBits = 2 * rangeBits;
        //this->stateBits = bit_length((2 * this->L) - 1);
        this->adaptativeInterval = adaptativeInterval;
        // The idea is try to pick the middle table, which is one closest to P(A) = 0.5 (or is P(A) 0.5).
        currentTableIdx = this->tables.size() / 2;
        Logger::get().log("Context configuration:");
        Logger::get().log(" State bit length: " + std::to_string(rangeBits));
        Logger::get().log(" Adaptive interval bit length (state + table): " + std::to_string(adaptativeBits));
    }

    void count(const uint8_t symbol) {
        this->counts[symbol] = this->counts[symbol] + 1;
        totalCount++;
    }

    // Used on encode to update model (for decode use check(reader,currentState))
    void update(BitstreamWriter &writer, uint32_t &currentState) {
        if (totalCount < adaptativeInterval) {
            // no update
            return;
        }
        Logger::get().log("Updating model (" + std::to_string(totalCount) + " bits encoded): ");
        // A = 0, B = 1
        const uint32_t countA = counts[0];
        const uint32_t countB = counts[1];
        Logger::get().log(" Counts:");
        Logger::get().log("  A (0): " + std::to_string(countA));
        Logger::get().log("  B (1): " + std::to_string(countB));
        Logger::get().log("  Total: " + std::to_string(totalCount));

        // Applying Bayes with Laplace smoothing to avoid 0 probability
        const double probA = static_cast<double>(countA + 1) / ((countA + countB) + 2);


        // Min Freq: 1 (when the probA is less than 1/range), Max Freq: range - 1 (when the probA is greater than range-1/range) - [e.g: range = 8, max freq for A = 7]
        const uint32_t freqA = std::max(1, std::min(static_cast<int>(L - 1), static_cast<int>(std::round(probA * L))));

        // Flush current state to bitstream
        // It's needed because is necessary recover which table was defined on decode since the encoding occurs inversed
        writer.write(rangeBits, currentState);
        writer.write(rangeBits, currentTableIdx);
        Logger::get().log(" Save actual table: ");
        Logger::get().log("  State: " + std::to_string(currentState));
        Logger::get().log("  Table (frequency): " + std::to_string(currentTableIdx) + " (" + std::to_string(currentTableIdx + 1) + ")");
        Stats::get().stateBits += rangeBits;
        Stats::get().tableBits += rangeBits;
        Stats::get().adaptiveBits += adaptativeBits;
        // Pick new table based on freqA
        const uint32_t idx = freqA - 1; // - 1 because starts with 0
        currentTableIdx = idx;
        currentState = this->tables[currentTableIdx].getFirstState();

        Logger::get().log(" Updating to new context: ");
        Logger::get().log("  New probability of A (0): " + std::to_string((probA)));
        Logger::get().log("  New frequency of A (0): " + std::to_string(freqA));
        Logger::get().log("  New table (index): " + std::to_string(currentTableIdx));
        Logger::get().log("  New state: " + std::to_string(currentState));

        // Redefine counts
        counts[0] = 0;
        counts[1] = 0;
        totalCount = 0;
        Stats::get().updates++;
    }

    void check(BitstreamReader &reader, uint32_t &currentState) {
        if (totalCount > 0) {
            // no update
            return;
        }

        // Check if was bits to read yet
        if (!reader.hasBits(adaptativeBits)) {
            return;
        }

        currentTableIdx = reader.read(rangeBits);
        currentState = reader.read(rangeBits);

        // Redefine counts
        totalCount = adaptativeInterval;
    }

    void clear() {
        counts[0] = 0;
        counts[1] = 0;
        totalCount = 0;
        currentTableIdx = 0;
    }

    static Context loadContextFromFile(const std::string &filename, const uint32_t adaptativeInterval) {
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

        return Context(ctxTables, L, adaptativeInterval);
    }
};
