// ============================================================================
// ws_session.hpp - WebSocket Session 管理
// ============================================================================
//
// 职责：
//   - 管理单个客户端的 WebSocket 连接
//   - 处理消息的收发
//   - 与 AgentLoop 交互
//
// 生命周期：
//   创建 -> 接受连接 -> 读取消息 -> Agent 处理 -> 发送响应 -> 循环 -> 断开
//
// 设计要点：
//   - 继承 enable_shared_from_this：确保异步回调期间对象存活
//   - 每个 Session 独立：一个客户端一个 Session
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

// ----------------------------------------------------------------------------
// WebSocket Session 类
// ----------------------------------------------------------------------------
class WsSession : public std::enable_shared_from_this<WsSession> {
public:
    // ---------------------------------------------------------------------
    // 构造函数
    // @param socket: 已连接的 TCP socket（由 Server 传递）
    // @param agent:  AgentLoop 引用（处理业务逻辑）
    // ---------------------------------------------------------------------
    WsSession(tcp::socket socket, AgentLoop& agent);
    
    // 启动 Session（开始 WebSocket 握手）
    void start();

private:
    // ---------------------------------------------------------------------
    // 异步回调方法
    // ---------------------------------------------------------------------
    
    // WebSocket 握手完成回调
    void on_accept(beast::error_code ec);
    
    // 读取完成回调
    void on_read(beast::error_code ec, std::size_t bytes_transferred);
    
    // 写入完成回调
    void on_write(beast::error_code ec, std::size_t bytes_transferred);
    
    // 启动异步读取
    void do_read();
    
    // ---------------------------------------------------------------------
    // 成员变量
    // ---------------------------------------------------------------------
    
    // WebSocket 流（包装 TCP socket，提供 WebSocket 协议支持）
    websocket::stream<tcp::socket> ws_;
    
    // AgentLoop 引用（业务逻辑）
    AgentLoop& agent_;
    
    // 读取缓冲区（Beast 的动态缓冲区，自动管理内存）
    beast::flat_buffer buffer_;
    
    // 会话 ID（用于关联 Agent 的 Session）
    std::string session_id_;
};
