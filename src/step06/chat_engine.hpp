// ============================================================================
// chat_engine.hpp/cpp - Agent 核心（ChatEngine）
// ============================================================================

#pragma once

#include "llm_client.hpp"
#include "tool_executor.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <utility>

// 对话上下文
struct ChatContext {
    std::vector<std::pair<std::string, std::string>> history;
    int message_count = 0;
};

// ChatEngine - Agent Loop 核心
// 完整的流程：输入 → LLM理解 → 决策 → 工具调用 → 生成回复
class ChatEngine {
public:
    ChatEngine() = default;
    
    // 处理用户输入（核心 Agent Loop）
    std::string process(const std::string& user_input, ChatContext& ctx) {
        ctx.message_count++;
        std::cout << "[🧠 Processing] \"" << user_input << "\"" << std::endl;
        
        // Step 1: LLM 判断是否需要工具
        if (llm_.needs_tool(user_input)) {
            std::cout << "  → Decision: Need tool" << std::endl;
            
            // Step 2: 解析工具调用
            ToolCall call = llm_.parse_tool_call(user_input);
            std::cout << "  → Tool: " << call.name << "(" <> call.arguments << ")" << std::endl;
            
            // Step 3: 执行工具
            ToolResult result = ToolExecutor::execute(call);
            std::cout << "  → Result: " << (result.success ? "OK" : "FAIL") << std::endl;
            
            // Step 4: LLM 根据结果生成回复
            std::string reply = llm_.generate_response(user_input, result, call);
            
            // 保存历史
            ctx.history.push_back({"user", user_input});
            ctx.history.push_back({"assistant", reply});
            
            return reply;
        }
        else {
            std::cout << "  → Decision: Direct reply" << std::endl;
            
            // 不需要工具，直接回答
            std::string reply = llm_.direct_reply(user_input);
            
            // 保存历史
            ctx.history.push_back({"user", user_input});
            ctx.history.push_back({"assistant", reply});
            
            return reply;
        }
    }
    
    std::string get_welcome_message() const {
        return "👋 欢迎来到 NuClaw Step 6！\n\n"
               "🔧 现在支持工具调用了！\n\n"
               "试试：\n"
               "- 北京天气如何？\n"
               "- 现在几点？\n"
               "- 25 * 4 等于多少？\n"
               "- 你能做什么？\n\n"
               "⚠️ 注意：工具是硬编码的\n"
               "（下一章解决扩展性问题）";
    }

private:
    LLMClient llm_;
};
