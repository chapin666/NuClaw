// ============================================================================
// ws_session.hpp - WebSocket Session
// ============================================================================

#pragma once

#include "agent_loop.hpp"
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <memory>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace asio = boost::asio;
using tcp = asio::ip::tcp;

class WsSession : public std::enable_shared_from_this<WsSession> {
public:
    WsSession(tcp::socket socket, AgentLoop& agent);
    void start();

private:
    void do_read();
    
    websocket::stream<tcp::socket> ws_;
    AgentLoop& agent_;
    beast::flat_buffer buffer_;
};
