// ============================================================================
// server.cpp - Server 实现
// ============================================================================

#include "server.hpp"
#include "http_tool.hpp"
#include "file_tool.hpp"
#include "code_tool.hpp"
#include <iostream>

// ----------------------------------------------------------------------------
// Agent
// ----------------------------------------------------------------------------
Agent::Agent() {
    registry_.register_tool(std::make_shared<HttpGetTool>());
    registry_.register_tool(std::make_shared<FileTool>());
    registry_.register_tool(std::make_shared<CodeExecuteTool>());
}

std::string Agent::process(const std::string& input) {
    try {
        auto json_input = json::parse(input);
        
        if (json_input.is_string() && json_input.as_string() == "list") {
            return json::serialize(registry_.list_tools());
        }
        
        auto obj = json_input.as_object();
        std::string tool_name(obj.at("tool").as_string());
        json::object args = obj.at("args").as_object();
        
        auto tool = registry_.get_tool(tool_name);
        if (!tool) {
            json::object error;
            error["success"] = false;
            error["error"] = "Tool not found: " + tool_name;
            return json::serialize(error);
        }
        
        auto result = tool->execute_safe(args);
        return json::serialize(result);
        
    } catch (const std::exception& e) {
        json::object error;
        error["success"] = false;
        error["error"] = e.what();
        return json::serialize(error);
    }
}

// ----------------------------------------------------------------------------
// WsSession
// ----------------------------------------------------------------------------
WsSession::WsSession(tcp::socket socket, Agent& agent)
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
    
    std::string response = agent_.process(msg);
    
    ws_.text(true);
    ws_.async_write(asio::buffer(response), 
        [self = shared_from_this()](beast::error_code ec, std::size_t) {
            self->on_write(ec);
        });
}

void WsSession::on_write(beast::error_code ec) {
    if (!ec) do_read();
}

// ----------------------------------------------------------------------------
// Server
// ----------------------------------------------------------------------------
Server::Server(asio::io_context& io, short port, Agent& agent)
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
