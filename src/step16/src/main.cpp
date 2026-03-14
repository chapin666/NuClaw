// src/main.cpp - Step 16: Agent 状态与记忆系统
#include "nuclaw/agent_state.hpp"
#include <iostream>
#include <string>

using namespace nuclaw;

// 带状态和记忆的 Agent
class StatefulAgent {
public:
    AgentState state;
    std::string current_user;
    
    StatefulAgent(const std::string& id) {
        state.agent_id = id;
    }
    
    std::string chat(const std::string& user_id, 
                     const std::string& user_name,
                     const std::string& message) {
        current_user = user_id;
        
        // 1. 记录用户输入到短期记忆
        state.add_memory("用户说: " + message, "conversation", 5.0f);
        
        // 2. 更新关系
        state.update_relationship(user_id, user_name, 0.5f);
        auto& rel = state.get_or_create_relationship(user_id);
        
        // 3. 生成回复（根据记忆和关系）
        std::string response = generate_response(message, rel);
        
        // 4. 记录回复到短期记忆
        state.add_memory("Agent回复: " + response, "conversation", 5.0f);
        
        // 5. 更新情感状态
        if (message.find("谢谢") != std::string::npos ||
            message.find("棒") != std::string::npos) {
            state.emotion.happiness += 1.0f;
        } else if (message.find("笨") != std::string::npos ||
                   message.find("差") != std::string::npos) {
            state.emotion.happiness -= 1.0f;
        }
        
        return response;
    }
    
    std::string generate_response(const std::string& message, 
                                   const Relationship& rel) {
        std::string response;
        
        // 根据熟悉度调整问候
        if (rel.interaction_count == 0) {
            response = "你好！我是有记忆的Agent，很高兴认识你！\n";
        } else if (rel.familiarity > 5) {
            response = "又见面了" + rel.user_name + "！";
            if (rel.affinity > 7) {
                response += "见到你真开心 😊\n";
            } else {
                response += "\n";
            }
        } else {
            response = "你好" + rel.user_name + "！\n";
        }
        
        // 处理具体意图
        if (message.find("我叫") != std::string::npos) {
            response += "我记住了你的名字！";
        }
        else if (message.find("我叫什么") != std::string::npos) {
            if (!rel.user_name.empty()) {
                response += "你叫" + rel.user_name + "呀！";
            } else {
                response += "你还没告诉我你的名字呢~";
            }
        }
        else if (message.find("记得") != std::string::npos) {
            response += "我记得我们聊过 " + std::to_string(rel.interaction_count) + " 次了！";
        }
        else if (message.find("状态") != std::string::npos) {
            response += "我现在心情" + state.emotion.describe() + "。\n";
            response += "对你的好感度: " + std::to_string(int(rel.affinity)) + "/10\n";
            response += "熟悉程度: " + std::to_string(int(rel.familiarity)) + "/10";
        }
        else {
            response += "我在听呢，请继续~";
        }
        
        return response;
    }
    
    void show_memory() {
        std::cout << "\n📚 短期记忆 (最近 " << state.short_term_memory.size() << " 条):\n";
        for (const auto& mem : state.short_term_memory) {
            std::cout << "  • " << mem.content << "\n";
        }
        
        std::cout << "\n👥 用户关系:\n";
        for (const auto& [id, rel] : state.relationships) {
            std::cout << "  • " << rel.user_name << ": "
                      << "熟悉度=" << int(rel.familiarity)
                      << ", 好感=" << int(rel.affinity)
                      << ", 互动" << rel.interaction_count << "次\n";
        }
        
        std::cout << "\n😊 当前情感: " << state.emotion.describe() << "\n";
        std::cout << "   开心度: " << int(state.emotion.happiness) << "/10\n";
        std::cout << "   精力: " << int(state.emotion.energy) << "/10\n";
    }
};

int main() {
    std::cout << "🤖 Step 16: Agent 状态与记忆系统\n";
    std::cout << "================================\n\n";
    
    StatefulAgent agent("agent_001");
    
    // 模拟多用户对话
    std::cout << "【场景】用户 A (user_a) 第一次对话:\n";
    std::cout << "用户A: 你好，我叫小明\n";
    std::cout << "Agent: " << agent.chat("user_a", "小明", "你好，我叫小明") << "\n\n";
    
    std::cout << "用户A: 记得我吗？\n";
    std::cout << "Agent: " << agent.chat("user_a", "小明", "记得我吗？") << "\n\n";
    
    std::cout << "【场景】用户 B (user_b) 第一次对话:\n";
    std::cout << "用户B: 你好，我叫小红\n";
    std::cout << "Agent: " << agent.chat("user_b", "小红", "你好，我叫小红") << "\n\n";
    
    std::cout << "【场景】用户 A 再次对话:\n";
    std::cout << "用户A: 我叫什么？\n";
    std::cout << "Agent: " << agent.chat("user_a", "小明", "我叫什么？") << "\n\n";
    
    std::cout << "用户A: 你真好！\n";
    std::cout << "Agent: " << agent.chat("user_a", "小明", "你真好！") << "\n\n";
    
    std::cout << "【场景】查看 Agent 状态:\n";
    agent.show_memory();
    
    std::cout << "\n\n================================\n";
    std::cout << "演示完成！Agent 记住了:\n";
    std::cout << "✓ 每个用户的名字\n";
    std::cout << "✓ 与每个用户的互动次数\n";
    std::cout << "✓ 对话历史（短期记忆）\n";
    std::cout << "✓ 情感状态变化\n";
    
    return 0;
}