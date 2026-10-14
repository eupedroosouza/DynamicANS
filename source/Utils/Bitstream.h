#pragma once
#include <bitset>
#include <cstdint>
#include <vector>

static constexpr uint8_t BIT_MASK[9] = {
    0b00000000, // 0 bits
    0b00000001, // 1 bit
    0b00000011, // 2 bits
    0b00000111, // 3 bits
    0b00001111, // 4 bits
    0b00011111, // 5 bits
    0b00111111, // 6 bits
    0b01111111, // 7 bits
    0b11111111 // 8 bits
};

class BitstreamWriter {
    uint8_t ptr = 8;
    uint8_t currentBitstream = 0;

public:
    // Using 8 bit bitstream and implemented as stack because of ANS characteristic (decode is inversed of encode)
    std::vector<uint8_t> bitstream = {};

    BitstreamWriter() = default;

    ~BitstreamWriter() = default;

    /**
     * Write bits on bitstream
     * @param size size of bitstream
     * @param bits bitstream (0bXXX)
     */
    void write(const uint8_t size, const uint32_t bits) {
        uint8_t count = size;
        while (count > 0) {
            const uint8_t sizeToWrite = std::min(count, ptr);
            count -= sizeToWrite;

            const uint8_t bitsToWrite = (bits >> count) & BIT_MASK[sizeToWrite];
            currentBitstream = currentBitstream | (bitsToWrite << (ptr - sizeToWrite));

            ptr -= sizeToWrite;

            if (ptr == 0) {
                bitstream.push_back(currentBitstream);
                currentBitstream = 0;
                ptr = 8;
            }
        }
    }

    /**
     * Flush bitstream
     * Call it when work with bitstream was ended
     * @return offset to read bitstream
     */
    void flush() {
        if (ptr < 8) {
            uint8_t offset = 0;
            bitstream.push_back(currentBitstream);
            offset = ptr;
            currentBitstream = 0;
            ptr = 8;
            // Renormalizing vector
            for (uint32_t i = (bitstream.size() - 1); i > 0; i--) {
                bitstream[i] = bitstream[i] >> offset;
                const uint32_t leftNeighborIdx = i - 1;
                const uint8_t left = bitstream[leftNeighborIdx];
                uint8_t value = left & BIT_MASK[offset];
                value = value << (8 - offset);
                bitstream[i] = bitstream[i] | value;
            }
            bitstream[0] = bitstream[0] >> offset;
        }
    }
};

class BitstreamReader {
    uint64_t buffer = 0;
    uint32_t count = 0;

    /**
     * Load the buffer with 4 bitstream
     */
    void refill() {
        while (count <= 56 && !bitstream.empty()) {
            const uint8_t nextBits = bitstream.back();
            bitstream.pop_back();
            buffer = buffer | (static_cast<uint64_t>(nextBits) << count);
            count += 8;
        }
    }

public:
    std::vector<uint8_t> bitstream = {};

    BitstreamReader() = default;

    explicit BitstreamReader(std::vector<uint8_t> bs) : bitstream(std::move(bs)) {
        this->refill();
    }

    /**
     * Advance (remove) n bits from bitstream
     * @param n number on bits
     */
    uint32_t advance(const uint32_t n) {
        if (count < n) {
            this->refill();
        }

        const uint32_t mask = (1U << n) - 1;
        const uint32_t data = buffer & mask;

        buffer = buffer >> n;
        count -= n;

        return data;
    }
};
