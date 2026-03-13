// ============================================================================
// chat_engine.hpp - Agent 核心（Step 7 演进）
// ============================================================================
// 演进说明：
//   基于 Step 6，支持异步工具执行
//   Step 6: process() 是同步的，阻塞等待工具完成
//   Step 7: process_async() 是异步的，立即返回，结果通过回调通知
// ============================================================================

#pragma once

#include "llm_client.hpp"
#include "tool_executor.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <utility>
#include <future>

// 对话上下文
struct ChatContext {
    std::vector<std::pair<std::string, std::string>> history;
    int message_count = 0;
};

// ChatEngine - Agent Loop 核心（支持异步）
class ChatEngine {
public:
    ChatEngine() = default;
    
    // Step 6 的同步接口（保留兼容）
    std::string process(const std::string& user_input, ChatContext& ctx) {
        ctx.message_count++;
        std::cout << "[🧠 Processing] \"" << user_input << "\"" << std::endl;
        
        if (llm_.needs_tool(user_input)) {
            std::cout << "  → Decision: Need tool" << std::endl;
            
            ToolCall call = llm_.parse_tool_call(user_input);
            std::cout << "  → Tool: " << call.name << std::endl;
            
            // 同步执行（Step 6 方式）
            ToolResult result = ToolExecutor::execute_sync(call);
            std::cout << "  → Result: " << (result.success ? "OK" : "FAIL") << std::endl;
            
            std::string reply = llm_.generate_response(user_input, result, call);
            ctx.history.push_back({"user", user_input});
            ctx.history.push_back({"assistant", reply});
            return reply;
        }
        else {
            std::string reply = llm_.direct_reply(user_input);
            ctx.history.push_back({"user", user_input});
            ctx.history.push_back({"assistant", reply});
            return reply;
        }
    }
    
    // Step 7 新增：异步处理
    // 立即返回，结果通过回调通知
    void process_async(const std::string& user_input, 
                       ChatContext& ctx,
                       std::function<void(std::string)> callback) {
        ctx.message_count++;
        std::cout << "[🧠 Processing async] \"" << user_input << "\"" << std::endl;
        
        if (llm_.needs_tool(user_input)) {
            ToolCall call = llm_.parse_tool_call(user_input);
            std::cout << "  → Will call tool: " << call.name << std::endl;
            
            // 异步执行工具
            executor_.execute_async(call, 
                [this, user_input, call, callback, &ctx](ToolResult result) {
                    std::cout << "  → Tool completed: " << (result.success ? "OK" : "FAIL") << std::endl;
                    
                    std::string reply = llm_.generate_response(user_input, result, call);
                    ctx.history.push_back({"user", user_input});
                    ctx.history.push_back({"assistant", reply});
                    callback(reply);
                },
                std::chrono::seconds(3)  // 3秒超时
            );
        }
        else {
            // 不需要工具，直接回复
            std::string reply = llm_.direct_reply(user_input);
            ctx.history.push_back({"user", user_input});
            ctx.history.push_back({"assistant", reply});
            callback(reply);
        }
    }
    
    // 获取执行器状态
    void print_executor_status() const {
        std::cout << "[📊 Executor] Running: " << executor_.get_running_count() 
                  << ", Queued: " << executor_.get_queue_length() << std::endl;
    }
    
    std::string get_welcome_message() const {
        return "👋 欢迎来到 NuClaw Step 7！\n\n"
               "⚡ 现在支持异步工具执行了！\n\n"
               "改进：\n"
               "- 工具异步执行，不阻塞\n"
               "- 并发控制（最多3个同时执行）\n"
               "- 超时保护（3秒）\n\n"
               "试试同时问多个问题！";
    }

private:
    LLMClient llm_;
    ToolExecutor executor_{3};  // 最多3个并发
};
