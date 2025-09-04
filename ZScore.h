#ifndef ZSCORE_H
#define ZSCORE_H

#include <Arduino.h>
#include <vector>
#include <stdexcept>
#include <cmath>

class Zscore {
private:
    std::vector<double> window;
    size_t window_size;
    double previous_val=0;
    // Compute mean of the current window
    double computeMean() const {
        double sum = 0.0;
        for (double val : window) sum += val;
        return sum / window.size();
    }

    // Compute standard deviation given the mean
    double computeStdDev(double mean) const {
        if (window.size() <= 1) return 0.0;
        double sum_squares = 0.0;
        for (double val : window) {
            sum_squares += (val - mean) * (val - mean);
        }
        return std::sqrt(sum_squares / window.size());
    }

public:
    explicit Zscore(size_t size) : window_size(size) {
        if (size == 0) {
            throw std::invalid_argument("Window size must be greater than 0");
        }
        window.reserve(size);
    }

    // Add value and return its z-score based on current window
    double addAndCalculate(double value) {
      if(abs(int(previous_val-value))==1)value=previous_val;// Just avoid little flactuation by one
      previous_val=value;
        // Enforce LIFO behavior: newest in front
        if (window.size() >= window_size) {
            window.pop_back(); // remove oldest (end)
        }
        window.insert(window.begin(), value); // push front (newest)

        double mean = computeMean();
        double stddev = computeStdDev(mean);

        if (stddev == 0.0) return 0.0;
        return (value - mean) / stddev;
    }

    // Get current mean
    double getMean() const {
        if (window.empty()) return 0.0;
        return computeMean();
    }

    // Get current standard deviation
    double getStdDev() const {
        if (window.empty()) return 0.0;
        double mean = computeMean();
        return computeStdDev(mean);
    }

    // Window size limit
    size_t getWindowSize() const { return window_size; }

    // Number of values currently in window
    size_t getCount() const { return window.size(); }
};

#endif // ZSCORE_H
