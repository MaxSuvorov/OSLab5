#ifndef DATABASE_MANAGER_H
#define DATABASE_MANAGER_H

#include <string>
#include <vector>
#include <ctime>
#include <memory>

struct TemperatureData {
    std::time_t timestamp;
    float temperature;
};

struct HourlyAverage {
    std::time_t hour_start;
    float average_temp;
    int count;
};

struct DailyAverage {
    std::time_t day_start;
    float average_temp;
    int count;
};

class DatabaseManager {
public:
    static DatabaseManager& get_instance();
    
    bool initialize(const std::string& db_path = "temperature_data.db");
    void cleanup();
    
    bool add_measurement(std::time_t timestamp, float temperature);
    bool add_hourly_average(std::time_t hour_start, float average_temp, int count);
    bool add_daily_average(std::time_t day_start, float average_temp, int count);
    
    std::vector<TemperatureData> get_measurements(std::time_t from = 0, std::time_t to = 0, int limit = 1000);
    std::vector<HourlyAverage> get_hourly_averages(std::time_t from = 0, std::time_t to = 0);
    std::vector<DailyAverage> get_daily_averages(std::time_t from = 0, std::time_t to = 0);
    
    TemperatureData get_last_measurement();
    float get_current_temperature();
    
    bool delete_old_measurements(std::time_t cutoff_time);
    bool delete_old_hourly_averages(std::time_t cutoff_time);
    bool delete_old_daily_averages(std::time_t cutoff_time);
    
private:
    DatabaseManager();
    ~DatabaseManager();
    
    struct DatabaseImpl;
    std::unique_ptr<DatabaseImpl> impl_;
    
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;
};

#endif // DATABASE_MANAGER_H
