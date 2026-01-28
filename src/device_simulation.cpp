#include "device_simulation.h"
#include <iostream>
#include <chrono>
#include <random>
#include <cstring>
#include <sstream>
#include <iomanip>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#endif

DeviceSimulator::DeviceSimulator(const std::string& port_name, int baud_rate)
    : port_name_(port_name), baud_rate_(baud_rate) {
}

DeviceSimulator::~DeviceSimulator() {
    stop();
}

bool DeviceSimulator::start() {
    if (running_) return true;

#ifdef _WIN32
    std::string full_port = "\\\\.\\" + port_name_;
    handle_ = CreateFileA(full_port.c_str(),
                         GENERIC_WRITE,
                         0,
                         NULL,
                         OPEN_EXISTING,
                         FILE_ATTRIBUTE_NORMAL,
                         NULL);
    
    if (handle_ == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to open port: " << port_name_ << std::endl;
        return false;
    }
    
    DCB dcb = {0};
    dcb.DCBlength = sizeof(DCB);
    GetCommState(handle_, &dcb);
    dcb.BaudRate = baud_rate_;
    dcb.ByteSize = 8;
    dcb.StopBits = ONESTOPBIT;
    dcb.Parity = NOPARITY;
    SetCommState(handle_, &dcb);
#else
    file_descriptor_ = open(port_name_.c_str(), O_WRONLY | O_NOCTTY);
    if (file_descriptor_ == -1) {
        std::cerr << "Failed to open port: " << port_name_ << " Error: " << strerror(errno) << std::endl;
        return false;
    }
    
    struct termios tty;
    memset(&tty, 0, sizeof(tty));
    
    if (tcgetattr(file_descriptor_, &tty) != 0) {
        std::cerr << "Error getting termios attributes" << std::endl;
        return false;
    }
    
    cfsetospeed(&tty, baud_rate_);
    cfsetispeed(&tty, baud_rate_);
    
    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cflag &= ~CRTSCTS;
    tty.c_cflag |= CREAD | CLOCAL;
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_oflag &= ~OPOST;
    
    if (tcsetattr(file_descriptor_, TCSANOW, &tty) != 0) {
        std::cerr << "Error setting termios attributes" << std::endl;
        return false;
    }
#endif

    running_ = true;
    simulation_thread_ = std::thread(&DeviceSimulator::simulation_loop, this);
    std::cout << "Device simulator started on port: " << port_name_ << std::endl;
    return true;
}

void DeviceSimulator::stop() {
    running_ = false;
    if (simulation_thread_.joinable()) {
        simulation_thread_.join();
    }
    
#ifdef _WIN32
    if (handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(handle_);
        handle_ = nullptr;
    }
#else
    if (file_descriptor_ != -1) {
        close(file_descriptor_);
        file_descriptor_ = -1;
    }
#endif
}

void DeviceSimulator::set_temperature_range(float min, float max) {
    min_temp_ = min;
    max_temp_ = max;
}

float DeviceSimulator::generate_temperature() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(min_temp_, max_temp_);
    return dis(gen);
}

void DeviceSimulator::simulation_loop() {
    std::cout << "Simulation loop started" << std::endl;
    
    while (running_) {
        float temperature = generate_temperature();
        
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t_now), "%Y-%m-%d %H:%M:%S");
        ss << "." << std::setfill('0') << std::setw(3) << ms.count();
        
        std::string message = "TEMP:" + std::to_string(temperature) +
                             " TIME:" + ss.str() + "\n";
        
#ifdef _WIN32
        DWORD bytes_written;
        WriteFile(handle_, message.c_str(), message.length(), &bytes_written, NULL);
#else
        write(file_descriptor_, message.c_str(), message.length());
#endif
        
        std::cout << "Sent: " << message;
        
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
    
    std::cout << "Simulation loop stopped" << std::endl;
}
