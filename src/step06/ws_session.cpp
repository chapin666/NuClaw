// ============================================================================
// ws_session.cpp - WebSocket Session 实现
// ============================================================================

#include "ws_session.hpp"
#include <iostream>

WsSession::WsSession(tcp::socket socket, AgentLoop& agent)
    : ws_(std::move(socket)), agent_(agent) {}

void WsSession::start() {
    ws_.async_accept([self = shared_from_this()](beast::error_code ec) {
        if (!ec) self->do_read();
    });
}

void WsSession::do_read() {
    ws_.async_read(buffer_, [self = shared_from_this()](beast::error_code ec, std::size_t) {
        if (ec) return;
        std::string msg = beast::buffers_to_string(self->buffer_.data());
        self->buffer_.consume(self->buffer_.size());
        
        std::cout << "[<] " << msg << "\n";
        std::string response = self->agent_.process(msg);
        
        self->ws_.text(true);
        self->ws_.async_write(asio::buffer(response),
            [self](beast::error_code, std::size_t) { self->do_read(); });
    });
}
