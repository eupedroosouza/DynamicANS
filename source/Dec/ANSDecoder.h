#pragma once
#include "Common/Context.h"


class ANSDecoder {
    Context *context = nullptr;
    BitstreamReader reader;



public:
    uint32_t currentState = 0;

    ~ANSDecoder() = default;

    ANSDecoder() = default;

    explicit ANSDecoder(Context *context, std::vector<uint8_t> bytestream);

    uint8_t decodeBin();

    uint32_t decodeBins(uint32_t numBins);
};
