// ============================================================================
// Step 2: HTTP 协议与路由系统
// ============================================================================
// 演进说明：
//   基于 Step 1 增加 HTTP 协议解析和路由功能
//   1. 新增 HttpRequest 结构体解析 HTTP 请求（method, path, headers）
//   2. 新增 Router 类支持动态路由注册（add_route）
//   3. Session 增加 HTTP 解析逻辑
//   4. 支持不同路径返回不同响应
// 编译: g++ -std=c++17 main.cpp -o server -lboost_system -lpthread
// 运行: ./server
// 测试: curl http://localhost:8080
//       curl http://localhost:8080/hello
//       curl http://localhost:8080/api/status
// ============================================================================

#include <boost/asio.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <sstream>
#include <map>
#include <functional>

// 简化命名空间
using boost::asio::ip::tcp;
namespace asio = boost::asio;

// ============================================================================
// 新增：HTTP 请求结构体
// ============================================================================
// 解析后的 HTTP 请求数据结构
// ============================================================================
struct HttpRequest {
    std::string method;                           // GET, POST, etc.
    std::string path;                             // 请求路径
    std::map<std::string, std::string> headers;    // HTTP 请求头
    std::string body;                             // 请求体
};

// ============================================================================
// 新增：HTTP 请求解析函数
// ============================================================================
// 简单解析 HTTP 请求字符串，提取 method, path, headers
// ============================================================================
HttpRequest parse_http_request(const char* data, size_t len) {
    HttpRequest req;
    std::string raw(data, len);
    std::istringstream stream(raw);
    std::string line;
    
    // 解析请求行: GET /path HTTP/1.1
    if (std::getline(stream, line)) {
        std::istringstream line_stream(line);
        line_stream >> req.method >> req.path;
    }
    
    // 解析请求头: Key: Value
    while (std::getline(stream, line) && !line.empty() && line != "\r") {
        auto pos = line.find(':');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            // 去除空格
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t\r") + 1);
            req.headers[key] = value;
        }
    }
    
    return req;
}

// ============================================================================
// 新增：Router 类 - 路由系统
// ============================================================================
// 支持注册不同路径的处理函数
// ============================================================================
class Router {
public:
    // 路由处理函数类型
    using Handler = std::function<std::string(const HttpRequest&)>;
    
    // 注册路由：path -> handler
    void add_route(const std::string& path, Handler handler) {
        routes_[path] = handler;
    }
    
    // 处理请求：根据 path 查找对应的 handler
    std::string handle(const HttpRequest& req) const {
        auto it = routes_.find(req.path);
        if (it != routes_.end()) {
            return it->second(req);  // 调用对应 handler
        }
        // 404 响应
        return R"({"error": "not found", "path": ")" + req.path + "\"}";
    }
    
    // 获取注册的路由列表（用于调试）
    std::vector<std::string> get_routes() const {
        std::vector<std::string> result;
        for (const auto& [path, _] : routes_) {
            result.push_back(path);
        }
        return result;
    }

private:
    std::map<std::string, Handler> routes_;  // 路由表
};

// ============================================================================
// 修改：Session 类 - 增加 HTTP 解析逻辑
// ============================================================================
class Session : public std::enable_shared_from_this<Session> {
public:
    // 修改：构造函数增加 Router 引用
    Session(tcp::socket socket, const Router& router)
        : socket_(std::move(socket)), router_(router) {}
    
    void start() {
        do_read();
    }

private:
    void do_read() {
        auto self = shared_from_this();
        
        socket_.async_read_some(
            asio::buffer(buffer_, sizeof(buffer_)),
            [this, self](boost::system::error_code ec, std::size_t len) {
                if (!ec) {
                    // ============================================================================
                    // 新增：HTTP 请求解析
                    // ============================================================================
                    HttpRequest req = parse_http_request(buffer_, len);
                    
                    std::cout << "[+] " << req.method << " " << req.path << std::endl;
                    
                    // 通过 Router 获取响应
                    auto response = make_response(router_.handle(req));
                    do_write(response);
                }
            }
        );
    }
    
    void do_write(const std::string& response) {
        auto self = shared_from_this();
        
        asio::async_write(socket_, asio::buffer(response),
            [this, self](boost::system::error_code, std::size_t) {
                // 短连接模式：发送后关闭
            }
        );
    }
    
    // 构造 HTTP 响应
    std::string make_response(const std::string& body) {
        return "HTTP/1.1 200 OK\r\n"
               "Content-Type: application/json\r\n"
               "Content-Length: " + std::to_string(body.length()) + "\r\n"
               "Connection: close\r\n"
               "\r\n" + body;
    }
    
    tcp::socket socket_;
    char buffer_[1024] = {};
    const Router& router_;  // 新增：Router 引用
};

// ============================================================================
// 修改：Server 类 - 增加 Router
// ============================================================================
class Server {
public:
    Server(asio::io_context& io, short port, const Router& router)
        : acceptor_(io, tcp::endpoint(tcp::v4(), port)), router_(router) {
        std::cout << "[i] Server binding to port " << port << std::endl;
        do_accept();
    }

private:
    void do_accept() {
        acceptor_.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket) {
                if (!ec) {
                    std::cout << "[+] New connection accepted" << std::endl;
                    // 修改：传递 router_ 给 Session
                    std::make_shared<Session>(std::move(socket), router_)->start();
                }
                do_accept();
            }
        );
    }
    
    tcp::acceptor acceptor_;
    const Router& router_;  // 新增：Router 引用
};

int main() {
    try {
        asio::io_context io;
        
        std::cout << "========================================" << std::endl;
        std::cout << "  NuClaw Step 2 - HTTP Router" << std::endl;
        std::cout << "========================================" << std::endl;
        
        // ============================================================================
        // 新增：配置路由
        // ============================================================================
        Router router;
        
        // 根路径
        router.add_route("/", [](const HttpRequest& req) {
            return R"({
    "status": "ok",
    "step": 2,
    "message": "HTTP Router working!",
    "path": "/"
})";
        });
        
        // /hello 路径
        router.add_route("/hello", [](const HttpRequest& req) {
            return R"({"message": "Hello from Router!"})";
        });
        
        // /api/status 路径
        router.add_route("/api/status", [](const HttpRequest& req) {
            return R"({"status": "running", "version": "2.0"})";
        });
        
        // 打印注册的路由
        std::cout << "[i] Registered routes:" << std::endl;
        for (const auto& path : router.get_routes()) {
            std::cout << "    " << path << std::endl;
        }
        
        // 修改：传递 router 给 Server
        Server server(io, 8080, router);
        
        std::cout << "Server listening on http://localhost:8080" << std::endl;
        std::cout << "Press Ctrl+C to stop" << std::endl;
        std::cout << std::endl;
        
        // 多线程运行
        std::vector<std::thread> threads;
        auto thread_count = std::thread::hardware_concurrency();
        
        std::cout << "[i] Starting " << thread_count << " worker threads..." << std::endl;
        
        for (unsigned i = 0; i < thread_count; ++i) {
            threads.emplace_back([&io]() { io.run(); });
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
    } catch (std::exception& e) {
        std::cerr << "[!] Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
