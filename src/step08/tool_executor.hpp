// ============================================================================
// tool_executor.hpp - 带安全的异步工具执行器（Step 8 演进）
// ============================================================================
// 演进说明：
//   基于 Step 7，添加：
//   1. 新工具：http_get, file, code_execute
//   2. 安全检查：SSRF、路径遍历、代码注入
// ============================================================================

#pragma once

#include "tool.hpp"
#include "sandbox.hpp"
#include "weather_tool.hpp"
#include "time_tool.hpp"
#include "calc_tool.hpp"
#include "http_tool.hpp"
#include "file_tool.hpp"
#include "code_tool.hpp"
#include <iostream>
#include <future>
#include <thread>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>

// 异步执行结果回调
template<typename T>
using AsyncCallback = std::function<void(T)>;

// Step 8 演进：带安全的异步工具执行器
class ToolExecutor {
public:
    ToolExecutor(size_t max_concurrent = 3) 
        : max_concurrent_(max_concurrent), running_count_(0) {}
    
    // 同步执行（Step 6/7 兼容）
    static ToolResult execute_sync(const ToolCall& call) {
        return execute_tool_with_safety(call);
    }
    
    // 异步执行（Step 7 演进）
    void execute_async(const ToolCall& call, 
                       AsyncCallback<ToolResult> callback,
                       std::chrono::milliseconds timeout_ms = std::chrono::seconds(5)) {
        
        // 并发控制
        {
            std::unique_lock<std::mutex> lock(mutex_);
            if (running_count_ >= max_concurrent_) {
                std::cout << "  [⏳ Queued] " << call.name << std::endl;
                pending_queue_.push({call, callback, timeout_ms});
                return;
            }
            running_count_++;
        }
        
        std::cout << "  [⚙️ Running] " << call.name << std::endl;
        
        // 异步执行
        std::thread([this, call, callback, timeout_ms]() {
            auto result = execute_with_timeout(call, timeout_ms);
            callback(result);
            
            // 处理队列
            {
                std::unique_lock<std::mutex> lock(mutex_);
                running_count_--;
                
                if (!pending_queue_.empty()) {
                    auto next = pending_queue_.front();
                    pending_queue_.pop();
                    lock.unlock();
                    execute_async(next.call, next.callback, next.timeout);
                }
            }
        }).detach();
    }
    
    // 带超时的执行
    static ToolResult execute_with_timeout(const ToolCall& call,
                                            std::chrono::milliseconds timeout_ms) {
        auto future = std::async(std::launch::async, [call]() {
            return execute_tool_with_safety(call);
        });
        
        if (future.wait_for(timeout_ms) == std::future_status::timeout) {
            return ToolResult::fail("工具执行超时 (" + std::to_string(timeout_ms.count()) + "ms)");
        }
        
        return future.get();
    }
    
    // 获取状态
    size_t get_queue_length() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return pending_queue_.size();
    }
    
    size_t get_running_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return running_count_;
    }

private:
    struct PendingTask {
        ToolCall call;
        AsyncCallback<ToolResult> callback;
        std::chrono::milliseconds timeout;
    };
    
    // Step 8：带安全检查的工具执行
    static ToolResult execute_tool_with_safety(const ToolCall& call) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        // Step 6 原有工具
        if (call.name == "get_weather" || call.name == "天气") {
            return WeatherTool::execute(call.arguments);
        }
        else if (call.name == "get_time" || call.name == "时间") {
            return TimeTool::execute(call.arguments);
        }
        else if (call.name == "calculate" || call.name == "计算") {
            return CalcTool::execute(call.arguments);
        }
        // Step 8 新增工具（带安全）
        else if (call.name == "http_get" || call.name == "http") {
            return HttpTool::execute(call.arguments);
        }
        else if (call.name == "file" || call.name == "文件") {
            // 简化解析，实际应该解析 JSON 参数
            return FileTool::execute("read", call.arguments);
        }
        else if (call.name == "code_execute" || call.name == "代码") {
            return CodeTool::execute(call.arguments);
        }
        
        return ToolResult::fail(
            "未知工具: " + call.name + "\n"
            "可用工具: get_weather, get_time, calculate, http_get, file, code_execute\n"
            "（Step 8 新增：http_get, file, code_execute 带安全检查）"
        );
    }
    
    size_t max_concurrent_;
    std::atomic<size_t> running_count_{0};
    mutable std::mutex mutex_;
    std::queue<PendingTask> pending_queue_;
    std::condition_variable cv_;
};
