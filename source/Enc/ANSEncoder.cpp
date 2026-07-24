#include "ANSEncoder.h"

ANSEncoder::ANSEncoder(Context *context) {
    this->context = context;
    // Init with initial state
    this->currentState = context->tables[context->currentTableIdx].getFirstState();
}

void ANSEncoder::encodeBin(const uint8_t bin) {
    context->tables[context->currentTableIdx].encode(currentState, bin, writer);
    context->count(bin);
    context->update(writer, currentState);
}

std::vector<uint8_t> &ANSEncoder::finishEncoding() {
    writer.write(16, context->totalCount);
    writer.write(context->rangeBits, context->currentTableIdx);
    writer.write(context->stateBits, currentState);

    const uint8_t offset = writer.flush();
    writer.bitstream.push_back(offset);
    return writer.bitstream;
}

void ANSEncoder::encodeBins(const uint32_t bins, const uint32_t numBins) {
    int remBins = static_cast<int>(numBins) - 1;
    while (remBins >= 0) {
        const uint32_t bit = (bins >> remBins) & 1; // Shift to make the remBin the lsb and take lsb
        encodeBin(bit); // Encode
        remBins--;
    }
}