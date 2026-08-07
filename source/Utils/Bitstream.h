#pragma once
#include <cstdint>
#include <stdexcept>
#include <vector>

#include "Stats.h"

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
        for (int i = size - 1; i >= 0; i--) {
            const uint8_t bit = (bits >> i) & 1;

            ptr--;
            currentBitstream = currentBitstream | (bit << ptr);

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
    uint8_t flush() {
        uint8_t offset = 0;
        if (ptr < 8) {
            bitstream.push_back(currentBitstream);
            offset = ptr;
            currentBitstream = 0;
            ptr = 8;
        }
        return offset;
    }
};

class BitstreamReader {
    uint32_t buffer = 0;
    unsigned int count = 0;

    /**
     * Load the buffer with 4 bitstream
     */
    void refill() {
        while (count <= 24 && !bitstream.empty()) {
            const uint8_t nextBits = bitstream.back();
            bitstream.pop_back();
            buffer = buffer | (static_cast<uint32_t>(nextBits) << count);
            count += 8;
        }
    }

public:
    std::vector<uint8_t> bitstream = {};

    BitstreamReader() = default;

    explicit BitstreamReader(std::vector<uint8_t> bs, const std::uint8_t offset) : bitstream(std::move(bs)) {
        this->refill();
        if (offset > 0 && offset < 8) {
            this->read(offset);
        }
    }

    /**
     * Advance (remove) n bits from bitstream
     * @param n number on bits
     */
    uint32_t read(const unsigned int n) {
        if (n == 0) {
            return 0;
        }
        if (n > 31) {
            throw std::invalid_argument("Can't read most 32 bits from bitstreams");
        }


        if (count < n) {
            this->refill();

            if (count < n) {
                throw std::runtime_error("Bitstream underflow (not has " + std::to_string(n) + " bits to read)");
            }
        }

        const uint32_t mask = (1U << n) - 1;
        const uint32_t data = buffer & mask;

        buffer = buffer >> n;
        count -= n;

        return data;
    }

    [[nodiscard]] bool hasBits(const unsigned int n) const {
        return (count + (bitstream.size() * 8)) >= n;
    }
};
