// ============================================================================
// chat_engine.hpp - Agent 核心（Step 9 注册表版）
// ============================================================================

#pragma once

#include "llm_client.hpp"
#include "tool_executor.hpp"
#include "tool_registry.hpp"
#include <vector>
#include <utility>

struct ChatContext {
    std::vector<std::pair<std::string, std::string>> history;
    int message_count = 0;
};

class ChatEngine {
public:
    ChatEngine() = default;
    
    // 处理用户输入
    std::string process(const std::string& user_input, ChatContext& ctx) {
        ctx.message_count++;
        std::cout << "[🧠 处理] \"" << user_input << "\"" << std::endl;
        
        // 判断是否需要工具
        if (llm_.needs_tool(user_input)) {
            std::cout << "  → 需要工具" << std::endl;
            
            ToolCall call = llm_.parse_tool_call(user_input);
            std::cout << "  → 工具: " << call.name << std::endl;
            
            // 通过注册表执行工具
            ToolResult result = ToolExecutor::execute_sync(call);
            std::cout << "  → 结果: " << (result.success ? "成功" : "失败") << std::endl;
            
            std::string reply = llm_.generate_response(user_input, result, call);
            
            ctx.history.push_back({"user", user_input});
            ctx.history.push_back({"assistant", reply});
            
            return reply;
        }
        else {
            std::cout << "  → 直接回复" << std::endl;
            
            std::string reply = llm_.direct_reply(user_input);
            
            ctx.history.push_back({"user", user_input});
            ctx.history.push_back({"assistant", reply});
            
            return reply;
        }
    }
    
    std::string get_welcome_message() const {
        return "👋 欢迎来到 NuClaw Step 9！\n\n"
               "📝 本章核心：工具注册表模式\n"
               "- 使用注册表管理工具，不再硬编码\n"
               "- 支持动态注册和扩展\n"
               "- 符合开闭原则\n\n"
               "可用命令：\n"
               "- 天气查询：北京天气如何？\n"
               "- 时间查询：现在几点？\n"
               "- 数学计算：25 * 4 等于多少？\n"
               "- 查看工具：有哪些工具？";
    }

private:
    LLMClient llm_;
};
