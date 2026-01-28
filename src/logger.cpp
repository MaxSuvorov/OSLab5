#include "logger.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <filesystem>

Logger& Logger::get_instance() {
    static Logger instance;
    return instance;
}

Logger::Logger() {
    load_measurements_from_file();
    load_hourly_averages_from_file();
    load_daily_averages_from_file();
    last_cleanup_ = std::time(nullptr);
}

Logger::~Logger() {
    save_measurements_to_file();
    save_hourly_averages_to_file();
    save_daily_averages_to_file();
}

void Logger::log_measurement(std::time_t timestamp, float temperature) {
    std::lock_guard<std::mutex> lock(measurements_mutex_);
    measurements_.emplace_back(timestamp, temperature);
    
    // Сохраняем в файл каждые 10 записей
    if (measurements_.size() % 10 == 0) {
        save_measurements_to_file();
    }
    
    // Проверяем необходимость вычисления средних
    std::time_t now = std::time(nullptr);
    
    // Ежечасное среднее
    if (last_hourly_save_ == 0 || (now - last_hourly_save_) >= 3600) {
        // Вычисляем среднее за последний час
        float sum = 0;
        int count = 0;
        for (const auto& record : measurements_) {
            if (now - record.timestamp <= 3600) {
                sum += record.temperature;
                count++;
            }
        }
        
        if (count > 0) {
            log_hourly_average(now, sum / count);
        }
        last_hourly_save_ = now;
    }
    
    // Ежедневное среднее
    if (last_daily_save_ == 0 || (now - last_daily_save_) >= 86400) {
        // Вычисляем среднее за последние 24 часа
        float sum = 0;
        int count = 0;
        for (const auto& record : measurements_) {
            if (now - record.timestamp <= 86400) {
                sum += record.temperature;
                count++;
            }
        }
        
        if (count > 0) {
            log_daily_average(now, sum / count);
        }
        last_daily_save_ = now;
    }
    
    // Очистка старых данных
    if (now - last_cleanup_ >= 300) { // Каждые 5 минут
        cleanup_old_data();
        last_cleanup_ = now;
    }
}

void Logger::log_hourly_average(std::time_t timestamp, float average_temp) {
    std::lock_guard<std::mutex> lock(hourly_mutex_);
    hourly_averages_.emplace_back(timestamp, average_temp);
    save_hourly_averages_to_file();
}

void Logger::log_daily_average(std::time_t timestamp, float average_temp) {
    std::lock_guard<std::mutex> lock(daily_mutex_);
    daily_averages_.emplace_back(timestamp, average_temp);
    save_daily_averages_to_file();
}

void Logger::cleanup_old_data() {
    std::time_t now = std::time(nullptr);
    
    // Очистка измерений старше 24 часов
    {
        std::lock_guard<std::mutex> lock(measurements_mutex_);
        auto it = std::remove_if(measurements_.begin(), measurements_.end(),
            [now](const TemperatureRecord& record) {
                return (now - record.timestamp) > 86400; // 24 часа
            });
        measurements_.erase(it, measurements_.end());
        save_measurements_to_file();
    }
    
    // Очистка средних за час старше 30 дней
    {
        std::lock_guard<std::mutex> lock(hourly_mutex_);
        auto it = std::remove_if(hourly_averages_.begin(), hourly_averages_.end(),
            [now](const TemperatureRecord& record) {
                return (now - record.timestamp) > 2592000; // 30 дней
            });
        hourly_averages_.erase(it, hourly_averages_.end());
        save_hourly_averages_to_file();
    }
    
    // Очистка средних за день старше 365 дней
    {
        std::lock_guard<std::mutex> lock(daily_mutex_);
        auto it = std::remove_if(daily_averages_.begin(), daily_averages_.end(),
            [now](const TemperatureRecord& record) {
                return (now - record.timestamp) > 31536000; // 365 дней
            });
        daily_averages_.erase(it, daily_averages_.end());
        save_daily_averages_to_file();
    }
}

void Logger::save_measurements_to_file() {
    std::ofstream file(measurements_file_, std::ios::trunc);
    if (!file.is_open()) return;
    
    for (const auto& record : measurements_) {
        std::tm* tm = std::localtime(&record.timestamp);
        file << std::put_time(tm, "%Y-%m-%d %H:%M:%S") << " "
             << record.temperature << std::endl;
    }
}

void Logger::save_hourly_averages_to_file() {
    std::ofstream file(hourly_file_, std::ios::trunc);
    if (!file.is_open()) return;
    
    for (const auto& record : hourly_averages_) {
        std::tm* tm = std::localtime(&record.timestamp);
        file << std::put_time(tm, "%Y-%m-%d %H:%M:%S") << " "
             << "HOURLY_AVG " << record.temperature << std::endl;
    }
}

void Logger::save_daily_averages_to_file() {
    std::ofstream file(daily_file_, std::ios::trunc);
    if (!file.is_open()) return;
    
    for (const auto& record : daily_averages_) {
        std::tm* tm = std::localtime(&record.timestamp);
        file << std::put_time(tm, "%Y-%m-%d") << " "
             << "DAILY_AVG " << record.temperature << std::endl;
    }
}

void Logger::load_measurements_from_file() {
    std::ifstream file(measurements_file_);
    if (!file.is_open()) return;
    
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::tm tm = {};
        float temp;
        
        if (iss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S") >> temp) {
            std::time_t timestamp = std::mktime(&tm);
            measurements_.emplace_back(timestamp, temp);
        }
    }
}

void Logger::load_hourly_averages_from_file() {
    std::ifstream file(hourly_file_);
    if (!file.is_open()) return;
    
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::tm tm = {};
        std::string type;
        float temp;
        
        if (iss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S") >> type >> temp) {
            if (type == "HOURLY_AVG") {
                std::time_t timestamp = std::mktime(&tm);
                hourly_averages_.emplace_back(timestamp, temp);
            }
        }
    }
}

void Logger::load_daily_averages_from_file() {
    std::ifstream file(daily_file_);
    if (!file.is_open()) return;
    
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::tm tm = {};
        std::string type;
        float temp;
        
        if (iss >> std::get_time(&tm, "%Y-%m-%d") >> type >> temp) {
            if (type == "DAILY_AVG") {
                tm.tm_hour = 12;
                tm.tm_min = 0;
                tm.tm_sec = 0;
                std::time_t timestamp = std::mktime(&tm);
                daily_averages_.emplace_back(timestamp, temp);
            }
        }
    }
}
