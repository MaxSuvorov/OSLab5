#include "http_server.h"
#include <iostream>
#include <sstream>
#include <string>
#include <cstring>
#include <ctime>
#include <iomanip>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#endif

HttpServer::HttpServer(int port) : port_(port) {
#ifdef _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        std::cerr << "WSAStartup failed" << std::endl;
    }
#endif
}

HttpServer::~HttpServer() {
    stop();
#ifdef _WIN32
    WSACleanup();
#endif
}

bool HttpServer::start() {
    if (running_) return true;
    
    // Создаем сокет
#ifdef _WIN32
    listen_socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_socket_ == INVALID_SOCKET) {
        std::cerr << "Failed to create socket" << std::endl;
        return false;
    }
    
    // Разрешаем повторное использование адреса
    int reuse = 1;
    if (setsockopt(listen_socket_, SOL_SOCKET, SO_REUSEADDR,
                   (char*)&reuse, sizeof(reuse)) == SOCKET_ERROR) {
        std::cerr << "setsockopt failed" << std::endl;
        closesocket(listen_socket_);
        return false;
    }
#else
    listen_socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_socket_ < 0) {
        std::cerr << "Failed to create socket" << std::endl;
        return false;
    }
    
    int reuse = 1;
    if (setsockopt(listen_socket_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        std::cerr << "setsockopt failed" << std::endl;
        close(listen_socket_);
        return false;
    }
#endif
    
    // Настраиваем адрес
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port_);
    
    // Привязываем сокет
#ifdef _WIN32
    if (bind(listen_socket_, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed" << std::endl;
        closesocket(listen_socket_);
        return false;
    }
#else
    if (bind(listen_socket_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Bind failed" << std::endl;
        close(listen_socket_);
        return false;
    }
#endif
    
    // Начинаем слушать
#ifdef _WIN32
    if (listen(listen_socket_, 10) == SOCKET_ERROR) {
        std::cerr << "Listen failed" << std::endl;
        closesocket(listen_socket_);
        return false;
    }
#else
    if (listen(listen_socket_, 10) < 0) {
        std::cerr << "Listen failed" << std::endl;
        close(listen_socket_);
        return false;
    }
#endif
    
    running_ = true;
    server_thread_ = std::thread(&HttpServer::server_loop, this);
    
    std::cout << "HTTP server started on port " << port_ << std::endl;
    return true;
}

void HttpServer::stop() {
    running_ = false;
    
    if (server_thread_.joinable()) {
        server_thread_.join();
    }
    
#ifdef _WIN32
    if (listen_socket_ != INVALID_SOCKET) {
        closesocket(listen_socket_);
        listen_socket_ = INVALID_SOCKET;
    }
#else
    if (listen_socket_ >= 0) {
        close(listen_socket_);
        listen_socket_ = -1;
    }
#endif
}

void HttpServer::register_handler(const std::string& path, RequestHandler handler) {
    handlers_[path] = handler;
}

void HttpServer::server_loop() {
    while (running_) {
        struct sockaddr_in client_addr;
#ifdef _WIN32
        int client_addr_len = sizeof(client_addr);
        SOCKET client_socket = accept(listen_socket_,
                                     (struct sockaddr*)&client_addr,
                                     &client_addr_len);
        
        if (client_socket == INVALID_SOCKET) {
            if (running_) {
                std::cerr << "Accept failed" << std::endl;
            }
            continue;
        }
#else
        socklen_t client_addr_len = sizeof(client_addr);
        int client_socket = accept(listen_socket_,
                                  (struct sockaddr*)&client_addr,
                                  &client_addr_len);
        
        if (client_socket < 0) {
            if (running_) {
                std::cerr << "Accept failed" << std::endl;
            }
            continue;
        }
#endif
        
        char buffer[4096] = {0};
        
#ifdef _WIN32
        int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';
            std::string response = handle_request(buffer);
            send(client_socket, response.c_str(), response.length(), 0);
        }
        closesocket(client_socket);
#else
        ssize_t bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';
            std::string response = handle_request(buffer);
            send(client_socket, response.c_str(), response.length(), 0);
        }
        close(client_socket);
#endif
    }
}

std::string HttpServer::handle_request(const std::string& request) {
    std::istringstream iss(request);
    std::string method, path, version;
    
    iss >> method >> path >> version;
    
    if (method != "GET") {
        return generate_error_response("Method not allowed", 405);
    }
    
    // Извлекаем путь без query параметров
    size_t query_pos = path.find('?');
    std::string clean_path = path.substr(0, query_pos);
    
    // Парсим query параметры
    std::map<std::string, std::string> params;
    if (query_pos != std::string::npos) {
        std::string query_str = path.substr(query_pos + 1);
        params = parse_query_params(query_str);
    }
    
    // Обрабатываем статические файлы
    if (clean_path == "/" || clean_path == "/index.html") {
        clean_path = "/index.html";
    }
    
    auto handler_it = handlers_.find(clean_path);
    if (handler_it != handlers_.end()) {
        return handler_it->second(params);
    }
    
    // Для статических файлов
    if (clean_path.find(".") != std::string::npos) {
        return generate_error_response("Static file serving not implemented", 501);
    }
    
    return generate_error_response("Not found", 404);
}

std::map<std::string, std::string> HttpServer::parse_query_params(const std::string& query) {
    std::map<std::string, std::string> params;
    std::istringstream iss(query);
    std::string pair;
    
    while (std::getline(iss, pair, '&')) {
        size_t eq_pos = pair.find('=');
        if (eq_pos != std::string::npos) {
            std::string key = pair.substr(0, eq_pos);
            std::string value = pair.substr(eq_pos + 1);
            
            // Декодируем URL-encoded значения
            // Простая реализация - можно расширить при необходимости
            std::string decoded_value;
            for (size_t i = 0; i < value.length(); ++i) {
                if (value[i] == '%' && i + 2 < value.length()) {
                    int hex_val;
                    std::istringstream hex_stream(value.substr(i + 1, 2));
                    if (hex_stream >> std::hex >> hex_val) {
                        decoded_value += static_cast<char>(hex_val);
                        i += 2;
                    } else {
                        decoded_value += value[i];
                    }
                } else if (value[i] == '+') {
                    decoded_value += ' ';
                } else {
                    decoded_value += value[i];
                }
            }
            
            params[key] = decoded_value;
        }
    }
    
    return params;
}

std::string HttpServer::generate_json_response(const std::string& data, int status_code) {
    std::ostringstream oss;
    oss << "HTTP/1.1 " << status_code << " OK\r\n";
    oss << "Content-Type: application/json\r\n";
    oss << "Access-Control-Allow-Origin: *\r\n";
    oss << "Content-Length: " << data.length() << "\r\n";
    oss << "Connection: close\r\n";
    oss << "\r\n";
    oss << data;
    
    return oss.str();
}

std::string HttpServer::generate_error_response(const std::string& message, int status_code) {
    std::ostringstream oss;
    oss << "{\"error\": \"" << message << "\", \"status\": " << status_code << "}";
    return generate_json_response(oss.str(), status_code);
}
