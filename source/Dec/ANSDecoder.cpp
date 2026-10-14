#include "ANSDecoder.h"

#include "Enc/ANSEncoder.h"

ANSDecoder::ANSDecoder(Context *context, const uint32_t adaptiveInterval, std::deque<std::vector<uint8_t> > buffer) : buffer(std::move(buffer)) {
    this->context = context;
    this->adaptiveInterval = adaptiveInterval;
    this->currentTableIdx = this->context->tables.size() / 2;
    this->unflush();
}

void ANSDecoder::unflush() {
    const std::vector<uint8_t> bs = this->buffer.front();
    this->buffer.pop_front();
    this->reader = BitstreamReader(bs);
    this->currentState = reader.advance(context->rangeBits);
}


void ANSDecoder::check(const uint8_t bin) {
    // Count of symbols
    this->counts[bin] = this->counts[bin] + 1;
    totalCount++;

    // If not was encoded adaptiveInterval yet, skip the update model
    if (totalCount < adaptiveInterval) {
        return;
    }

    // Updating model
    // A = 0, B = 1
    const uint32_t countA = counts[0];
    const uint32_t countB = counts[1];

    // Applying Bayes with Laplace smoothing to avoid 0 probability
    const double probA = static_cast<double>(countA + 1) / ((countA + countB) + 2);

    // Min Freq: 1 (when the probA is less than 1/range), Max Freq: range - 1 (when the probA is greater than range-1/range) - [e.g: range = 8, max freq for A = 7]
    const uint32_t freqA = std::max(1, std::min(static_cast<int>(context->L - 1),
                                                static_cast<int>(std::round(probA * context->L))));

    // Flush current state to bitstream
    // It's needed because is necessary recover which table was defined on decode since the encoding occurs inversed
    // Pick new table based on freqA
    const uint32_t idx = freqA - 1; // - 1 because starts with 0
    currentTableIdx = idx;

    // Redefine counts
    counts[0] = 0;
    counts[1] = 0;
    totalCount = 0;

    // Move to the next adaptation chunk
    this->unflush();
}

Table *ANSDecoder::getCurrentTable() const {
    return &context->tables[currentTableIdx];
}


uint8_t ANSDecoder::decodeBin() {
    const uint8_t bin = getCurrentTable()->decode(currentState, reader);
    return bin;
}

uint32_t ANSDecoder::decodeBins(const uint32_t numBins) {
    uint32_t num = 0;
    for (uint32_t pos = 0; pos < numBins; pos++) {
        const uint32_t bit = decodeBin();
        num = num | (bit << pos);
    }
    return num;
}
