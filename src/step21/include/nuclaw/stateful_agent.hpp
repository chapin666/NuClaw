// ============================================================================
// stateful_agent.hpp - 有状态的 Agent（继承 Step 15）
// ============================================================================

#pragma once
#include "nuclaw/agent.hpp"      // 来自 step15（全局命名空间）
#include "agent_state.hpp"
#include <iostream>

namespace nuclaw {

// StatefulAgent 继承自 step15 的 Agent（全局命名空间），添加记忆功能
class StatefulAgent : public Agent {
public:
    StatefulAgent(const std::string& name, const std::string& role)
        : Agent(name, role) {
        state_.agent_id = name;
    }
    
    // 处理用户消息（新增记忆功能）
    std::string process_user_message(const std::string& user_id,
                                      const std::string& user_name,
                                      const std::string& message) {
        // 记录到记忆
        state_.add_memory("用户说: " + message, "conversation", user_id);
        
        // 获取用户关系
        auto& rel = state_.get_relationship(user_id);
        if (rel.user_name.empty()) rel.user_name = user_name;
        
        // 生成回复（基于关系）
        std::string response = generate_response(message, user_name, rel);
        
        // 记录回复
        state_.add_memory("Agent回复: " + response, "conversation", user_id);
        
        // 更新情感和关系
        state_.update_emotion(message);
        state_.update_relationship(user_id, user_name, 0.5f);
        
        return response;
    }
    
    // 获取状态
    AgentState& get_state() { return state_; }
    const AgentState& get_state() const { return state_; }
    
    // 显示当前状态
    void show_state() const {
        std::cout << "\n📊 Agent 状态:\n";
        std::cout << "  情感: " << state_.emotion.describe() << "\n";
        std::cout << "  记忆条数: " << state_.short_term_memory.size() << "\n";
        std::cout << "  用户关系: " << state_.relationships.count() << "人\n";
        
        for (const auto& [id, rel] : state_.relationships.get_all()) {
            std::cout << "    - " << rel.user_name 
                      << " (熟悉" << int(rel.familiarity) 
                      << ", 好感" << int(rel.affinity) << ")\n";
        }
    }
    
    // 实现基类纯虚函数（多 Agent 协作场景）
    void process_message(const Message& msg) override {
        if (msg.type == "USER_MESSAGE") {
            std::string response = process_user_message(
                "user_" + msg.from, msg.from, msg.content
            );
            
            std::cout << "[StatefulAgent " << get_name() << "] 回复: " 
                      << response << "\n";
        }
    }

protected:
    AgentState state_;
    
    std::string generate_response(const std::string& message,
                                   const std::string& user_name,
                                   const Relationship& rel) {
        std::string response;
        
        // 根据熟悉度调整
        if (rel.interaction_count == 0) {
            response = "你好" + user_name + "！我是有记忆的Agent。\n";
        } else if (rel.familiarity > 5) {
            response = "又见面了" + user_name + "！";
            if (rel.affinity > 7) response += "😊\n";
            else response += "\n";
        } else {
            response = "你好" + user_name + "！\n";
        }
        
        // 意图处理
        if (message.find("我叫什么") != std::string::npos) {
            response += "你叫" + user_name + "呀！";
        }
        else if (message.find("记得") != std::string::npos) {
            response += "我们聊过 " + std::to_string(rel.interaction_count) + " 次了！";
        }
        else if (message.find("状态") != std::string::npos) {
            response += "我现在心情" + state_.emotion.describe() + "。";
        }
        else {
            response += "我在听，请继续~";
        }
        
        return response;
    }
};

} // namespace nuclaw
