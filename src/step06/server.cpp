// ============================================================================
// server.cpp - Server 实现
// ============================================================================

#include "server.hpp"
#include <iostream>

// ----------------------------------------------------------------------------
// AgentLoop
// ----------------------------------------------------------------------------
AgentLoop::AgentLoop(const ToolRegistry& registry) : registry_(registry) {}

std::string AgentLoop::process(const std::string& input) {
    try {
        auto json_input = json::parse(input);
        
        if (json_input.is_object()) {
            auto obj = json_input.as_object();
            std::string tool_name = std::string(obj.at("tool").as_string());
            json::object args = obj.contains("args") 
                ? obj.at("args").as_object() : json::object{};
            
            std::cout << "[⚙️ Executing] " << tool_name << "\n";
            auto result = registry_.execute(tool_name, args);
            return json::serialize(result);
        }
    } catch (const std::exception& e) {
        json::object error;
        error["success"] = false;
        error["error"] = "Parse error: " + std::string(e.what());
        return json::serialize(error);
    }
    
    json::object help;
    help["message"] = "Send JSON: {\"tool\": \"name\", \"args\": {...}}";
    help["available_tools"] = registry_.get_all_schemas();
    return json::serialize(help);
}

// ----------------------------------------------------------------------------
// WsSession
// ----------------------------------------------------------------------------
WsSession::WsSession(tcp::socket socket, AgentLoop& agent)
    : ws_(std::move(socket)), agent_(agent) {}

void WsSession::start() {
    ws_.async_accept([self = shared_from_this()](beast::error_code ec) {
        if (!ec) self->do_read();
    });
}

void WsSession::do_read() {
    ws_.async_read(buffer_, [self = shared_from_this()](beast::error_code ec, std::size_t) {
        self->on_read(ec);
    });
}

void WsSession::on_read(beast::error_code ec) {
    if (ec) return;
    
    std::string msg = beast::buffers_to_string(buffer_.data());
    buffer_.consume(buffer_.size());
    
    std::cout << "[<] " << msg << "\n";
    std::string response = agent_.process(msg);
    
    ws_.text(true);
    ws_.async_write(asio::buffer(response), [self = shared_from_this()](beast::error_code ec, std::size_t) {
        self->on_write(ec);
    });
}

void WsSession::on_write(beast::error_code ec) {
    if (!ec) do_read();
}

// ----------------------------------------------------------------------------
// Server
// ----------------------------------------------------------------------------
Server::Server(asio::io_context& io, short port, AgentLoop& agent)
    : acceptor_(io, tcp::endpoint(tcp::v4(), port)), agent_(agent) {
    do_accept();
}

void Server::do_accept() {
    acceptor_.async_accept([this](boost::system::error_code ec, tcp::socket socket) {
        if (!ec) {
            std::make_shared<WsSession>(std::move(socket), agent_)->start();
        }
        do_accept();
    });
}
