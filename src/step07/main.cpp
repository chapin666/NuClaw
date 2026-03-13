// ============================================================================
// Step 7: 异步工具执行 - 基于 Step 6 演进
// ============================================================================
//
// 演进说明：
//   Step 6: 同步工具执行，阻塞等待
//   Step 7: 异步工具执行，支持并发和超时
//
// 关键变化：
//   - tool_executor.hpp: 新增 execute_async(), 并发控制, 超时
//   - chat_engine.hpp: 新增 process_async(), 支持回调
//   - main.cpp: 演示并发执行多个工具
//
// 编译: mkdir build && cd build && cmake .. && make
// 运行: ./nuclaw_step07
// 测试: wscat -c ws://localhost:8080/ws
//
// ============================================================================

#include "chat_engine.hpp"
#include "server.hpp"
#include <iostream>
#include <thread>
#include <vector>
#include <chrono>

int main() {
    try {
        asio::io_context io;
        
        std::cout << "========================================" << std::endl;
        std::cout << "  NuClaw Step 7 - Async Tool Execution" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << std::endl;
        
        std::cout << "📊 演进对比：" << std::endl;
        std::cout << "  Step 6: 同步执行 → 阻塞等待" << std::endl;
        std::cout << "  Step 7: 异步执行 → 立即返回 + 回调" << std::endl;
        std::cout << std::endl;
        
        ChatEngine ai;
        Server server(io, 8080, ai);
        
        std::cout << "✅ Server started on ws://localhost:8080" << std::endl;
        std::cout << std::endl;
        
        // 演示异步执行
        std::cout << "🧪 演示异步执行（非交互模式）：" << std::endl;
        std::cout << std::endl;
        
        ChatContext ctx;
        std::atomic<int> completed{0};
        int total = 5;
        
        std::cout << "提交 " << total << " 个工具调用..." << std::endl;
        
        auto start = std::chrono::steady_clock::now();
        
        // 同时提交多个工具调用
        for (int i = 0; i < total; i++) {
            std::string city = (i % 2 == 0) ? "北京" : "上海";
            std::string input = city + "天气";
            
            ai.process_async(input, ctx, [&completed, i, city](std::string reply) {
                std::cout << "  [✓] " << city << " completed: " 
                          << reply.substr(0, 20) << "..." << std::endl;
                completed++;
            });
        }
        
        // 等待所有完成
        while (completed < total) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            ai.print_executor_status();
        }
        
        auto end = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        
        std::cout << std::endl;
        std::cout << "✅ 全部完成！总耗时: " << elapsed << "ms" << std::endl;
        std::cout << "   （如果是同步执行，需要 " << (total * 100) << "ms）" << std::endl;
        std::cout << std::endl;
        
        std::cout << "📝 代码演进（基于 Step 6）：" << std::endl;
        std::cout << "  tool_executor.hpp: +异步执行, +并发控制, +超时" << std::endl;
        std::cout << "  chat_engine.hpp:   +process_async()" << std::endl;
        std::cout << std::endl;
        
        std::cout << "🚀 现在可以用 wscat 测试交互模式：" << std::endl;
        std::cout << "  wscat -c ws://localhost:8080/ws" << std::endl;
        std::cout << std::endl;
        
        // 进入服务循环
        std::vector<std::thread> threads;
        for (unsigned i = 0; i < std::thread::hardware_concurrency(); ++i) {
            threads.emplace_back([&io]() { io.run(); });
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
    } catch (std::exception& e) {
        std::cerr << "[!] Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
