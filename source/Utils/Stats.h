#pragma once
#include <memory>

class Stats {
    static inline std::unique_ptr<Stats> instance = nullptr;

public:
    uint32_t updates = 0;
    uint32_t bitstreamBits = 0;
    uint32_t stateBits = 0;
    uint32_t tableBits = 0;
    uint32_t adaptiveBits = 0;
    uint32_t miscBits = 0; // final state, count, offset, etc...

    static void init() {
        if (!instance) {
            instance = std::make_unique<Stats>();
        }
    }

    static Stats &get() {
        return *instance;
    }
};
