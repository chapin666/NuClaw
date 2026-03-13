// ============================================================================
// server.hpp - WebSocket 服务器
// ============================================================================
//
// 职责：
//   - 监听端口
//   - 接受新连接
//   - 创建 WsSession 处理连接
//
// 设计模式：Acceptor 模式（Asio 的标准做法）
// ============================================================================

#pragma once

#include "ws_session.hpp"
#include <boost/asio.hpp>

namespace asio = boost::asio;
using tcp = asio::ip::tcp;

// ----------------------------------------------------------------------------
// WebSocket Server 类
// ----------------------------------------------------------------------------
class Server {
public:
    // ---------------------------------------------------------------------
    // 构造函数
    // @param io:     Asio 事件循环（io_context）
    // @param port:   监听端口
    // @param agent:  AgentLoop 引用（传递给每个 Session）
    // ---------------------------------------------------------------------
    Server(asio::io_context& io, short port, AgentLoop& agent);

private:
    // 启动异步接受
    void do_accept();
    
    // 接受完成回调
    void on_accept(boost::system::error_code ec, tcp::socket socket);
    
    // TCP 接受器（用于监听和接受连接）
    tcp::acceptor acceptor_;
    
    // AgentLoop 引用
    AgentLoop& agent_;
};
