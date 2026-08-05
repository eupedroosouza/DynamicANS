// Cross-platform RSS query (bytes)
// https://github.com/Jiovana/StaticBAC/blob/main/source/Lib/Test/test_encmodel.cpp
#pragma once

#include <atomic>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>

#if defined(_WIN32)
  #include <windows.h>
  #include <psapi.h>
#elif defined(__APPLE__)
  #include <mach/mach.h>
#endif


static size_t getCurrentRSS()
{
#if defined(_WIN32)
    // Windows: use GetProcessMemoryInfo
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
        return static_cast<size_t>(pmc.WorkingSetSize);
    return 0;

#elif defined(__linux__)
    // Linux: read /proc/self/status VmRSS
    std::ifstream f("/proc/self/status");
    std::string line;
    while (std::getline(f, line))
    {
        if (line.rfind("VmRSS:", 0) == 0)
        {
            std::istringstream iss(line);
            std::string key;
            size_t kb;
            iss >> key >> kb;
            return kb * 1024;
        }
    }
    return 0;

#elif defined(__APPLE__)
    struct mach_task_basic_info info;
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
                  reinterpret_cast<task_info_t>(&info), &count) == KERN_SUCCESS)
        return static_cast<size_t>(info.resident_size);
    return 0;
#else
    return 0;
#endif
}

static double toMB(const size_t b) {
    return static_cast<double>(b) / (1024.0 * 1024.0);
}


struct MemoryStats
{
    size_t baselineBytes = 0;   // RSS before the call
    size_t peakBytes     = 0;   // peak RSS during the call
    size_t deltaBytes    = 0;   // peak - baseline
};

class PeakMemorySampler
{
public:
    // interval_ms: sampling interval (default 10 ms, matches Python)
    explicit PeakMemorySampler(int interval_ms = 10)
        : m_interval(interval_ms), m_done(false), m_peak(0) {}

    // Call just before the workload
    void start()
    {
        m_done = false;
        m_peak = getCurrentRSS();
        m_thread = std::thread([this]()
        {
            while (!m_done.load(std::memory_order_relaxed))
            {
                size_t rss = getCurrentRSS();
                size_t cur = m_peak.load(std::memory_order_relaxed);
                while (rss > cur &&
                       !m_peak.compare_exchange_weak(cur, rss,
                           std::memory_order_relaxed))
                {}
                std::this_thread::sleep_for(m_interval);
            }
        });
    }

    // Call just after the workload; returns filled MemoryStats
    MemoryStats stop(size_t baseline)
    {
        m_done.store(true, std::memory_order_relaxed);
        if (m_thread.joinable())
            m_thread.join();

        MemoryStats s;
        s.baselineBytes = baseline;
        s.peakBytes     = m_peak.load();
        s.deltaBytes    = (s.peakBytes > s.baselineBytes)
                              ? s.peakBytes - s.baselineBytes
                              : 0;
        return s;
    }

private:
    std::chrono::milliseconds  m_interval;
    std::atomic<bool>          m_done;
    std::atomic<size_t>        m_peak;
    std::thread                m_thread;
};

// Convenience: print a MemoryStats block with a label
static void printMemStats(const std::string& label, const MemoryStats& ms)
{
    std::cout << label << "\n"
              << "  baseline : " << toMB(ms.baselineBytes) << " MB\n"
              << "  peak     : " << toMB(ms.peakBytes)     << " MB\n"
              << "  delta    : " << toMB(ms.deltaBytes)     << " MB\n";
}