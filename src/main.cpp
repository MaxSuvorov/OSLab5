#include "temperature_server.h"
#include <iostream>
#include <csignal>
#include <atomic>

std::atomic<bool> running{true};

void signal_handler(int signal) {
    std::cout << "\nReceived signal " << signal << ", stopping..." << std::endl;
    running = false;
}

int main(int argc, char* argv[]) {
    // Установка обработчика сигналов
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
#ifdef _WIN32
    std::signal(SIGBREAK, signal_handler);
#endif
    
    std::string port_name;
    int http_port = 8080;
    
    // Парсим аргументы командной строки
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            port_name = argv[++i];
        } else if (arg == "--http-port" && i + 1 < argc) {
            http_port = std::stoi(argv[++i]);
        } else if (arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
            std::cout << "Options:" << std::endl;
            std::cout << "  --port <name>      Serial port name (e.g., COM3 or /dev/ttyUSB0)" << std::endl;
            std::cout << "  --http-port <num>  HTTP server port (default: 8080)" << std::endl;
            std::cout << "  --help             Show this help message" << std::endl;
            return 0;
        }
    }
    
    // Если порт не указан, спросим пользователя
    if (port_name.empty()) {
        std::cout << "Enter serial port name (or press Enter to skip): ";
        std::getline(std::cin, port_name);
    }
    
    std::cout << "Starting Temperature Monitoring Server v2.0" << std::endl;
    std::cout << "HTTP Server will be available at http://localhost:" << http_port << std::endl;
    
    TemperatureServer server;
    
    if (!server.initialize(port_name, http_port)) {
        std::cerr << "Failed to initialize server" << std::endl;
        return 1;
    }
    
    // Если мы не подключены к порту, можно генерировать тестовые данные
    if (port_name.empty()) {
        std::cout << "No serial port specified. Running in simulation mode." << std::endl;
        std::cout << "Test data will be generated automatically." << std::endl;
    }
    
    server.run();
    
    std::cout << "Server stopped." << std::endl;
    return 0;
}
