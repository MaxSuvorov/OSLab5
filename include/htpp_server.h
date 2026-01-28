#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include <map>

class HttpServer {
public:
    using RequestHandler = std::function<std::string(const std::map<std::string, std::string>&)>;
    
    HttpServer(int port = 8080);
    ~HttpServer();
    
    bool start();
    void stop();
    
    void register_handler(const std::string& path, RequestHandler handler);
    
    std::string generate_json_response(const std::string& data, int status_code = 200);
    std::string generate_error_response(const std::string& message, int status_code = 400);
    
private:
    void server_loop();
    std::string handle_request(const std::string& request);
    std::map<std::string, std::string> parse_query_params(const std::string& query);
    
    int port_;
    std::atomic<bool> running_{false};
    std::thread server_thread_;
    
#ifdef _WIN32
    SOCKET listen_socket_{INVALID_SOCKET};
#else
    int listen_socket_{-1};
#endif
    
    std::map<std::string, RequestHandler> handlers_;
};

#endif // HTTP_SERVER_H
