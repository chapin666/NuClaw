// src/step01/main.cpp
// Step 1: HTTP 路由系统
// 编译: g++ -std=c++17 main.cpp -o server -lboost_system -lboost_thread -lpthread

#include <boost/asio.hpp>
#include <boost/algorithm/string.hpp>
#include <iostream>
#include <sstream>
#include <string>
#include <map>
#include <functional>

using boost::asio::ip::tcp;

// HTTP 请求结构体
struct HttpRequest {
    std::string method;                           // GET, POST, etc.
    std::string path;                             // /, /health, /chat
    std::string body;                             // 请求体
    std::map<std::string, std::string> headers;   // 请求头
};

// 解析原始 HTTP 请求字符串
HttpRequest parse_request(const std::string& raw) {
    HttpRequest req;
    std::istringstream stream(raw);
    std::string line;
    
    // 解析请求行: GET /path HTTP/1.1
    if (std::getline(stream, line)) {
        std::istringstream line_stream(line);
        line_stream >> req.method >> req.path;
    }
    
    // 解析请求头
    while (std::getline(stream, line) && !line.empty() && line != "\r") {
        auto pos = line.find(':');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            boost::trim(key);
            boost::trim(value);
            req.headers[key] = value;
        }
    }
    
    // 解析请求体 (如果存在 Content-Length)
    auto it = req.headers.find("Content-Length");
    if (it != req.headers.end()) {
        int content_length = std::stoi(it->second);
        if (content_length > 0) {
            // 找到 body 开始位置 (\r\n\r\n 之后)
            size_t body_start = raw.find("\r\n\r\n");
            if (body_start != std::string::npos) {
                req.body = raw.substr(body_start + 4, content_length);
            }
        }
    }
    
    return req;
}

// 构造 HTTP 响应
std::string make_response(int code, const std::string& body) {
    std::string status_text = (code == 200) ? "200 OK" : "404 Not Found";
    return "HTTP/1.1 " + status_text + "\r\n"
           "Content-Type: application/json\r\n"
           "Content-Length: " + std::to_string(body.length()) + "\r\n"
           "Connection: close\r\n"
           "\r\n" + body;
}

// 路由系统
class Router {
public:
    using Handler = std::function<std::string(const HttpRequest&)>;
    
    // 添加路由
    void add(const std::string& path, Handler handler) {
        routes_[path] = handler;
    }
    
    // 处理请求
    std::string handle(const HttpRequest& req) {
        auto it = routes_.find(req.path);
        if (it != routes_.end()) {
            return make_response(200, it->second(req));
        }
        return make_response(404, R"({"error":"not found"})");
    }
    
private:
    std::map<std::string, Handler> routes_;
};

// 路由处理器
class Handlers {
public:
    // GET / - 服务状态
    static std::string root(const HttpRequest&) {
        return R"({
    "status": "ok",
    "step": 1,
    "name": "NuClaw",
    "features": ["routing", "request-parsing"]
})";
    }
    
    // GET /health - 健康检查
    static std::string health(const HttpRequest&) {
        return R"({"health": "ok", "timestamp": )" + 
               std::to_string(std::time(nullptr)) + "}";
    }
    
    // POST /chat - 聊天接口 (echo)
    static std::string chat(const HttpRequest& req) {
        return R"({
    "echo": true,
    "received": ")" + req.body + R"(",
    "method": ")" + req.method + R"(",
    "path": ")" + req.path + R"("
})";
    }
    
    // GET /info - 请求信息
    static std::string info(const HttpRequest& req) {
        std::string headers_json = "{";
        for (const auto& [k, v] : req.headers) {
            if (headers_json != "{") headers_json += ",";
            headers_json += "\"" + k + "\":\"" + v + "\"";
        }
        headers_json += "}";
        
        return R"({
    "method": ")" + req.method + R"(",
    "path": ")" + req.path + R"(",
    "body_length": )" + std::to_string(req.body.length()) + R"(,
    "headers": )" + headers_json + R"(
})";
    }
};

int main() {
    try {
        boost::asio::io_context io;
        tcp::acceptor acceptor(io, tcp::endpoint(tcp::v4(), 8080));
        
        // 初始化路由
        Router router;
        router.add("/", Handlers::root);
        router.add("/health", Handlers::health);
        router.add("/chat", Handlers::chat);
        router.add("/info", Handlers::info);
        
        std::cout << "========================================" << std::endl;
        std::cout << "  NuClaw Step 1 - HTTP Router" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "Routes:" << std::endl;
        std::cout << "  GET  /        - Service status" << std::endl;
        std::cout << "  GET  /health  - Health check" << std::endl;
        std::cout << "  GET  /info    - Request info" << std::endl;
        std::cout << "  POST /chat    - Chat echo" << std::endl;
        std::cout << std::endl;
        std::cout << "Listening on http://localhost:8080" << std::endl;
        
        while (true) {
            tcp::socket socket(io);
            acceptor.accept(socket);
            
            char buffer[4096] = {};
            size_t len = socket.read_some(boost::asio::buffer(buffer));
            
            auto req = parse_request(std::string(buffer, len));
            auto response = router.handle(req);
            
            std::cout << "[" << req.method << " " << req.path << "] -> " 
                      << (response.find("200 OK") != std::string::npos ? "200" : "404") 
                      << std::endl;
            
            boost::asio::write(socket, boost::asio::buffer(response));
        }
        
    } catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
