#ifndef TEMPERATURE_SERVER_H
#define TEMPERATURE_SERVER_H

#include <string>
#include <memory>
#include <thread>
#include <atomic>

class PortReader;
class HttpServer;
class DatabaseManager;

class TemperatureServer {
public:
    TemperatureServer();
    ~TemperatureServer();
    
    bool initialize(const std::string& port_name = "", int http_port = 8080);
    void run();
    void stop();
    
    // HTTP обработчики
    std::string handle_current_temp(const std::map<std::string, std::string>& params);
    std::string handle_measurements(const std::map<std::string, std::string>& params);
    std::string handle_hourly_stats(const std::map<std::string, std::string>& params);
    std::string handle_daily_stats(const std::map<std::string, std::string>& params);
    std::string handle_system_info(const std::map<std::string, std::string>& params);
    
private:
    void process_temperature_data(const std::string& data);
    void calculate_statistics();
    void cleanup_old_data();
    
    std::unique_ptr<PortReader> port_reader_;
    std::unique_ptr<HttpServer> http_server_;
    
    std::thread stats_thread_;
    std::thread cleanup_thread_;
    std::atomic<bool> running_{false};
    
    float current_temperature_{0.0f};
    std::time_t last_update_{0};
    
    static constexpr int STATS_INTERVAL_SECONDS = 3600; // 1 час
    static constexpr int CLEANUP_INTERVAL_SECONDS = 300; // 5 минут
};

#endif // TEMPERATURE_SERVER_H
