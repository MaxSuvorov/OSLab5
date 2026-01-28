#ifndef TEMPERATURE_CALCULATOR_H
#define TEMPERATURE_CALCULATOR_H

#include <vector>
#include <ctime>
#include <mutex>

struct TemperatureData {
    std::time_t timestamp;
    float temperature;
};

class TemperatureCalculator {
public:
    TemperatureCalculator();
    
    void add_measurement(std::time_t timestamp, float temperature);
    float calculate_hourly_average(std::time_t start_time) const;
    float calculate_daily_average(std::time_t start_time) const;
    
    std::vector<TemperatureData> get_measurements_last_24h() const;
    void cleanup_old_data(std::time_t current_time);
    
private:
    mutable std::mutex mutex_;
    std::vector<TemperatureData> measurements_;
    
    const int SECONDS_IN_HOUR = 3600;
    const int SECONDS_IN_DAY = 86400;
};
