#pragma once

#include "smartsupport/common/types.hpp"
#include <boost/asio.hpp>
#include <memory>
#include <map>

namespace smartsupport::chat {

namespace asio = boost::asio;

// 会话状态
enum class SessionState {
    ACTIVE,
    WAITING_AI,
    WITH_HUMAN,
    CLOSED
};

// 会话类
class Session : public std::enable_shared_from_this<Session> {
public:
    Session(const std::string& id,
            const std::string& tenant_id,
            const std::string& user_id,
            const std::string& channel);
    
    void add_user_message(const std::string& content);
    void add_ai_message(const std::string& content,
                        const std::vector<std::string>& sources);
    std::vector<ChatMessage> get_recent_history(size_t limit = 10) const;
    void transition_to(SessionState new_state);
    SessionState get_state() const;
    
    std::string get_id() const { return id_; }
    std::string get_tenant_id() const { return tenant_id_; }

private:
    std::string id_;
    std::string tenant_id_;
    std::string user_id_;
    std::string channel_;
    SessionState state_ = SessionState::ACTIVE;
    std::vector<ChatMessage> messages_;
    std::chrono::steady_clock::time_point last_activity_;
};

// 路由决策
struct RouteDecision {
    enum class Target { AI, HUMAN };
    Target target;
    std::string reason;
};

} // namespace smartsupport::chat
