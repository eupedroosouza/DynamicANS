#include "ANSEncoder.h"


ANSEncoder::ANSEncoder(Context *context, const uint32_t adaptiveInterval) {
    this->context = context;
    this->adaptiveInterval = adaptiveInterval;
    // Start first table with the most balanced distribution (50/50 frequencies)
    this->currentTableIdx = this->context->tables.size() / 2;
    this->currentState = getCurrentTable()->getFirstState();
}

Table *ANSEncoder::getCurrentTable() const {
    return &context->tables[currentTableIdx];
}

void ANSEncoder::encodeBin(const uint8_t bin) {
    getCurrentTable()->encode(currentState, bin, writer);
    update(bin);
}

void ANSEncoder::encodeBins(const uint32_t bins, const uint32_t numBins) {
    int remBins = static_cast<int>(numBins) - 1;
    while (remBins >= 0) {
        const uint32_t bit = (bins >> remBins) & 1; // Shift to make the remBin the lsb and take lsb
        encodeBin(bit); // Encode
        remBins--;
    }
}

void ANSEncoder::update(const uint8_t bin) {
    // Count of symbols
    this->counts[bin] = this->counts[bin] + 1;
    totalCount++;

    // If not was encoded adaptiveInterval yet, skip the update model
    if (totalCount < adaptiveInterval) {
        return;
    }

    // Finalizing model and add to  buffer
    this->flush();

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
}

void ANSEncoder::flush() {
    // Steps:
    //  1. Save current state to bitstream
    //  2. Renormalize bitstream to work as a LIFO
    //  3. Push to a buffer
    writer.write(context->rangeBits, currentState);
    writer.flush();
    this->buffer.push_back(writer.bitstream);
}


std::deque<std::vector<uint8_t> > &ANSEncoder::finishEncoding() {
    // Flush last adaptive interval
    this->flush();
    return buffer;
}
