#include "port_reader.h"
#include <iostream>
#include <chrono>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <ctime>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#endif

PortReader::PortReader(const std::string& port_name, int baud_rate)
    : port_name_(port_name), baud_rate_(baud_rate) {
}

PortReader::~PortReader() {
    stop();
}

bool PortReader::start() {
    if (running_) return true;

#ifdef _WIN32
    std::string full_port = "\\\\.\\" + port_name_;
    handle_ = CreateFileA(full_port.c_str(),
                         GENERIC_READ,
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
    
    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = 50;
    timeouts.ReadTotalTimeoutMultiplier = 10;
    SetCommTimeouts(handle_, &timeouts);
#else
    file_descriptor_ = open(port_name_.c_str(), O_RDONLY | O_NOCTTY);
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
    reading_thread_ = std::thread(&PortReader::reading_loop, this);
    std::cout << "Port reader started on port: " << port_name_ << std::endl;
    return true;
}

void PortReader::stop() {
    running_ = false;
    if (reading_thread_.joinable()) {
        reading_thread_.join();
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

void PortReader::set_callback(DataCallback callback) {
    callback_ = callback;
}

void PortReader::reading_loop() {
    std::cout << "Reading loop started" << std::endl;
    
    char buffer[256];
    std::string partial_data;
    
    while (running_) {
#ifdef _WIN32
        DWORD bytes_read;
        if (ReadFile(handle_, buffer, sizeof(buffer) - 1, &bytes_read, NULL)) {
            if (bytes_read > 0) {
                buffer[bytes_read] = '\0';
                partial_data += buffer;
                
                size_t pos;
                while ((pos = partial_data.find('\n')) != std::string::npos) {
                    std::string line = partial_data.substr(0, pos);
                    partial_data.erase(0, pos + 1);
                    
                    if (!line.empty() && callback_) {
                        callback_(line);
                    }
                }
            }
        }
#else
        ssize_t bytes_read = read(file_descriptor_, buffer, sizeof(buffer) - 1);
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            partial_data += buffer;
            
            size_t pos;
            while ((pos = partial_data.find('\n')) != std::string::npos) {
                std::string line = partial_data.substr(0, pos);
                partial_data.erase(0, pos + 1);
                
                if (!line.empty() && callback_) {
                    callback_(line);
                }
            }
        } else if (bytes_read < 0 && errno != EAGAIN) {
            std::cerr << "Read error: " << strerror(errno) << std::endl;
            break;
        }
#endif
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    std::cout << "Reading loop stopped" << std::endl;
}

bool PortReader::parse_temperature(const std::string& data, float& temperature, std::time_t& timestamp) {
    // Ожидаемый формат: TEMP:25.5 TIME:2024-01-15 14:30:45.123
    size_t temp_pos = data.find("TEMP:");
    size_t time_pos = data.find("TIME:");
    
    if (temp_pos == std::string::npos || time_pos == std::string::npos) {
        return false;
    }
    
    // Извлекаем температуру
    size_t temp_start = temp_pos + 5;
    size_t temp_end = data.find(' ', temp_start);
    if (temp_end == std::string::npos) {
        temp_end = time_pos - 1;
    }
    
    std::string temp_str = data.substr(temp_start, temp_end - temp_start);
    try {
        temperature = std::stof(temp_str);
    } catch (...) {
        return false;
    }
    
    // Извлекаем время
    size_t time_start = time_pos + 5;
    std::string time_str = data.substr(time_start);
    
    std::tm tm = {};
    std::istringstream ss(time_str);
    
    // Парсим с миллисекундами
    std::string datetime;
    ss >> datetime;
    
    if (strptime(datetime.c_str(), "%Y-%m-%d %H:%M:%S", &tm) != nullptr) {
        timestamp = std::mktime(&tm);
        return true;
    }
    
    return false;
}
