// ============================================================================
// tool_executor.hpp - 异步工具执行器（Step 7 演进）
// ============================================================================
// 演进说明：
//   基于 Step 6 的同步执行器，添加异步执行能力
//   Step 6 问题：同步阻塞，无法并发
//   Step 7 解决：
//     1. 异步执行（不阻塞主线程）
//     2. 并发控制（限制同时执行的工具数）
//     3. 超时控制（防止工具无限执行）
// ============================================================================

#pragma once

#include "tool.hpp"
#include "weather_tool.hpp"
#include "time_tool.hpp"
#include "calc_tool.hpp"
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

// Step 7 演进：异步工具执行器
class ToolExecutor {
public:
    ToolExecutor(size_t max_concurrent = 3) 
        : max_concurrent_(max_concurrent), running_count_(0) {}
    
    // Step 6 的同步执行（保留兼容）
    static ToolResult execute_sync(const ToolCall& call) {
        return execute_tool(call);
    }
    
    // Step 7 新增：异步执行（不阻塞）
    // @param call: 工具调用请求
    // @param callback: 结果回调（异步完成后调用）
    // @param timeout_ms: 超时时间（毫秒）
    void execute_async(const ToolCall& call, 
                       AsyncCallback<ToolResult> callback,
                       std::chrono::milliseconds timeout_ms = std::chrono::seconds(5)) {
        
        // 如果并发已满，加入等待队列
        {
            std::unique_lock<std::mutex> lock(mutex_);
            if (running_count_ >= max_concurrent_) {
                std::cout << "  [⏳ Queued] " << call.name << " (running: " 
                          << running_count_ << "/" << max_concurrent_ << ")" << std::endl;
                
                // 加入队列
                pending_queue_.push({call, callback, timeout_ms});
                return;
            }
            
            running_count_++;
        }
        
        // 立即执行
        std::cout << "  [⚙️ Running] " << call.name << " (" << running_count_ 
                  << "/" << max_concurrent_ << ")" << std::endl;
        
        // 在新线程中执行（异步）
        std::thread([this, call, callback, timeout_ms]() {
            auto result = execute_with_timeout(call, timeout_ms);
            
            // 回调通知结果
            callback(result);
            
            // 完成，检查队列
            {
                std::unique_lock<std::mutex> lock(mutex_);
                running_count_--;
                
                // 检查等待队列
                if (!pending_queue_.empty()) {
                    auto next = pending_queue_.front();
                    pending_queue_.pop();
                    lock.unlock();
                    
                    // 递归调用，处理队列中的下一个
                    execute_async(next.call, next.callback, next.timeout);
                }
            }
        }).detach();
    }
    
    // 带超时的执行
    static ToolResult execute_with_timeout(const ToolCall& call,
                                            std::chrono::milliseconds timeout_ms) {
        // 使用 std::async 和 future 实现超时
        auto future = std::async(std::launch::async, [call]() {
            return execute_tool(call);
        });
        
        // 等待结果（带超时）
        if (future.wait_for(timeout_ms) == std::future_status::timeout) {
            return ToolResult::fail("工具执行超时 (" + std::to_string(timeout_ms.count()) + "ms)");
        }
        
        return future.get();
    }
    
    // 获取等待队列长度
    size_t get_queue_length() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return pending_queue_.size();
    }
    
    // 获取当前运行数
    size_t get_running_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return running_count_;
    }

private:
    // 待执行的任务
    struct PendingTask {
        ToolCall call;
        AsyncCallback<ToolResult> callback;
        std::chrono::milliseconds timeout;
    };
    
    // 实际的工具执行（从 Step 6 继承）
    static ToolResult execute_tool(const ToolCall& call) {
        // 模拟执行时间（实际工具可能需要时间）
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        if (call.name == "get_weather" || call.name == "天气") {
            return WeatherTool::execute(call.arguments);
        }
        else if (call.name == "get_time" || call.name == "时间") {
            return TimeTool::execute(call.arguments);
        }
        else if (call.name == "calculate" || call.name == "计算") {
            return CalcTool::execute(call.arguments);
        }
        
        return ToolResult::fail("未知工具: " + call.name);
    }
    
    size_t max_concurrent_;                      // 最大并发数
    std::atomic<size_t> running_count_{0};       // 当前运行数
    mutable std::mutex mutex_;
    std::queue<PendingTask> pending_queue_;      // 等待队列
    std::condition_variable cv_;
};
