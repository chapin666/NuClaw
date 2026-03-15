#pragma once

#include <string>
#include <chrono>
#include <nlohmann/json.hpp>

namespace smartsupport {

using json = nlohmann::json;

// 消息结构
struct ChatMessage {
    std::string id;
    std::string session_id;
    std::string sender_type;  // user/ai/human
    std::string sender_id;
    std::string content;
    std::string content_type; // text/image/card
    json metadata;
    std::chrono::system_clock::time_point created_at;
};

// 会话信息
struct SessionInfo {
    std::string id;
    std::string tenant_id;
    std::string user_id;
    std::string channel;  // web/wechat/feishu
    std::string status;   // active/closed/waiting_human
    json metadata;
    std::chrono::system_clock::time_point created_at;
};

// AI 响应
struct AIResponse {
    std::string content;
    bool needs_escalation = false;
    float confidence = 0.0f;
    std::vector<std::string> knowledge_sources;
    json tool_calls;
};

} // namespace smartsupport
