// ============================================================================
// Step 3: WebSocket 支持
// ============================================================================
// 演进说明：
//   基于 Step 2 增加 WebSocket 协议支持
//   1. 新增 is_websocket_upgrade() 检测 HTTP Upgrade 头
//   2. 新增 WebSocketSession 类处理 WebSocket 连接
//   3. Session 检测升级请求并路由到 WebSocketSession
// 编译: g++ -std=c++17 main.cpp -o server -lboost_system -lpthread
// 运行: ./server
// HTTP测试: curl http://localhost:8080
// WebSocket测试: wscat -c ws://localhost:8080/ws
// ============================================================================

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/websocket.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <sstream>
#include <map>
#include <functional>

using boost::asio::ip::tcp;
namespace asio = boost::asio;
namespace beast = boost::beast;
namespace websocket = beast::websocket;

// ============================================================================
// HTTP 请求结构体
// ============================================================================
struct HttpRequest {
    std::string method;
    std::string path;
    std::map<std::string, std::string> headers;
    std::string body;
    
    // ============================================================================
    // 新增：检测是否为 WebSocket 升级请求
    // ============================================================================
    bool is_websocket_upgrade() const {
        auto it = headers.find("Upgrade");
        return it != headers.end() && it->second == "websocket";
    }
};

// ============================================================================
// HTTP 请求解析
// ============================================================================
HttpRequest parse_http_request(const char* data, size_t len) {
    HttpRequest req;
    std::string raw(data, len);
    std::istringstream stream(raw);
    std::string line;
    
    if (std::getline(stream, line)) {
        std::istringstream line_stream(line);
        line_stream >> req.method >> req.path;
    }
    
    while (std::getline(stream, line) && !line.empty() && line != "\r") {
        auto pos = line.find(':');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
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
// Router 类
// ============================================================================
class Router {
public:
    using Handler = std::function<std::string(const HttpRequest&)>;
    
    void add_route(const std::string& path, Handler handler) {
        routes_[path] = handler;
    }
    
    std::string handle(const HttpRequest& req) const {
        auto it = routes_.find(req.path);
        if (it != routes_.end()) {
            return it->second(req);
        }
        return R"({"error": "not found", "path": ")" + req.path + "\"}";
    }

private:
    std::map<std::string, Handler> routes_;
};

// ============================================================================
// 新增：WebSocket Session 类
// ============================================================================
class WebSocketSession : public std::enable_shared_from_this<WebSocketSession> {
public:
    WebSocketSession(tcp::socket socket) : ws_(std::move(socket)) {}
    
    void start() {
        ws_.async_accept(
            beast::bind_front_handler(&WebSocketSession::on_accept, shared_from_this()));
    }

private:
    void on_accept(beast::error_code ec) {
        if (ec) {
            std::cerr << "[!] WebSocket accept failed: " << ec.message() << std::endl;
            return;
        }
        std::cout << "[+] WebSocket connection established" << std::endl;
        do_read();
    }
    
    void do_read() {
        ws_.async_read(buffer_,
            beast::bind_front_handler(&WebSocketSession::on_read, shared_from_this()));
    }
    
    void on_read(beast::error_code ec, std::size_t bytes) {
        if (ec == websocket::error::closed) {
            std::cout << "[-] WebSocket closed" << std::endl;
            return;
        }
        if (ec) {
            std::cerr << "[!] WebSocket read error: " << ec.message() << std::endl;
            return;
        }
        
        std::string msg = beast::buffers_to_string(buffer_.data());
        buffer_.consume(buffer_.size());
        std::cout << "[<] WS: " << msg << std::endl;
        
        std::string reply = "Echo: " + msg;
        ws_.text(true);
        ws_.async_write(asio::buffer(reply),
            beast::bind_front_handler(&WebSocketSession::on_write, shared_from_this()));
    }
    
    void on_write(beast::error_code ec, std::size_t bytes) {
        if (ec) {
            std::cerr << "[!] WebSocket write error: " << ec.message() << std::endl;
            return;
        }
        std::cout << "[>] WS sent " << bytes << " bytes" << std::endl;
        do_read();
    }
    
    websocket::stream<tcp::socket> ws_;
    beast::flat_buffer buffer_;
};

// ============================================================================
// Session 类
// ============================================================================
class Session : public std::enable_shared_from_this<Session> {
public:
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
                    HttpRequest req = parse_http_request(buffer_, len);
                    std::cout << "[+] " << req.method << " " << req.path << std::endl;
                    
                    // ============================================================================
                    // 新增：检测 WebSocket 升级请求
                    // ============================================================================
                    if (req.is_websocket_upgrade()) {
                        std::cout << "[i] Upgrading to WebSocket" << std::endl;
                        std::make_shared<WebSocketSession>(
                            std::move(socket_))->start();
                    } else {
                        auto response = make_response(router_.handle(req));
                        do_write(response);
                    }
                }
            }
        );
    }
    
    void do_write(const std::string& response) {
        auto self = shared_from_this();
        asio::async_write(socket_, asio::buffer(response),
            [this, self](boost::system::error_code, std::size_t) {}
        );
    }
    
    std::string make_response(const std::string& body) {
        return "HTTP/1.1 200 OK\r\n"
               "Content-Type: application/json\r\n"
               "Content-Length: " + std::to_string(body.length()) + "\r\n"
               "Connection: close\r\n"
               "\r\n" + body;
    }
    
    tcp::socket socket_;
    char buffer_[1024] = {};
    const Router& router_;
};

// ============================================================================
// Server 类
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
                    std::make_shared<Session>(std::move(socket), router_)->start();
                }
                do_accept();
            }
        );
    }
    
    tcp::acceptor acceptor_;
    const Router& router_;
};

int main() {
    try {
        asio::io_context io;
        
        std::cout << "========================================" << std::endl;
        std::cout << "  NuClaw Step 3 - WebSocket Support" << std::endl;
        std::cout << "========================================" << std::endl;
        
        Router router;
        router.add_route("/", [](const HttpRequest& req) {
            return R"({
    "status": "ok",
    "step": 3,
    "message": "WebSocket Server ready!",
    "endpoints": ["/", "/hello"]
})";
        });
        
        router.add_route("/hello", [](const HttpRequest& req) {
            return R"({"message": "Hello from Step 3!"})";
        });
        
        Server server(io, 8080, router);
        
        std::cout << "Server listening on http://localhost:8080" << std::endl;
        std::cout << "WebSocket endpoint: ws://localhost:8080/ws" << std::endl;
        std::cout << "Press Ctrl+C to stop" << std::endl;
        std::cout << std::endl;
        
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
