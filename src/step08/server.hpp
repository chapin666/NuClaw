// ============================================================================
// server.hpp - WebSocket Server 和 Agent
// ============================================================================

#pragma once

#include "registry.hpp"
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <memory>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace asio = boost::asio;
using tcp = asio::ip::tcp;

// ----------------------------------------------------------------------------
// Agent - 整合注册表和消息处理
// ----------------------------------------------------------------------------
class Agent {
public:
    Agent();
    
    // 处理输入消息，返回 JSON 响应
    std::string process(const std::string& input);
    
    // 获取注册表引用
    ToolRegistry& get_registry() { return registry_; }

private:
    ToolRegistry registry_;
};

// ----------------------------------------------------------------------------
// WebSocket Session
// ----------------------------------------------------------------------------
class WsSession : public std::enable_shared_from_this<WsSession> {
public:
    WsSession(tcp::socket socket, Agent& agent);
    void start();

private:
    void do_read();
    void on_read(beast::error_code ec);
    void on_write(beast::error_code ec);
    
    websocket::stream<tcp::socket> ws_;
    Agent& agent_;
    beast::flat_buffer buffer_;
};

// ----------------------------------------------------------------------------
// TCP Server
// ----------------------------------------------------------------------------
class Server {
public:
    Server(asio::io_context& io, short port, Agent& agent);

private:
    void do_accept();
    
    tcp::acceptor acceptor_;
    Agent& agent_;
};
