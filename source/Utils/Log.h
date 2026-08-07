#pragma once
#include <atomic>
#include <fstream>
#include <string>
#include <thread>

#include "Util.h"


class Logger {

    static inline std::unique_ptr<Logger> instance = nullptr;

    std::thread thread;
    std::ofstream stream;

    ThreadSafeQueue<std::string> queue = {};
    std::atomic<bool> done = std::atomic<bool>(false);

    void print();

public:
    Logger() = default;

    explicit Logger(const std::string &fileName);

    ~Logger() = default;

    void log(const std::string& msg);

    void close();

    static void init(const std::string& fileName) {
        if (!instance) {
            instance = std::make_unique<Logger>(fileName);
        }
    }

    static Logger& get() {
        return *instance;
    }
};

inline Logger& getLogger() {
    static Logger instance;
    return instance;
}