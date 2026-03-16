#include "smartsupport/services/chat/service.hpp"
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

namespace smartsupport::chat {

ChatService::ChatService(asio::io_context& io,
                         std::shared_ptr<services::ai::AIService> ai_service,
                         std::shared_ptr<services::knowledge::KnowledgeService> kb_service)
    : io_(io), ai_service_(ai_service), kb_service_(kb_service) {}

void ChatService::on_connect(const std::string& conn_id,
                             const std::string& tenant_id,
                             const std::string& user_id) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    
    auto session = get_or_create_session(tenant_id, user_id);
    conn_to_session_[conn_id] = session->get_id();
    
    json welcome_msg = {
        {"type", "welcome"},
        {"session_id", session->get_id()},
        {"message", "Welcome to SmartSupport!"}
    };
    send_to_client(conn_id, welcome_msg);
}

void ChatService::on_disconnect(const std::string& conn_id) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    conn_to_session_.erase(conn_id);
    connections_.erase(conn_id);
}

void ChatService::on_message(const std::string& conn_id, const std::string& content) {
    std::shared_ptr<Session> session;
    std::string tenant_id;
    
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        auto it = conn_to_session_.find(conn_id);
        if (it == conn_to_session_.end()) return;
        
        auto sit = sessions_.find(it->second);
        if (sit == sessions_.end()) return;
        
        session = sit->second;
        tenant_id = session->get_tenant_id();
    }
    
    session->add_user_message(content);
    
    auto route = decide_route(*session, content);
    
    if (route.target == RouteDecision::Target::AI) {
        auto history = session->get_recent_history(10);
        
        ai_service_>process(tenant_id, session->get_id(), history, content,
            [this, conn_id, session](const AIResponse& response) {
                handle_ai_response(session->get_id(), conn_id, response);
            });
    } else {
        json msg = {
            {"type", "escalation"},
            {"message", "Transferring to human agent..."}
        };
        send_to_client(conn_id, msg);
    }
}

void ChatService::send_to_client(const std::string& conn_id, const json& message) {
    // WebSocket 发送实现（简化版）
    auto it = connections_.find(conn_id);
    if (it != connections_.end()) {
        std::string payload = message.dump();
        // async_write(*it->second.socket, asio::buffer(payload), ...);
    }
}

std::shared_ptr<Session> ChatService::get_or_create_session(
    const std::string& tenant_id, const std::string& user_id) {
    
    // 查找现有会话
    for (const auto& [id, session] : sessions_) {
        if (session->get_tenant_id() == tenant_id && 
            session->get_state() != SessionState::CLOSED) {
            return session;
        }
    }
    
    // 创建新会话
    boost::uuids::uuid uuid = boost::uuids::random_generator()();
    std::string id = boost::uuids::to_string(uuid);
    
    auto session = std::make_shared<Session>(id, tenant_id, user_id, "web");
    sessions_[id] = session;
    return session;
}

RouteDecision ChatService::decide_route(const Session& session, const std::string& message) {
    // 检查是否请求人工
    if (message.find("人工") != std::string::npos ||
        message.find("客服") != std::string::npos ||
        message.find("转人工") != std::string::npos) {
        return {RouteDecision::Target::HUMAN, "user_requested"};
    }
    
    // 检查是否已在人工服务中
    if (session.get_state() == SessionState::WITH_HUMAN) {
        return {RouteDecision::Target::HUMAN, "already_with_human"};
    }
    
    return {RouteDecision::Target::AI, "default"};
}

void ChatService::handle_ai_response(const std::string& session_id,
                                     const std::string& conn_id,
                                     const AIResponse& response) {
    auto it = sessions_.find(session_id);
    if (it != sessions_.end()) {
        it->second->add_ai_message(response.content, response.knowledge_sources);
    }
    
    json msg = {
        {"type", "ai_response"},
        {"content", response.content},
        {"confidence", response.confidence}
    };
    
    if (!response.knowledge_sources.empty()) {
        msg["sources"] = response.knowledge_sources;
    }
    
    send_to_client(conn_id, msg);
    
    if (response.needs_escalation) {
        json escalation_msg = {
            {"type", "escalation"},
            {"message", "AI confidence is low, would you like to speak with a human agent?"}
        };
        send_to_client(conn_id, escalation_msg);
    }
}

} // namespace smartsupport::chat
