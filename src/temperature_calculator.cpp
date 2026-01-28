#include "temperature_calculator.h"
#include <algorithm>
#include <numeric>
#include <iostream>

TemperatureCalculator::TemperatureCalculator() {
}

void TemperatureCalculator::add_measurement(std::time_t timestamp, float temperature) {
    std::lock_guard<std::mutex> lock(mutex_);
    measurements_.push_back({timestamp, temperature});
}

float TemperatureCalculator::calculate_hourly_average(std::time_t start_time) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<float> temps;
    for (const auto& data : measurements_) {
        if (data.timestamp >= start_time &&
            data.timestamp < start_time + SECONDS_IN_HOUR) {
            temps.push_back(data.temperature);
        }
    }
    
    if (temps.empty()) return 0.0f;
    
    float sum = std::accumulate(temps.begin(), temps.end(), 0.0f);
    return sum / temps.size();
}

float TemperatureCalculator::calculate_daily_average(std::time_t start_time) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<float> temps;
    for (const auto& data : measurements_) {
        if (data.timestamp >= start_time &&
            data.timestamp < start_time + SECONDS_IN_DAY) {
            temps.push_back(data.temperature);
        }
    }
    
    if (temps.empty()) return 0.0f;
    
    float sum = std::accumulate(temps.begin(), temps.end(), 0.0f);
    return sum / temps.size();
}

std::vector<TemperatureData> TemperatureCalculator::get_measurements_last_24h() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::time_t now = std::time(nullptr);
    
    std::vector<TemperatureData> result;
    for (const auto& data : measurements_) {
        if (now - data.timestamp <= SECONDS_IN_DAY) {
            result.push_back(data);
        }
    }
    
    return result;
}

void TemperatureCalculator::cleanup_old_data(std::time_t current_time) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = std::remove_if(measurements_.begin(), measurements_.end(),
        [current_time, this](const TemperatureData& data) {
            return current_time - data.timestamp > SECONDS_IN_DAY;
        });
    
    measurements_.erase(it, measurements_.end());
}
