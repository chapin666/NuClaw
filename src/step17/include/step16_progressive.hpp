// ============================================================================
// step16_progressive.hpp - Step 16: 渐进式演进展示
// 
// 本文件展示如何在 Step 15 基础上添加状态记忆功能
// 逻辑渐进式，代码独立可编译
// ============================================================================

#pragma once
#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <iostream>

// ============================================
// Step 15 基线（已有代码）
// ============================================
namespace step15 {

// Step 15 的消息结构
struct Message {
    std::string id;
    std::string from;
    std::string to;
    std::string type;
    std::string content;
    
    static int counter;
    static std::string generate_id() {
        return "msg_" + std::to_string(++counter);
    }
};
int Message::counter = 0;

// Step 15 的 Agent 基类
class Agent {
public:
    Agent(const std::string& name, const std::string& role)
        : name_(name), role_(role) {}
    
    virtual ~Agent() = default;
    virtual void process_message(const Message& msg) = 0;
    
    std::string get_name() const { return name_; }
    std::string get_role() const { return role_; }
    
protected:
    std::string name_;
    std::string role_;
};

} // namespace step15

// ============================================
// Step 16 新增：状态与记忆系统
// ============================================
namespace step16 {

using namespace step15;

// 新增：情感状态
struct EmotionState {
    float happiness = 0.0f;
    float energy = 5.0f;
    float trust = 5.0f;
    
    std::string describe() const {
        if (happiness > 5) return "开心";
        if (happiness < -5) return "难过";
        if (energy < 3) return "疲惫";
        return "平静";
    }
    
    void update_from_interaction(const std::string& user_msg) {
        if (user_msg.find("谢谢") != std::string::npos ||
            user_msg.find("棒") != std::string::npos) {
            happiness += 1.0f;
            trust += 0.5f;
        } else if (user_msg.find("笨") != std::string::npos ||
                   user_msg.find("差") != std::string::npos) {
            happiness -= 1.0f;
            trust -= 0.5f;
        }
        happiness = std::max(-10.0f, std::min(10.0f, happiness));
        trust = std::max(0.0f, std::min(10.0f, trust));
    }
};

// 新增：记忆条目
struct Memory {
    std::string content;
    std::string category;
    float importance = 5.0f;
    std::string user_id;
    std::chrono::system_clock::time_point timestamp;
};

// 新增：用户关系
struct Relationship {
    std::string user_id;
    std::string user_name;
    float familiarity = 0.0f;
    float affinity = 5.0f;
    int interaction_count = 0;
};

// 新增：Agent 状态
class AgentState {
public:
    std::string agent_id;
    std::string current_activity = "idle";
    EmotionState emotion;
    std::vector<Memory> short_term_memory;
    std::map<std::string, Relationship> relationships;
    
    void add_memory(const std::string& content, 
                    const std::string& category,
                    const std::string& user_id = "") {
        Memory mem{content, category, 5.0f, user_id, 
                   std::chrono::system_clock::now()};
        short_term_memory.push_back(mem);
        if (short_term_memory.size() > 10) {
            short_term_memory.erase(short_term_memory.begin());
        }
    }
    
    Relationship& get_or_create_relationship(const std::string& user_id) {
        if (relationships.count(user_id) == 0) {
            relationships[user_id] = Relationship();
            relationships[user_id].user_id = user_id;
        }
        return relationships[user_id];
    }
    
    void update_relationship(const std::string& user_id, 
                             const std::string& user_name,
                             float affinity_delta) {
        auto& rel = get_or_create_relationship(user_id);
        rel.user_name = user_name;
        rel.affinity += affinity_delta;
        rel.interaction_count++;
        rel.familiarity = std::min(10.0f, rel.familiarity + 0.5f);
    }
};

// ============================================
// Step 16 核心：继承并扩展 Step 15 的 Agent
// ============================================

// StatefulAgent 继承自 Step 15 的 Agent，添加记忆功能
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
        auto& rel = state_.get_or_create_relationship(user_id);
        if (rel.user_name.empty()) rel.user_name = user_name;
        
        // 生成回复（基于关系）
        std::string response = generate_response(message, user_name, rel);
        
        // 记录回复
        state_.add_memory("Agent回复: " + response, "conversation", user_id);
        
        // 更新情感和关系
        state_.emotion.update_from_interaction(message);
        state_.update_relationship(user_id, user_name, 0.5f);
        
        return response;
    }
    
    // 获取状态（公共接口）
    AgentState& get_state() { return state_; }
    const AgentState& get_state() const { return state_; }
    
    // 显示当前状态
    void show_state() const {
        std::cout << "\n📊 Agent 状态:\n";
        std::cout << "  情感: " << state_.emotion.describe() << "\n";
        std::cout << "  记忆条数: " << state_.short_term_memory.size() << "\n";
        std::cout << "  用户关系: " << state_.relationships.size() << "人\n";
        
        for (const auto& [id, rel] : state_.relationships) {
            std::cout << "    - " << rel.user_name 
                      << " (熟悉" << int(rel.familiarity) 
                      << ", 好感" << int(rel.affinity) << ")\n";
        }
    }
    
    // 实现基类纯虚函数（多 Agent 协作场景）
    void process_message(const Message& msg) override {
        if (msg.type == "USER_MESSAGE") {
            // 简化处理：解析用户消息
            std::string response = process_user_message(
                "user_" + msg.from, msg.from, msg.content
            );
            
            std::cout << "[StatefulAgent " << name_ << "] 回复: " 
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

} // namespace step16