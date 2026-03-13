// src/step02/main.cpp
// Step 2: HTTP 长连接优化 (Keep-Alive)
// 编译: mkdir build && cd build && cmake .. && make
// 运行: ./nuclaw_step02

#include <boost/asio.hpp>
#include <boost/algorithm/string.hpp>
#include <iostream>
#include <sstream>
#include <string>
#include <map>
#include <thread>
#include <vector>
#include <memory>
#include <chrono>

namespace asio = boost::asio;
using tcp = asio::ip::tcp;
using namespace std::chrono_literals;

// HTTP 请求
struct HttpRequest {
    std::string method;
    std::string path;
    std::string body;
    std::map<std::string, std::string> headers;
    bool keep_alive = false;
};

// 解析 HTTP 请求
HttpRequest parse_request(const std::string& raw) {
    HttpRequest req;
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
            boost::trim(key);
            boost::trim(value);
            req.headers[key] = value;
            
            // 检查 Keep-Alive
            if (boost::iequals(key, "Connection") && 
                boost::iequals(value, "keep-alive")) {
                req.keep_alive = true;
            }
        }
    }
    
    auto it = req.headers.find("Content-Length");
    if (it != req.headers.end()) {
        size_t body_start = raw.find("\r\n\r\n");
        if (body_start != std::string::npos) {
            req.body = raw.substr(body_start + 4, std::stoi(it->second));
        }
    }
    
    return req;
}

// 路由
class Router {
public:
    using Handler = std::function<std::string(const HttpRequest&)>;
    
    void add(const std::string& path, Handler handler) {
        routes_[path] = handler;
    }
    
    std::string handle(const HttpRequest& req) {
        auto it = routes_.find(req.path);
        if (it != routes_.end()) {
            return it->second(req);
        }
        return R"({"error":"not found"})";
    }
    
private:
    std::map<std::string, Handler> routes_;
};

// Session 支持 Keep-Alive
class Session : public std::enable_shared_from_this<Session> {
public:
    Session(tcp::socket socket, Router& router) 
        : socket_(std::move(socket)), router_(router) {}
    
    void start() {
        do_read();
    }
    
private:
    void do_read() {
        auto self = shared_from_this();
        
        // 设置读取超时
        timer_.expires_after(30s);
        timer_.async_wait([self](boost::system::error_code ec) {
            if (!ec) {
                self->socket_.close();
            }
        });
        
        socket_.async_read_some(
            asio::buffer(buffer_),
            [this, self](boost::system::error_code ec, std::size_t len) {
                timer_.cancel();
                
                if (ec) {
                    return; // 连接关闭或错误
                }
                
                auto req = parse_request(std::string(buffer_.data(), len));
                auto body = router_.handle(req);
                
                bool keep_alive = req.keep_alive;
                do_write(body, keep_alive);
            }
        );
    }
    
    void do_write(const std::string& body, bool keep_alive) {
        auto self = shared_from_this();
        
        std::string response = make_response(body, keep_alive);
        
        asio::async_write(socket_, asio::buffer(response),
            [this, self, keep_alive](boost::system::error_code ec, std::size_t) {
                if (ec) {
                    return;
                }
                
                if (keep_alive) {
                    // 保持连接，继续读取下一个请求
                    do_read();
                }
                // 否则关闭连接（socket 析构）
            }
        );
    }
    
    std::string make_response(const std::string& body, bool keep_alive) {
        std::string conn_header = keep_alive 
            ? "Connection: keep-alive\r\n" 
            : "Connection: close\r\n";
        
        return "HTTP/1.1 200 OK\r\n"
               "Content-Type: application/json\r\n"
               "Content-Length: " + std::to_string(body.length()) + "\r\n"
               + conn_header +
               "\r\n" + body;
    }
    
    tcp::socket socket_;
    Router& router_;
    asio::steady_timer timer_{socket_.get_executor()};
    std::array<char, 8192> buffer_;
};

// 服务器
class Server {
public:
    Server(asio::io_context& io, short port, Router& router)
        : acceptor_(io, tcp::endpoint(tcp::v4(), port)), router_(router) {
        do_accept();
    }
    
private:
    void do_accept() {
        acceptor_.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket) {
                if (!ec) {
                    std::make_shared<Session>(std::move(socket), router_)->start();
                }
                do_accept();
            }
        );
    }
    
    tcp::acceptor acceptor_;
    Router& router_;
};

int main() {
    try {
        asio::io_context io;
        
        Router router;
        router.add("/", [](const HttpRequest& req) {
            return R"({"status":"ok","step":2,"keep_alive":)" + 
                   std::string(req.keep_alive ? "true" : "false") + "}";
        });
        
        Server server(io, 8080, router);
        
        std::cout << "NuClaw Step 2 - HTTP Keep-Alive\n";
        std::cout << "Listening on http://localhost:8080\n";
        
        std::vector<std::thread> threads;
        for (unsigned i = 0; i < std::thread::hardware_concurrency(); ++i) {
            threads.emplace_back([&io]() { io.run(); });
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
    } catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    return 0;
}
