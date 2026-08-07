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
    writer.write(context->rangeBits, currentState);

    const uint8_t offset = writer.flush();
    writer.bitstream.push_back(offset);

    Stats::get().miscBits += (16 + 8 + (2 * context->rangeBits));
    Logger::get().log(" Finish encoding: ");
    Logger::get().log("  Final count: " + std::to_string(context->totalCount));
    Logger::get().log("  Final table: " + std::to_string(context->currentTableIdx));
    Logger::get().log("  Final state: " + std::to_string(currentState));
    Logger::get().log("  Bitstream offset: " + std::to_string(offset));


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