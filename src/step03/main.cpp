// ============================================================================
// Step 3: Agent Loop - 状态机与对话管理
// ============================================================================
// 
// 文件结构：
//   main.cpp           - 程序入口，负责初始化和启动
//   agent_loop.hpp     - AgentLoop 类定义（状态机核心）
//   agent_loop.cpp     - AgentLoop 实现
//   ws_session.hpp     - WebSocket Session 类定义
//   ws_session.cpp     - WebSocket Session 实现
//   server.hpp         - Server 类定义
//   server.cpp         - Server 实现
//
// 依赖关系：
//   main.cpp -> server.hpp -> ws_session.hpp -> agent_loop.hpp
//
// 编译：mkdir build && cd build && cmake .. && make
// 运行：./nuclaw_step03
// ============================================================================

#include "server.hpp"
#include <iostream>
#include <thread>

// 入口函数：职责是初始化和启动，不包含业务逻辑
int main() {
    try {
        std::cout << "========================================\n";
        std::cout << "  NuClaw Step 3 - Agent Loop\n";
        std::cout << "========================================\n\n";
        
        // 1. 创建事件循环（Asio 核心）
        asio::io_context io;
        
        // 2. 创建 Agent（业务逻辑核心）
        AgentLoop agent;
        std::cout << "[✓] Agent Loop initialized\n";
        
        // 3. 创建服务器（网络层）
        Server server(io, 8081, agent);
        std::cout << "[✓] WebSocket server started on ws://localhost:8081\n\n";
        
        // 4. 打印测试提示
        std::cout << "Test commands:\n";
        std::cout << "  1. npm install -g wscat\n";
        std::cout << "  2. wscat -c ws://localhost:8081\n";
        std::cout << "  3. Send: Hello Agent!\n\n";
        std::cout << "Press Ctrl+C to stop\n\n";
        
        // 5. 启动多线程事件循环
        // 使用 hardware_concurrency 获取 CPU 核心数
        unsigned thread_count = std::thread::hardware_concurrency();
        std::cout << "[i] Starting " << thread_count << " worker threads...\n";
        
        std::vector<std::thread> threads;
        for (unsigned i = 0; i < thread_count; ++i) {
            // 每个线程都运行 io_context::run()
            // 这意味着所有线程都可以处理异步事件
            threads.emplace_back([&io]() { 
                io.run(); 
            });
        }
        
        // 6. 等待所有线程结束（阻塞）
        for (auto& t : threads) {
            t.join();
        }
        
    } catch (const std::exception& e) {
        std::cerr << "[✗] Fatal error: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
