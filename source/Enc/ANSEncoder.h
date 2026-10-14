#pragma once

#include "Common/Context.h"

class ANSEncoder {
    Context *context = nullptr;;
    uint32_t adaptiveInterval = 0;

    std::vector<uint32_t> counts = std::vector<uint32_t>(2);
    uint32_t totalCount = 0;

    size_t currentTableIdx = 0;
    uint32_t currentState = 0;

    BitstreamWriter writer = BitstreamWriter();
    std::deque<std::vector<uint8_t> > buffer;

    void update(uint8_t bin);

    void flush();

    [[nodiscard]] Table *getCurrentTable() const;
public:
    ~ANSEncoder() = default;

    ANSEncoder() = default;

    explicit ANSEncoder(Context *context, uint32_t adaptiveInterval);

    void encodeBin(uint8_t bin);

    void encodeBins(uint32_t bins, uint32_t numBins);

    std::deque<std::vector<uint8_t> > &finishEncoding();
};
