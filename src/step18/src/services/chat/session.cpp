#include "smartsupport/services/chat/session.hpp"

namespace smartsupport::chat {

Session::Session(const std::string& id,
                 const std::string& tenant_id,
                 const std::string& user_id,
                 const std::string& channel)
    : id_(id), tenant_id_(tenant_id), user_id_(user_id), channel_(channel),
      state_(SessionState::ACTIVE), last_activity_(std::chrono::steady_clock::now()) {}

void Session::add_user_message(const std::string& content) {
    ChatMessage msg;
    msg.id = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    msg.session_id = id_;
    msg.sender_type = "user";
    msg.sender_id = user_id_;
    msg.content = content;
    msg.content_type = "text";
    msg.created_at = std::chrono::system_clock::now();
    messages_.push_back(msg);
    last_activity_ = std::chrono::steady_clock::now();
}

void Session::add_ai_message(const std::string& content,
                             const std::vector<std::string>& sources) {
    ChatMessage msg;
    msg.id = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    msg.session_id = id_;
    msg.sender_type = "ai";
    msg.content = content;
    msg.content_type = "text";
    msg.metadata["sources"] = sources;
    msg.created_at = std::chrono::system_clock::now();
    messages_.push_back(msg);
    last_activity_ = std::chrono::steady_clock::now();
}

std::vector<ChatMessage> Session::get_recent_history(size_t limit) const {
    std::vector<ChatMessage> recent;
    size_t start = messages_.size() > limit ? messages_.size() - limit : 0;
    for (size_t i = start; i < messages_.size(); ++i) {
        recent.push_back(messages_[i]);
    }
    return recent;
}

void Session::transition_to(SessionState new_state) {
    state_ = new_state;
}

SessionState Session::get_state() const {
    return state_;
}

} // namespace smartsupport::chat
