#pragma once
#include <iostream>
#include <vector>

template <typename T>
void debugVector(const std::vector<T> &elements) {
    std::cout << "[";
    for (size_t i = 0; i < elements.size(); ++i) {
        std::cout << std::to_string(elements[i]);
        if (i != elements.size() - 1) {
            std::cout << ", ";
        }
    }
    std::cout << "]\n";
}

