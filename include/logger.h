#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <vector>
#include <ctime>
#include <mutex>
#include <memory>

struct TemperatureRecord {
    std::time_t timestamp;
    float temperature;
    
    TemperatureRecord(std::time_t ts, float temp) : timestamp(ts), temperature(temp) {}
};

class Logger {
public:
    static Logger& get_instance();
    
    void log_measurement(std::time_t timestamp, float temperature);
    void log_hourly_average(std::time_t timestamp, float average_temp);
    void log_daily_average(std::time_t timestamp, float average_temp);
    
    void cleanup_old_data();
    
private:
    Logger();
    ~Logger();
    
    void save_measurements_to_file();
    void save_hourly_averages_to_file();
    void save_daily_averages_to_file();
    
    void load_measurements_from_file();
    void load_hourly_averages_from_file();
    void load_daily_averages_from_file();
    
    std::vector<TemperatureRecord> measurements_;
    std::vector<TemperatureRecord> hourly_averages_;
    std::vector<TemperatureRecord> daily_averages_;
    
    std::mutex measurements_mutex_;
    std::mutex hourly_mutex_;
    std::mutex daily_mutex_;
    
    const std::string measurements_file_ = "temperature_measurements.log";
    const std::string hourly_file_ = "hourly_averages.log";
    const std::string daily_file_ = "daily_averages.log";
    
    std::time_t last_hourly_save_ = 0;
    std::time_t last_daily_save_ = 0;
    std::time_t last_cleanup_ = 0;
};

#endif // LOGGER_H
