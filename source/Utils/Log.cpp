#include "Log.h"

#include <filesystem>

Logger::Logger(const std::string &fileName) {
    const std::filesystem::path filePath(fileName);
    const std::filesystem::path dirPath = filePath.parent_path();
    if (!dirPath.empty() && !std::filesystem::exists(dirPath)) {
        std::filesystem::create_directories(dirPath);
    }

    this->stream = std::ofstream(fileName, std::ios::app);
    if (!this->stream.is_open()) {
        throw std::runtime_error("Could not open the file: " + fileName);
    }
    this->thread = std::thread([this]() {
        while (!done.load(std::memory_order_relaxed)) {
            this->print();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });
}

void Logger::print() {
    while (!queue.empty()) {
        std::string msg = queue.pop();
        stream << msg << std::endl;
    }
}

void Logger::close() {
    this->done.store(true, std::memory_order_relaxed);
    if (thread.joinable()) {
        thread.join();
    }
    this->print();
    this->stream.close();
}

void Logger::log(const std::string& msg) {
    this->queue.push(msg);
}
