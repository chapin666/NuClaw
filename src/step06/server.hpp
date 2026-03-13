// ============================================================================
// server.hpp/cpp - WebSocket 服务器
// ============================================================================

#pragma once

#include "chat_engine.hpp"
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/websocket.hpp>
#include <memory>
#include <string>

using boost::asio::ip::tcp;
namespace asio = boost::asio;
namespace beast = boost::beast;
namespace websocket = beast::websocket;

// WebSocket Session
class WebSocketSession : public std::enable_shared_from_this<WebSocketSession> {
public:
    WebSocketSession(tcp::socket socket, ChatEngine& ai)
        : ws_(std::move(socket)), ai_(ai) {}
    
    void start() {
        ws_.async_accept(
            beast::bind_front_handler(&WebSocketSession::on_accept, shared_from_this()));
    }

private:
    void on_accept(beast::error_code ec) {
        if (ec) return;
        
        ws_.text(true);
        ws_.async_write(asio::buffer(ai_.get_welcome_message()),
            beast::bind_front_handler(&WebSocketSession::on_write, shared_from_this()));
    }
    
    void do_read() {
        ws_.async_read(buffer_,
            beast::bind_front_handler(&WebSocketSession::on_read, shared_from_this()));
    }
    
    void on_read(beast::error_code ec, std::size_t) {
        if (ec == websocket::error::closed) return;
        if (ec) return;
        
        std::string msg = beast::buffers_to_string(buffer_.data());
        buffer_.consume(buffer_.size());
        
        std::string reply = ai_.process(msg, ctx_);
        
        ws_.text(true);
        ws_.async_write(asio::buffer(reply),
            beast::bind_front_handler(&WebSocketSession::on_write, shared_from_this()));
    }
    
    void on_write(beast::error_code ec, std::size_t) {
        if (!ec) do_read();
    }
    
    websocket::stream<tcp::socket> ws_;
    ChatEngine& ai_;
    beast::flat_buffer buffer_;
    ChatContext ctx_;  // 连接期间保持的上下文
};

// HTTP 请求解析
struct HttpRequest {
    std::string method, path;
    std::map<std::string, std::string> headers;
    bool is_websocket_upgrade() const {
        auto it = headers.find("Upgrade");
        return it != headers.end() && it->second == "websocket";
    }
};

inline HttpRequest parse_http_request(const char* data, size_t len) {
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

// Session（处理 HTTP 和 WebSocket 升级）
class Session : public std::enable_shared_from_this<Session> {
public:
    Session(tcp::socket socket, ChatEngine& ai)
        : socket_(std::move(socket)), ai_(ai) {}
    
    void start() {
        socket_.async_read_some(asio::buffer(buffer_, sizeof(buffer_)),
            [this, self = shared_from_this()](boost::system::error_code ec, std::size_t len) {
                if (!ec) {
                    auto req = parse_http_request(buffer_, len);
                    if (req.is_websocket_upgrade()) {
                        std::make_shared<WebSocketSession>(std::move(socket_), ai_)->start();
                    } else {
                        std::string body = R"({"status": "ok", "step": 6})";
                        std::string response = "HTTP/1.1 200 OK\r\nContent-Length: " + 
                                               std::to_string(body.length()) + "\r\n\r\n" + body;
                        asio::async_write(socket_, asio::buffer(response), 
                            [](boost::system::error_code, std::size_t) {});
                    }
                }
            });
    }

private:
    tcp::socket socket_;
    ChatEngine& ai_;
    char buffer_[4096] = {};
};

// Server
class Server {
public:
    Server(asio::io_context& io, short port, ChatEngine& ai)
        : acceptor_(io, tcp::endpoint(tcp::v4(), port)), ai_(ai) {
        do_accept();
    }

private:
    void do_accept() {
        acceptor_.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket) {
                if (!ec) {
                    std::make_shared<Session>(std::move(socket), ai_)->start();
                }
                do_accept();
            });
    }
    
    tcp::acceptor acceptor_;
    ChatEngine& ai_;
};
