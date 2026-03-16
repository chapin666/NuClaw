#pragma once

#include "smartsupport/services/chat/session.hpp"
#include <boost/asio.hpp>
#include <memory>
#include <map>
#include <mutex>

namespace smartsupport::services::ai {
    class AIService;
}

namespace smartsupport::services::knowledge {
    class KnowledgeService;
}

namespace smartsupport::chat {

namespace asio = boost::asio;
using json = nlohmann::json;

// WebSocket 连接（简化版）
struct WebSocketConnection {
    std::string id;
    std::shared_ptr<asio::ip::tcp::socket> socket;
    std::string tenant_id;
    std::string user_id;
};

// Chat 服务
class ChatService {
public:
    ChatService(asio::io_context& io,
                std::shared_ptr<services::ai::AIService> ai_service,
                std::shared_ptr<services::knowledge::KnowledgeService> kb_service);
    
    // WebSocket 连接管理
    void on_connect(const std::string& conn_id,
                    const std::string& tenant_id,
                    const std::string& user_id);
    void on_disconnect(const std::string& conn_id);
    void on_message(const std::string& conn_id, const std::string& content);
    
    // 发送消息给客户端
    void send_to_client(const std::string& conn_id, const json& message);
    void broadcast_to_session(const std::string& session_id, const json& message);

private:
    std::shared_ptr<Session> get_or_create_session(
        const std::string& tenant_id, const std::string& user_id);
    RouteDecision decide_route(const Session& session, const std::string& message);
    void handle_ai_response(const std::string& session_id,
                            const std::string& conn_id,
                            const AIResponse& response);

    asio::io_context& io_;
    std::shared_ptr<services::ai::AIService> ai_service_;
    std::shared_ptr<services::knowledge::KnowledgeService> kb_service_;
    
    std::map<std::string, std::shared_ptr<Session>> sessions_;
    std::map<std::string, std::string> conn_to_session_;
    std::map<std::string, WebSocketConnection> connections_;
    std::mutex sessions_mutex_;
};

} // namespace smartsupport::chat
