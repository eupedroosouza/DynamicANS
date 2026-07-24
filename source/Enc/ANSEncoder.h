#pragma once

#include "Common/Context.h"

class ANSEncoder {
    Context *context = nullptr;
    BitstreamWriter writer = BitstreamWriter();



public:
    uint32_t currentState = 0;

    ~ANSEncoder() = default;

    ANSEncoder() = default;

    explicit ANSEncoder(Context *context);

    void encodeBin(uint8_t bin);

    void encodeBins(uint32_t bins, uint32_t numBins);

    std::vector<uint8_t> &finishEncoding();
};
