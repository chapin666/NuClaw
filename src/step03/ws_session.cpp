// ============================================================================
// ws_session.cpp - WebSocket Session 实现
// ============================================================================

#include "ws_session.hpp"
#include <iostream>

// ----------------------------------------------------------------------------
// 构造函数
// ----------------------------------------------------------------------------
WsSession::WsSession(tcp::socket socket, AgentLoop& agent)
    : ws_(std::move(socket))   // 将 socket 移入 WebSocket 流
    , agent_(agent) {
    // 注意：这里不启动读写，等到 start() 被调用后再开始
}

// ----------------------------------------------------------------------------
// 启动 Session
// ----------------------------------------------------------------------------
void WsSession::start() {
    // async_accept: 执行 WebSocket 握手
    // 客户端发送 HTTP Upgrade 请求，服务器回应 101 Switching Protocols
    ws_.async_accept(
        // beast::bind_front_handler: 将成员函数绑定为回调
        // 参数：成员函数指针，shared_ptr（保持对象存活）
        beast::bind_front_handler(
            &WsSession::on_accept,  
            shared_from_this()       // 关键！确保回调执行时对象仍存在
        )
    );
}

// ----------------------------------------------------------------------------
// 握手完成回调
// ----------------------------------------------------------------------------
void WsSession::on_accept(beast::error_code ec) {
    if (ec) {
        std::cerr << "[✗] WebSocket accept failed: " << ec.message() << "\n";
        return;
    }
    
    // 生成唯一的会话 ID
    // 使用 this 指针的地址作为 ID（简单有效）
    session_id_ = "ws_" + std::to_string(reinterpret_cast<uintptr_t>(this));
    
    std::cout << "[+] New connection: " << session_id_ << "\n";
    
    // 开始读取消息
    do_read();
}

// ----------------------------------------------------------------------------
// 启动异步读取
// ----------------------------------------------------------------------------
void WsSession::do_read() {
    // async_read: 读取一条完整的 WebSocket 消息
    // 注意：WebSocket 是帧协议，Beast 会自动组装帧
    ws_.async_read(
        buffer_,  // 读取到此缓冲区
        beast::bind_front_handler(
            &WsSession::on_read,
            shared_from_this()
        )
    );
}

// ----------------------------------------------------------------------------
// 读取完成回调
// ----------------------------------------------------------------------------
void WsSession::on_read(beast::error_code ec, std::size_t bytes_transferred) {
    // 检查错误
    if (ec == websocket::error::closed) {
        // 客户端正常关闭连接
        std::cout << "[-] Connection closed: " << session_id_ << "\n";
        return;
    }
    
    if (ec) {
        std::cerr << "[✗] Read error: " << ec.message() << "\n";
        return;
    }
    
    // 从缓冲区提取消息（转换为字符串）
    std::string msg = beast::buffers_to_string(buffer_.data());
    buffer_.consume(buffer_.size());  // 清空缓冲区（重要！）
    
    std::cout << "[<] " << session_id_ << ": " << msg << "\n";
    
    // ---------------------------------------------------------------------
    // 调用 Agent 处理消息
    // ---------------------------------------------------------------------
    std::string response = agent_.process(msg, session_id_);
    
    // ---------------------------------------------------------------------
    // 发送响应
    // ---------------------------------------------------------------------
    ws_.text(true);  // 设置消息类型为文本（而非二进制）
    
    ws_.async_write(
        asio::buffer(response),
        beast::bind_front_handler(
            &WsSession::on_write,
            shared_from_this()
        )
    );
}

// ----------------------------------------------------------------------------
// 写入完成回调
// ----------------------------------------------------------------------------
void WsSession::on_write(beast::error_code ec, std::size_t bytes_transferred) {
    if (ec) {
        std::cerr << "[✗] Write error: " << ec.message() << "\n";
        return;
    }
    
    std::cout << "[>] Sent " << bytes_transferred << " bytes to " << session_id_ << "\n";
    
    // 继续读取下一条消息（循环）
    do_read();
}
