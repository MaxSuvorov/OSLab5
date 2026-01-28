#include "temperature_server.h"
#include "port_reader.h"
#include "http_server.h"
#include "database_manager.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <vector>
#include <map>
#include <cmath>

TemperatureServer::TemperatureServer() {
}

TemperatureServer::~TemperatureServer() {
    stop();
}

bool TemperatureServer::initialize(const std::string& port_name, int http_port) {
    // Инициализируем базу данных
    if (!DatabaseManager::get_instance().initialize()) {
        std::cerr << "Failed to initialize database" << std::endl;
        return false;
    }
    
    // Инициализируем HTTP сервер
    http_server_ = std::make_unique<HttpServer>(http_port);
    
    // Регистрируем обработчики
    http_server_->register_handler("/api/current",
        [this](const std::map<std::string, std::string>& params) {
            return handle_current_temp(params);
        });
    
    http_server_->register_handler("/api/measurements",
        [this](const std::map<std::string, std::string>& params) {
            return handle_measurements(params);
        });
    
    http_server_->register_handler("/api/stats/hourly",
        [this](const std::map<std::string, std::string>& params) {
            return handle_hourly_stats(params);
        });
    
    http_server_->register_handler("/api/stats/daily",
        [this](const std::map<std::string, std::string>& params) {
            return handle_daily_stats(params);
        });
    
    http_server_->register_handler("/api/system/info",
        [this](const std::map<std::string, std::string>& params) {
            return handle_system_info(params);
        });
    
    // Запускаем HTTP сервер
    if (!http_server_->start()) {
        std::cerr << "Failed to start HTTP server" << std::endl;
        return false;
    }
    
    // Инициализируем чтение порта, если указан
    if (!port_name.empty()) {
        port_reader_ = std::make_unique<PortReader>(port_name, 9600);
        port_reader_->set_callback([this](const std::string& data) {
            process_temperature_data(data);
        });
        
        if (!port_reader_->start()) {
            std::cerr << "Warning: Failed to start port reader" << std::endl;
        }
    }
    
    running_ = true;
    
    // Запускаем фоновые потоки
    stats_thread_ = std::thread([this]() {
        while (running_) {
            calculate_statistics();
            std::this_thread::sleep_for(std::chrono::seconds(STATS_INTERVAL_SECONDS));
        }
    });
    
    cleanup_thread_ = std::thread([this]() {
        while (running_) {
            cleanup_old_data();
            std::this_thread::sleep_for(std::chrono::seconds(CLEANUP_INTERVAL_SECONDS));
        }
    });
    
    return true;
}

void TemperatureServer::run() {
    std::cout << "Temperature server running. Press Enter to stop..." << std::endl;
    std::cin.get();
    stop();
}

void TemperatureServer::stop() {
    running_ = false;
    
    if (port_reader_) {
        port_reader_->stop();
    }
    
    if (http_server_) {
        http_server_->stop();
    }
    
    if (stats_thread_.joinable()) {
        stats_thread_.join();
    }
    
    if (cleanup_thread_.joinable()) {
        cleanup_thread_.join();
    }
    
    DatabaseManager::get_instance().cleanup();
}

void TemperatureServer::process_temperature_data(const std::string& data) {
    // Парсим температуру из данных
    size_t temp_pos = data.find("TEMP:");
    if (temp_pos == std::string::npos) return;
    
    size_t temp_start = temp_pos + 5;
    size_t temp_end = data.find(' ', temp_start);
    std::string temp_str = data.substr(temp_start, temp_end - temp_start);
    
    try {
        float temperature = std::stof(temp_str);
        std::time_t timestamp = std::time(nullptr);
        
        current_temperature_ = temperature;
        last_update_ = timestamp;
        
        // Сохраняем в базу данных
        DatabaseManager::get_instance().add_measurement(timestamp, temperature);
        
        std::cout << "Temperature: " << temperature << "°C at "
                  << std::ctime(&timestamp);
                  
    } catch (...) {
        std::cerr << "Failed to parse temperature from: " << data << std::endl;
    }
}

void TemperatureServer::calculate_statistics() {
    std::time_t now = std::time(nullptr);
    
    // Вычисляем среднечасовую температуру
    std::time_t hour_start = (now / 3600) * 3600;
    auto measurements = DatabaseManager::get_instance().get_measurements(
        hour_start - 3600, hour_start);
    
    if (!measurements.empty()) {
        float sum = 0;
        for (const auto& m : measurements) {
            sum += m.temperature;
        }
        float hourly_avg = sum / measurements.size();
        
        DatabaseManager::get_instance().add_hourly_average(
            hour_start - 3600, hourly_avg, measurements.size());
        
        std::cout << "Hourly average: " << hourly_avg
                  << "°C (based on " << measurements.size()
                  << " measurements)" << std::endl;
    }
    
    // Вычисляем среднесуточную температуру
    std::tm* tm = std::localtime(&now);
    tm->tm_hour = 0;
    tm->tm_min = 0;
    tm->tm_sec = 0;
    std::time_t day_start = std::mktime(tm) - 86400; // Вчера
    
    auto day_measurements = DatabaseManager::get_instance().get_measurements(
        day_start, day_start + 86400);
    
    if (!day_measurements.empty()) {
        float sum = 0;
        for (const auto& m : day_measurements) {
            sum += m.temperature;
        }
        float daily_avg = sum / day_measurements.size();
        
        DatabaseManager::get_instance().add_daily_average(
            day_start, daily_avg, day_measurements.size());
        
        std::cout << "Daily average: " << daily_avg
                  << "°C (based on " << day_measurements.size()
                  << " measurements)" << std::endl;
    }
}

void TemperatureServer::cleanup_old_data() {
    std::time_t now = std::time(nullptr);
    
    // Удаляем измерения старше 7 дней
    DatabaseManager::get_instance().delete_old_measurements(now - 7 * 86400);
    
    // Удаляем часовые средние старше 30 дней
    DatabaseManager::get_instance().delete_old_hourly_averages(now - 30 * 86400);
    
    // Удаляем дневные средние старше 365 дней
    DatabaseManager::get_instance().delete_old_daily_averages(now - 365 * 86400);
}

std::string TemperatureServer::handle_current_temp(const std::map<std::string, std::string>& params) {
    std::ostringstream json;
    
    float temp = DatabaseManager::get_instance().get_current_temperature();
    auto last_measurement = DatabaseManager::get_instance().get_last_measurement();
    
    json << "{";
    json << "\"current_temperature\": " << temp << ",";
    json << "\"last_update\": " << last_measurement.timestamp << ",";
    json << "\"unit\": \"celsius\"";
    json << "}";
    
    return http_server_->generate_json_response(json.str());
}

std::string TemperatureServer::handle_measurements(const std::map<std::string, std::string>& params) {
    std::time_t from = 0;
    std::time_t to = 0;
    int limit = 100;
    
    if (params.find("from") != params.end()) {
        from = std::stol(params.at("from"));
    }
    
    if (params.find("to") != params.end()) {
        to = std::stol(params.at("to"));
    }
    
    if (params.find("limit") != params.end()) {
        limit = std::stoi(params.at("limit"));
    }
    
    auto measurements = DatabaseManager::get_instance().get_measurements(from, to, limit);
    
    std::ostringstream json;
    json << "{\"measurements\": [";
    
    for (size_t i = 0; i < measurements.size(); ++i) {
        const auto& m = measurements[i];
        json << "{";
        json << "\"timestamp\": " << m.timestamp << ",";
        json << "\"temperature\": " << m.temperature;
        json << "}";
        
        if (i < measurements.size() - 1) {
            json << ",";
        }
    }
    
    json << "], \"count\": " << measurements.size() << "}";
    
    return http_server_->generate_json_response(json.str());
}

std::string TemperatureServer::handle_hourly_stats(const std::map<std::string, std::string>& params) {
    std::time_t from = 0;
    std::time_t to = 0;
    
    if (params.find("from") != params.end()) {
        from = std::stol(params.at("from"));
    }
    
    if (params.find("to") != params.end()) {
        to = std::stol(params.at("to"));
    }
    
    // По умолчанию за последний месяц
    if (from == 0 && to == 0) {
        to = std::time(nullptr);
        from = to - 30 * 86400;
    }
    
    auto hourly_stats = DatabaseManager::get_instance().get_hourly_averages(from, to);
    
    std::ostringstream json;
    json << "{\"hourly_averages\": [";
    
    for (size_t i = 0; i < hourly_stats.size(); ++i) {
        const auto& stat = hourly_stats[i];
        json << "{";
        json << "\"hour_start\": " << stat.hour_start << ",";
        json << "\"average_temperature\": " << stat.average_temp << ",";
        json << "\"measurement_count\": " << stat.count;
        json << "}";
        
        if (i < hourly_stats.size() - 1) {
            json << ",";
        }
    }
    
    json << "], \"count\": " << hourly_stats.size() << "}";
    
    return http_server_->generate_json_response(json.str());
}

std::string TemperatureServer::handle_daily_stats(const std::map<std::string, std::string>& params) {
    std::time_t from = 0;
    std::time_t to = 0;
    
    if (params.find("from") != params.end()) {
        from = std::stol(params.at("from"));
    }
    
    if (params.find("to") != params.end()) {
        to = std::stol(params.at("to"));
    }
    
    // По умолчанию за текущий год
    if (from == 0 && to == 0) {
        to = std::time(nullptr);
        std::tm* tm = std::localtime(&to);
        tm->tm_mon = 0;
        tm->tm_mday = 1;
        tm->tm_hour = 0;
        tm->tm_min = 0;
        tm->tm_sec = 0;
        from = std::mktime(tm);
    }
    
    auto daily_stats = DatabaseManager::get_instance().get_daily_averages(from, to);
    
    std::ostringstream json;
    json << "{\"daily_averages\": [";
    
    for (size_t i = 0; i < daily_stats.size(); ++i) {
        const auto& stat = daily_stats[i];
        json << "{";
        json << "\"day_start\": " << stat.day_start << ",";
        json << "\"average_temperature\": " << stat.average_temp << ",";
        json << "\"measurement_count\": " << stat.count;
        json << "}";
        
        if (i < daily_stats.size() - 1) {
            json << ",";
        }
    }
    
    json << "], \"count\": " << daily_stats.size() << "}";
    
    return http_server_->generate_json_response(json.str());
}

std::string TemperatureServer::handle_system_info(const std::map<std::string, std::string>& params) {
    std::ostringstream json;
    
    auto last_measurement = DatabaseManager::get_instance().get_last_measurement();
    auto measurements = DatabaseManager::get_instance().get_measurements(0, 0, 1);
    auto hourly_stats = DatabaseManager::get_instance().get_hourly_averages(0, 0);
    auto daily_stats = DatabaseManager::get_instance().get_daily_averages(0, 0);
    
    json << "{";
    json << "\"system\": \"Temperature Monitoring System\",";
    json << "\"version\": \"2.0\",";
    json << "\"status\": \"running\",";
    json << "\"current_temperature\": " << current_temperature_ << ",";
    json << "\"last_update\": " << last_measurement.timestamp << ",";
    json << "\"measurements_count\": " << measurements.size() << ",";
    json << "\"hourly_stats_count\": " << hourly_stats.size() << ",";
    json << "\"daily_stats_count\": " << daily_stats.size() << ",";
    json << "\"server_time\": " << std::time(nullptr);
    json << "}";
    
    return http_server_->generate_json_response(json.str());
}
