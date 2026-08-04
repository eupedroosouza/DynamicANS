    #include "ANSDecoder.h"

    ANSDecoder::ANSDecoder(Context *context, std::vector<uint8_t> bytestream) {
        this->context = context;
        const uint8_t offset = bytestream.back();
        bytestream.pop_back();
        this->reader = BitstreamReader(std::move(bytestream), offset);
        this->currentState = reader.read(context->stateBits);
        this->context->currentTableIdx = reader.read(context->rangeBits);
        this->context->totalCount = reader.read(16);
        this->context->check(reader, currentState);
    }

    uint8_t ANSDecoder::decodeBin() {
        const uint8_t bin = this->context->tables[this->context->currentTableIdx].decode(currentState, reader);
        this->context->totalCount--;
        this->context->check(reader, currentState);
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