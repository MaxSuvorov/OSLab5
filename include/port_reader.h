#ifndef PORT_READER_H
#define PORT_READER_H

#include <string>
#include <thread>
#include <atomic>
#include <functional>

class PortReader {
public:
    using DataCallback = std::function<void(const std::string&)>;
    
    PortReader(const std::string& port_name, int baud_rate = 9600);
    ~PortReader();
    
    bool start();
    void stop();
    void set_callback(DataCallback callback);
    
private:
    void reading_loop();
    bool parse_temperature(const std::string& data, float& temperature, std::time_t& timestamp);
    
    std::string port_name_;
    int baud_rate_;
    std::atomic<bool> running_{false};
    std::thread reading_thread_;
    DataCallback callback_;
    
    int file_descriptor_{-1};
    
#ifdef _WIN32
    void* handle_{nullptr};
#endif
};

#endif // PORT_READER_H
