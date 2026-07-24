#pragma once

inline unsigned bit_length(const unsigned int x) {
    if (x == 0) {
        return 0;
    }
    // 32 is the total bits in an int. __builtin_clz counts leading zeros.
    return 32 - __builtin_clz(x);
}
