// src/step03/main.cpp
// Step 3: WebSocket + Agent Loop
// 编译: g++ -std=c++17 main.cpp -o server -lboost_system -lpthread

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <iostream>
#include <map>
#include <vector>
#include <memory>
#include <string>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace asio = boost::asio;
using tcp = asio::ip::tcp;

// Agent Loop: 管理对话历史
class AgentLoop {
public:
    struct Message {
        std::string role;    // user, assistant, system
        std::string content;
    };
    
    struct Session {
        std::string id;
        std::vector<Message> history;
    };
    
    std::string process(const std::string& input, const std::string& session_id) {
        auto& session = sessions_[session_id];
        session.id = session_id;
        
        // 添加用户消息
        session.history.push_back({"user", input});
        
        // 生成响应 (简化，后续调用 LLM)
        std::string response = "Echo: " + input + 
            " [session: " + std::to_string(session.history.size()) + " msgs]";
        
        session.history.push_back({"assistant", response});
        return response;
    }
    
private:
    std::map<std::string, Session> sessions_;
};

// WebSocket Session
class WsSession : public std::enable_shared_from_this<WsSession> {
public:
    WsSession(tcp::socket socket, AgentLoop& agent)
        : ws_(std::move(socket)), agent_(agent) {}
    
    void start() {
        ws_.async_accept(
            beast::bind_front_handler(&WsSession::on_accept, shared_from_this()));
    }
    
private:
    void on_accept(beast::error_code ec) {
        if (ec) return;
        session_id_ = "ws_" + std::to_string(reinterpret_cast<uintptr_t>(this));
        do_read();
    }
    
    void do_read() {
        ws_.async_read(buffer_,
            beast::bind_front_handler(&WsSession::on_read, shared_from_this()));
    }
    
    void on_read(beast::error_code ec, std::size_t) {
        if (ec) return;
        
        std::string msg = beast::buffers_to_string(buffer_.data());
        buffer_.consume(buffer_.size());
        
        std::string response = agent_.process(msg, session_id_);
        
        ws_.text(true);
        ws_.async_write(asio::buffer(response),
            beast::bind_front_handler(&WsSession::on_write, shared_from_this()));
    }
    
    void on_write(beast::error_code ec, std::size_t) {
        if (ec) return;
        do_read();
    }
    
    websocket::stream<tcp::socket> ws_;
    AgentLoop& agent_;
    beast::flat_buffer buffer_;
    std::string session_id_;
};

// HTTP Server
class HttpSession : public std::enable_shared_from_this<HttpSession> {
public:
    HttpSession(tcp::socket socket) : socket_(std::move(socket)) {}
    
    void start() {
        do_read();
    }
    
private:
    void do_read() {
        auto self = shared_from_this();
        socket_.async_read_some(asio::buffer(buffer_),
            [this, self](boost::system::error_code ec, std::size_t len) {
                if (!ec) {
                    std::string response = 
                        "HTTP/1.1 200 OK\r\n"
                        "Content-Type: application/json\r\n"
                        "Content-Length: 50\r\n"
                        "\r\n"
                        R"({"status":"ok","step":3,"websocket":"ws://localhost:8081"})";
                    
                    asio::async_write(socket_, asio::buffer(response),
                        [this, self](boost::system::error_code, std::size_t) {
                            socket_.close();
                        });
                }
            });
    }
    
    tcp::socket socket_;
    std::array<char, 4096> buffer_;
};

// 服务器
class Server {
public:
    Server(asio::io_context& io, short port, bool is_ws)
        : acceptor_(io, tcp::endpoint(tcp::v4(), port)), is_ws_(is_ws) {
        do_accept();
    }
    
private:
    void do_accept() {
        acceptor_.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket) {
                if (!ec) {
                    if (is_ws_) {
                        std::make_shared<WsSession>(std::move(socket), agent_)->start();
                    } else {
                        std::make_shared<HttpSession>(std::move(socket))->start();
                    }
                }
                do_accept();
            });
    }
    
    tcp::acceptor acceptor_;
    bool is_ws_;
    AgentLoop agent_;
};

int main() {
    try {
        asio::io_context io;
        
        // HTTP on 8080, WebSocket on 8081
        Server http_server(io, 8080, false);
        Server ws_server(io, 8081, true);
        
        std::cout << "NuClaw Step 3 - WebSocket + Agent Loop\n";
        std::cout << "HTTP:     http://localhost:8080\n";
        std::cout << "WebSocket: ws://localhost:8081\n";
        
        std::vector<std::thread> threads;
        auto count = std::thread::hardware_concurrency();
        for (unsigned i = 0; i < count; ++i) {
            threads.emplace_back([&io]() { io.run(); });
        }
        for (auto& t : threads) t.join();
        
    } catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    return 0;
}
