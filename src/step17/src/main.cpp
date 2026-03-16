// ============================================================================
// main.cpp - Step 16: Agent 状态与记忆系统（完整文件版）
// ============================================================================
//
// Step 16 完整实现：在 Step 15 基础上添加状态记忆功能
// 文件结构：
//   include/nuclaw/
//     - emotion.hpp       情感状态
//     - memory.hpp        记忆系统
//     - relationship.hpp  用户关系
//     - agent_state.hpp   完整 Agent 状态
//     - stateful_agent.hpp 有状态的 Agent（继承 step15::Agent）
// ============================================================================

#include "nuclaw/stateful_agent.hpp"
#include <iostream>

using namespace nuclaw;

int main() {
    std::cout << "========================================\n";
    std::cout << "Step 16: Agent 状态与记忆系统\n";
    std::cout << "========================================\n";
    std::cout << "演进：Step 15 Agent 基类 → StatefulAgent\n\n";
    
    // 创建有状态的 Agent（继承自 Step 15 的 Agent）
    StatefulAgent agent("memory_bot", "记忆型助手");
    
    std::cout << "【演示 1】多用户对话，Agent 记住每个人\n";
    std::cout << "----------------------------------------\n\n";
    
    // 用户 A 首次对话
    std::cout << "用户 A (小明): 你好，我叫小明\n";
    std::cout << "Agent: " << agent.process_user_message("user_a", "小明", 
        "你好，我叫小明") << "\n\n";
    
    // 用户 B 首次对话
    std::cout << "用户 B (小红): 你好，我叫小红\n";
    std::cout << "Agent: " << agent.process_user_message("user_b", "小红",
        "你好，我叫小红") << "\n\n";
    
    // 用户 A 再次对话 - Agent 记得他
    std::cout << "用户 A (小明): 记得我吗？\n";
    std::cout << "Agent: " << agent.process_user_message("user_a", "小明",
        "记得我吗？") << "\n\n";
    
    // 用户 A 询问名字
    std::cout << "用户 A (小明): 我叫什么？\n";
    std::cout << "Agent: " << agent.process_user_message("user_a", "小明",
        "我叫什么？") << "\n\n";
    
    // 积极互动，提升好感
    std::cout << "用户 A (小明): 你真棒！\n";
    std::cout << "Agent: " << agent.process_user_message("user_a", "小明",
        "你真棒！") << "\n";
    
    // 显示状态
    std::cout << "\n【Agent 内部状态】\n";
    agent.show_state();
    
    std::cout << "\n========================================\n";
    std::cout << "演进成果:\n";
    std::cout << "  ✓ 继承 Step 15 Agent 基类\n";
    std::cout << "  ✓ 添加 AgentState 状态管理\n";
    std::cout << "  ✓ 短期记忆：保留最近10条\n";
    std::cout << "  ✓ 用户关系：区分不同用户\n";
    std::cout << "  ✓ 情感计算：根据交互动态变化\n";
    std::cout << "\n下一步 → Step 17: 基于 StatefulAgent 构建旅行助手\n";
    
    return 0;
}