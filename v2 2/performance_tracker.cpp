#include "performance_tracker.h"
#include <iostream>

std::chrono::high_resolution_clock::time_point PerformanceTracker::beginTiming() {
    return std::chrono::high_resolution_clock::now();
}

void PerformanceTracker::endTiming(const std::chrono::high_resolution_clock::time_point& start, const std::string& task) {
    auto finish = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = finish - start;
    std::cout << task << " took: " << duration.count() << " seconds" << std::endl;
}