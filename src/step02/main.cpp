// src/step02/main.cpp
// Step 2: 异步 I/O 和多线程
// 编译: g++ -std=c++17 main.cpp -o server -lboost_system -lboost_thread -lpthread

#include <boost/asio.hpp>
#include <boost/algorithm/string.hpp>
#include <iostream>
#include <sstream>
#include <string>
#include <map>
#include <thread>
#include <vector>
#include <memory>

namespace asio = boost::asio;
using tcp = asio::ip::tcp;

// HTTP 请求
struct HttpRequest {
    std::string method;
    std::string path;
    std::string body;
    std::map<std::string, std::string> headers;
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

// 路由系统
class Router {
public:
    using Handler = std::function<std::string(const HttpRequest&)>;
    
    void add(const std::string& path, Handler handler) {
        routes_[path] = handler;
    }
    
    std::string handle(const HttpRequest& req) {
        auto it = routes_.find(req.path);
        if (it != routes_.end()) {
            return make_response(200, it->second(req));
        }
        return make_response(404, R"({"error":"not found"})");
    }
    
private:
    std::string make_response(int code, const std::string& body) {
        std::string status = (code == 200) ? "200 OK" : "404 Not Found";
        return "HTTP/1.1 " + status + "\r\n"
               "Content-Type: application/json\r\n"
               "Content-Length: " + std::to_string(body.length()) + "\r\n"
               "Connection: close\r\n"
               "\r\n" + body;
    }
    
    std::map<std::string, Handler> routes_;
};

// Session 类：异步处理单个连接
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
        socket_.async_read_some(
            asio::buffer(buffer_),
            [this, self](boost::system::error_code ec, std::size_t len) {
                if (!ec) {
                    auto req = parse_request(std::string(buffer_.data(), len));
                    auto response = router_.handle(req);
                    do_write(response);
                }
            }
        );
    }
    
    void do_write(const std::string& response) {
        auto self = shared_from_this();
        asio::async_write(socket_, asio::buffer(response),
            [this, self](boost::system::error_code, std::size_t) {
                socket_.close();
            }
        );
    }
    
    tcp::socket socket_;
    Router& router_;
    std::array<char, 8192> buffer_;
};

// 服务器类
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
        router.add("/", [](const HttpRequest&) {
            return R"({"status":"ok","step":2,"features":"async+multithread"})";
        });
        router.add("/health", [](const HttpRequest&) {
            return R"({"health":"ok","threads":)" + 
                   std::to_string(std::thread::hardware_concurrency()) + "}";
        });
        
        Server server(io, 8080, router);
        
        std::cout << "NuClaw Step 2 - Async I/O\n";
        std::cout << "Listening on http://localhost:8080\n";
        
        // 多线程运行 io_context
        std::vector<std::thread> threads;
        auto count = std::thread::hardware_concurrency();
        for (unsigned i = 0; i < count; ++i) {
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
