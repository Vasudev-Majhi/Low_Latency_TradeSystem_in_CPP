#ifndef PERFORMANCE_TRACKER_H
#define PERFORMANCE_TRACKER_H

#include <chrono>
#include <string>

class PerformanceTracker {
public:
    static std::chrono::high_resolution_clock::time_point beginTiming();
    static void endTiming(const std::chrono::high_resolution_clock::time_point& start, const std::string& task);
};

#endif // PERFORMANCE_TRACKER_H