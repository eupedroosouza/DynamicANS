#pragma once

#include <condition_variable>
#include <mutex>
#include <queue>

inline unsigned bit_length(const unsigned int x) {
    if (x == 0) {
        return 0;
    }
    // 32 is the total bits in an int. __builtin_clz counts leading zeros.
    return 32 - __builtin_clz(x);
}


// Thread-safe queue
template<typename T>
class ThreadSafeQueue {
    std::queue<T> m_queue;
    std::mutex m_mutex;
    std::condition_variable m_cond;

public:
    void push(T item) {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_queue.push(item);
        m_cond.notify_one();
    }

    T pop() {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cond.wait(lock,
                    [this]() { return !m_queue.empty(); });
        T item = m_queue.front();
        m_queue.pop();
        return item;
    }

    bool empty() {
        return m_queue.empty();
    }
};
