#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <fstream>
#include <sstream>
#include <map>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

class WebServer {
public:
    WebServer(int port = 8081, const std::string& web_dir = "web_client")
        : port_(port), web_dir_(web_dir), running_(false) {
        
#ifdef _WIN32
        WSADATA wsa_data;
        WSAStartup(MAKEWORD(2, 2), &wsa_data);
#endif
    }
    
    ~WebServer() {
        stop();
#ifdef _WIN32
        WSACleanup();
#endif
    }
    
    bool start() {
        if (running_) return true;
        
        // Создаем сокет
#ifdef _WIN32
        SOCKET server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd == INVALID_SOCKET) {
            return false;
        }
#else
        int server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd < 0) {
            return false;
        }
#endif
        
        // Разрешаем повторное использование адреса
        int opt = 1;
#ifdef _WIN32
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
#else
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif
        
        // Настраиваем адрес
        struct sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(port_);
        
        // Привязываем сокет
#ifdef _WIN32
        if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) == SOCKET_ERROR) {
            closesocket(server_fd);
            return false;
        }
#else
        if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
            close(server_fd);
            return false;
        }
#endif
        
        // Начинаем слушать
#ifdef _WIN32
        if (listen(server_fd, 10) == SOCKET_ERROR) {
            closesocket(server_fd);
            return false;
        }
#else
        if (listen(server_fd, 10) < 0) {
            close(server_fd);
            return false;
        }
#endif
        
        running_ = true;
        server_thread_ = std::thread([this, server_fd]() {
            while (running_) {
                struct sockaddr_in client_addr;
#ifdef _WIN32
                int client_addr_len = sizeof(client_addr);
                SOCKET client_fd = accept(server_fd,
                                         (struct sockaddr*)&client_addr,
                                         &client_addr_len);
                
                if (client_fd == INVALID_SOCKET) {
                    continue;
                }
#else
                socklen_t client_addr_len = sizeof(client_addr);
                int client_fd = accept(server_fd,
                                      (struct sockaddr*)&client_addr,
                                      &client_addr_len);
                
                if (client_fd < 0) {
                    continue;
                }
#endif
                
                // Обрабатываем запрос в отдельном потоке
                std::thread([this, client_fd]() {
                    handle_client(client_fd);
                }).detach();
            }
            
#ifdef _WIN32
            closesocket(server_fd);
#else
            close(server_fd);
#endif
        });
        
        std::cout << "Web server started on http://localhost:" << port_ << std::endl;
        return true;
    }
    
    void stop() {
        running_ = false;
        if (server_thread_.joinable()) {
            server_thread_.join();
        }
    }
    
private:
    void handle_client(int client_fd) {
        char buffer[4096] = {0};
        
#ifdef _WIN32
        int bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
#else
        ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
#endif
        
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            std::string request(buffer);
            
            // Извлекаем путь запроса
            size_t start = request.find(' ') + 1;
            size_t end = request.find(' ', start);
            std::string path = request.substr(start, end - start);
            
            if (path == "/" || path == "") {
                path = "/index.html";
            }
            
            // Обслуживаем файл
            std::string full_path = web_dir_ + path;
            std::ifstream file(full_path, std::ios::binary);
            
            std::string response;
            if (file) {
                // Читаем файл
                std::stringstream file_content;
                file_content << file.rdbuf();
                std::string content = file_content.str();
                
                // Определяем Content-Type
                std::string content_type = "text/plain";
                if (path.find(".html") != std::string::npos) {
                    content_type = "text/html";
                } else if (path.find(".css") != std::string::npos) {
                    content_type = "text/css";
                } else if (path.find(".js") != std::string::npos) {
                    content_type = "application/javascript";
                } else if (path.find(".png") != std::string::npos) {
                    content_type = "image/png";
                } else if (path.find(".jpg") != std::string::npos ||
                           path.find(".jpeg") != std::string::npos) {
                    content_type = "image/jpeg";
                }
                
                // Формируем ответ
                response = "HTTP/1.1 200 OK\r\n";
                response += "Content-Type: " + content_type + "\r\n";
                response += "Content-Length: " + std::to_string(content.length()) + "\r\n";
                response += "Connection: close\r\n";
                response += "\r\n";
                response += content;
            } else {
                // Файл не найден
                response = "HTTP/1.1 404 Not Found\r\n";
                response += "Content-Type: text/html\r\n";
                response += "Connection: close\r\n";
                response += "\r\n";
                response += "<h1>404 Not Found</h1>";
            }
            
            // Отправляем ответ
#ifdef _WIN32
            send(client_fd, response.c_str(), response.length(), 0);
            closesocket(client_fd);
#else
            send(client_fd, response.c_str(), response.length(), 0);
            close(client_fd);
#endif
        }
    }
    
    int port_;
    std::string web_dir_;
    std::atomic<bool> running_;
    std::thread server_thread_;
};

int main(int argc, char* argv[]) {
    int port = 8081;
    std::string web_dir = "web_client";
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if (arg == "--dir" && i + 1 < argc) {
            web_dir = argv[++i];
        }
    }
    
    WebServer server(port, web_dir);
    
    if (!server.start()) {
        std::cerr << "Failed to start web server" << std::endl;
        return 1;
    }
    
    std::cout << "Web server running. Press Enter to stop..." << std::endl;
    std::cin.get();
    
    server.stop();
    return 0;
}
