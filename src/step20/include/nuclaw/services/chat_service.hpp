// ============================================================================
// chat_service.hpp - Step 18: WebSocket 会话管理服务
// ============================================================================
// 演进说明：
//   从 Step 17 CustomerServiceAgent 提取会话管理功能
//   
//   Step 17: CustomerServiceAgent 直接处理消息
//   Step 18: ChatService 管理连接，委托 AIService 处理智能回复
//
//   职责分离：
//     - ChatService: 连接管理、会话生命周期、消息路由
//     - AIService: LLM 调用、RAG 检索、智能回复生成
// ============================================================================

#pragma once
#include "nuclaw/customer_service_agent.hpp"
#include "nuclaw/services/ai_service.hpp"
#include <boost/asio.hpp>
#include <memory>
#include <map>
#include <mutex>

namespace nuclaw {

// 前置声明
class AIService;

// WebSocket 连接（简化版，为 Step 18 演示）
struct WebSocketConnection {
    std::string connection_id;
    std::string tenant_id;
    std::string user_id;
    std::string user_name;
    std::chrono::steady_clock::time_point connected_at;
    
    WebSocketConnection(const std::string& conn_id, 
                        const std::string& t_id,
                        const std::string& u_id,
                        const std::string& u_name)
        : connection_id(conn_id), tenant_id(t_id), 
          user_id(u_id), user_name(u_name),
          connected_at(std::chrono::steady_clock::now()) {}
};

// 会话状态
struct ChatSession {
    std::string session_id;
    std::string tenant_id;
    std::string user_id;
    std::vector<std::pair<std::string, std::string>> messages;  // role, content
    std::chrono::steady_clock::time_point last_activity;
    
    ChatSession(const std::string& sid, const std::string& tid, const std::string& uid)
        : session_id(sid), tenant_id(tid), user_id(uid),
          last_activity(std::chrono::steady_clock::now()) {}
    
    void add_message(const std::string& role, const std::string& content) {
        messages.push_back({role, content});
        last_activity = std::chrono::steady_clock::now();
    }
};

// ChatService: 管理 WebSocket 连接和会话（Step 18 新增）
class ChatService {
public:
    ChatService(boost::asio::io_context& io, 
                std::shared_ptr<AIService> ai_service)
        : io_(io), ai_service_(ai_service), session_counter_(0) {}
    
    // 客户连接（模拟 WebSocket 连接建立）
    std::string connect_customer(const std::string& tenant_id,
                                  const std::string& user_id,
                                  const std::string& user_name) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::string conn_id = "conn_" + std::to_string(++session_counter_);
        connections_[conn_id] = std::make_unique<WebSocketConnection>(
            conn_id, tenant_id, user_id, user_name);
        
        // 获取或创建会话
        std::string session_key = tenant_id + ":" + user_id;
        if (sessions_.find(session_key) == sessions_.end()) {
            std::string session_id = "session_" + std::to_string(session_counter_);
            sessions_[session_key] = std::make_unique<ChatSession>(
                session_id, tenant_id, user_id);
        }
        
        std::cout << "[ChatService] 客户连接: " << conn_id 
                  << " (租户: " << tenant_id << ", 用户: " << user_name << ")\n";
        return conn_id;
    }
    
    // 处理客户消息（核心：委托给 AIService）
    std::string handle_message(const std::string& connection_id,
                                const std::string& message) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto conn_it = connections_.find(connection_id);
        if (conn_it == connections_.end()) {
            return "[错误] 连接不存在";
        }
        
        auto& conn = conn_it->second;
        std::string session_key = conn->tenant_id + ":" + conn->user_id;
        
        // 记录用户消息
        auto session_it = sessions_.find(session_key);
        if (session_it != sessions_.end()) {
            session_it->second->add_message("user", message);
        }
        
        // 委托给 AIService 生成回复（Step 18 核心：职责分离）
        // 注意：这里简化处理，实际应该异步调用
        std::string response = "[模拟回复] 收到消息: " + message;
        
        // 记录回复
        if (session_it != sessions_.end()) {
            session_it->second->add_message("assistant", response);
        }
        
        return response;
    }
    
    // 断开连接
    void disconnect(const std::string& connection_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = connections_.find(connection_id);
        if (it != connections_.end()) {
            std::cout << "[ChatService] 客户断开: " << connection_id << "\n";
            connections_.erase(it);
        }
    }
    
    // 获取会话历史
    std::vector<std::pair<std::string, std::string>> get_session_history(
        const std::string& tenant_id, 
        const std::string& user_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string session_key = tenant_id + ":" + user_id;
        auto it = sessions_.find(session_key);
        if (it != sessions_.end()) {
            return it->second->messages;
        }
        return {};
    }
    
    // 统计信息
    void print_stats() const {
        std::cout << "\n[ChatService 统计]\n";
        std::cout << "  活跃连接: " << connections_.size() << "\n";
        std::cout << "  总会话: " << sessions_.size() << "\n";
    }

private:
    boost::asio::io_context& io_;
    std::shared_ptr<AIService> ai_service_;
    
    std::map<std::string, std::unique_ptr<WebSocketConnection>> connections_;
    std::map<std::string, std::unique_ptr<ChatSession>> sessions_;
    mutable std::mutex mutex_;
    int session_counter_;
};

} // namespace nuclaw
