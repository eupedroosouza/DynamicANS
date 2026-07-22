#pragma once

#include <cstdint>
#include <vector>

class Table;

class ContextModeler {
    std::vector<Table> tables;
    std::vector<uint32_t> counts = std::vector<uint32_t>(2);

    Table *currentTable = nullptr;

    ~ContextModeler() = default;

    explicit ContextModeler(std::vector<Table> tables) : tables(std::move(tables)) {

    }


public:

    void count(const uint8_t symbol) {
        this->counts[symbol] += 1;
    }

    void update() {
        // A = 0, B = 1
        const uint32_t countA = counts[0];
        const uint32_t countB = counts[1];

        uint32_t newFreqA = countA / (countA + countB);

    }

    [[nodiscard]] Table * getCurrentTable() const {
        return currentTable;
    }

};
