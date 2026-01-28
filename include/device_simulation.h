#ifndef DEVICE_SIMULATION_H
#define DEVICE_SIMULATION_H

#include <string>
#include <thread>
#include <atomic>
#include <vector>

class DeviceSimulator {
public:
    DeviceSimulator(const std::string& port_name, int baud_rate = 9600);
    ~DeviceSimulator();
    
    bool start();
    void stop();
    void set_temperature_range(float min, float max);
    
private:
    void simulation_loop();
    float generate_temperature();
    
    std::string port_name_;
    int baud_rate_;
    std::atomic<bool> running_{false};
    std::thread simulation_thread_;
    
    float min_temp_{20.0f};
    float max_temp_{25.0f};
    int file_descriptor_{-1};
    
#ifdef _WIN32
    void* handle_{nullptr};
#endif
};

#endif // DEVICE_SIMULATION_H
