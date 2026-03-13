// ============================================================================
// agent_loop.cpp - Agent 状态机实现
// ============================================================================

#include "agent_loop.hpp"
#include <sstream>

// ----------------------------------------------------------------------------
// Session 实现
// ----------------------------------------------------------------------------

void AgentLoop::Session::add_message(const std::string& role, 
                                     const std::string& content) {
    // 超出限制时，移除最旧的消息（FIFO）
    // 策略说明：
    // - 简单高效
    // - 缺点是可能丢失重要上下文
    // - Step 5 会学习更智能的压缩策略
    if (history.size() >= max_history) {
        history.erase(history.begin());
    }
    
    history.push_back({role, content, current_time()});
}

std::string AgentLoop::Session::current_time() const {
    auto now = std::time(nullptr);
    char buf[100];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
    return buf;
}

// ----------------------------------------------------------------------------
// AgentLoop 实现
// ----------------------------------------------------------------------------

std::string AgentLoop::process(const std::string& user_input,
                               const std::string& session_id) {
    // ---------------------------------------------------------------------
    // 步骤 1: 获取或创建会话
    // ---------------------------------------------------------------------
    // operator[] 的行为：
    // - 如果 key 存在，返回对应的 value
    // - 如果 key 不存在，创建一个默认的 Session 并返回
    // ---------------------------------------------------------------------
    auto& session = sessions_[session_id];
    session.id = session_id;
    
    // ---------------------------------------------------------------------
    // 步骤 2: 状态转换 IDLE -> THINKING
    // ---------------------------------------------------------------------
    // 状态机的好处：
    // - 明确知道当前在做什么
    // - 可以拒绝不合法的请求（如正在思考时的新输入）
    // - 便于调试和监控
    // ---------------------------------------------------------------------
    session.state = State::THINKING;
    
    // ---------------------------------------------------------------------
    // 步骤 3: 保存用户输入到历史
    // ---------------------------------------------------------------------
    // 为什么立即保存？
    // - 即使后续处理失败，用户输入也不会丢失
    // - 历史记录是"已发生的事实"
    // ---------------------------------------------------------------------
    session.add_message("user", user_input);
    
    // ---------------------------------------------------------------------
    // 步骤 4: 生成响应
    // ---------------------------------------------------------------------
    // 简化版：实际应该调用 LLM API
    // LLM 调用流程：
    // 1. 构建 prompt（系统提示 + 历史 + 当前输入）
    // 2. 调用 API（OpenAI/Claude/本地模型）
    // 3. 解析输出（可能是纯文本或工具调用）
    // 4. 如果需要工具，进入 TOOL_CALLING 状态
    // 5. 收集工具结果后，再次调用 LLM 生成最终回复
    // ---------------------------------------------------------------------
    std::string response = generate_response(user_input, session);
    
    // ---------------------------------------------------------------------
    // 步骤 5: 保存助手响应到历史
    // ---------------------------------------------------------------------
    session.add_message("assistant", response);
    
    // ---------------------------------------------------------------------
    // 步骤 6: 状态转换 THINKING -> IDLE
    // ---------------------------------------------------------------------
    session.state = State::IDLE;
    
    return response;
}

json::value AgentLoop::get_session_info(const std::string& session_id) const {
    // find() vs operator[]:
    // - find() 不会创建新元素，适合查询
    // - operator[] 会创建新元素，适合插入/更新
    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) {
        return json::object{{"error", "Session not found: " + session_id}};
    }
    
    const auto& session = it->second;
    
    // 构建 JSON 响应
    json::object info;
    info["session_id"] = session_id;
    info["state"] = state_to_string(session.state);
    info["message_count"] = static_cast<int>(session.history.size());
    info["max_history"] = static_cast<int>(session.max_history);
    
    // 添加历史记录
    json::array msgs;
    for (const auto& m : session.history) {
        json::object msg;
        msg["role"] = m.role;
        msg["content"] = m.content;
        msg["timestamp"] = m.timestamp;
        msgs.push_back(msg);
    }
    info["history"] = msgs;
    
    return info;
}

// ----------------------------------------------------------------------------
// 私有方法
// ----------------------------------------------------------------------------

std::string AgentLoop::generate_response(const std::string& input,
                                         const Session& session) const {
    // 简化版响应生成
    // 实际应该调用 LLM，传入完整的对话历史
    
    std::ostringstream oss;
    oss << "Echo: " << input
        << " [state: " << state_to_string(session.state)
        << ", history: " << session.history.size() << " msgs]";
    
    return oss.str();
}

std::string AgentLoop::state_to_string(State s) {
    switch (s) {
        case State::IDLE:         return "idle";
        case State::THINKING:     return "thinking";
        case State::TOOL_CALLING: return "tool_calling";
        case State::RESPONDING:   return "responding";
        default:                  return "unknown";
    }
}
