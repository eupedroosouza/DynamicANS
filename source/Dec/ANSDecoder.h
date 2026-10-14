#pragma once
#include "Common/Context.h"


class ANSDecoder {
    Context *context = nullptr;
    uint32_t adaptiveInterval = 0;

    std::deque<std::vector<uint8_t> > buffer;

    std::vector<uint32_t> counts = std::vector<uint32_t>(2);
    uint32_t totalCount = 0;

    size_t currentTableIdx = 0;
    BitstreamReader reader;

    void unflush();

    void check(uint8_t bin);

    [[nodiscard]] Table *getCurrentTable() const;

public:
    uint32_t currentState = 0;


    ~ANSDecoder() = default;

    ANSDecoder() = default;

    explicit ANSDecoder(Context *context, uint32_t adaptiveInterval, std::deque<std::vector<uint8_t> > buffer);

    uint8_t decodeBin();

    uint32_t decodeBins(uint32_t numBins);
};
