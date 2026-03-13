// ============================================================================
// Step 6: 工具调用（硬编码版）- 完整的 Agent Loop
// ============================================================================
// 
// 这是第一个完整功能的 AI Agent，包含：
// - WebSocket 实时通信
// - LLM 语义理解
// - 工具调用能力
//
// 但工具有硬编码问题，Step 7 将解决。
//
// 编译: mkdir build && cd build && cmake .. && make
// 运行: ./nuclaw_step06
// 测试: wscat -c ws://localhost:8080/ws
// ============================================================================

#include "chat_engine.hpp"
#include "server.hpp"
#include "tool_executor.hpp"
#include <iostream>
#include <thread>
#include <vector>

int main() {
    try {
        asio::io_context io;
        
        std::cout << "========================================" << std::endl;
        std::cout << "  NuClaw Step 6 - Tool Calling" << std::endl;
        std::cout << "========================================" << std::endl;
        
        ChatEngine ai;
        Server server(io, 8080, ai);
        
        std::cout << std::endl;
        std::cout << "🚀 Agent Loop 完整了！" << std::endl;
        std::cout << std::endl;
        std::cout << "架构：" << std::endl;
        std::cout << "  感知(WebSocket) → 理解(LLM) → 决策 → 行动(工具) → 响应" << std::endl;
        std::cout << std::endl;
        
        std::cout << "📁 代码结构（模块化）：" << std::endl;
        std::cout << "  - tool.hpp              工具基类" << std::endl;
        std::cout << "  - weather_tool.hpp      天气工具" << std::endl;
        std::cout << "  - time_tool.hpp         时间工具" << std::endl;
        std::cout << "  - calc_tool.hpp         计算工具" << std::endl;
        std::cout << "  - tool_executor.hpp     工具执行器" << std::endl;
        std::cout << "  - llm_client.hpp        LLM客户端" << std::endl;
        std::cout << "  - chat_engine.hpp       Agent核心" << std::endl;
        std::cout << "  - server.hpp            WebSocket服务器" << std::endl;
        std::cout << std::endl;
        
        std::cout << "⚠️  硬编码问题：" << std::endl;
        std::cout << "  每加一个工具要改 tool_executor.hpp" << std::endl;
        std::cout << "  下一章：Tool 系统架构（注册表模式）" << std::endl;
        std::cout << std::endl;
        
        std::cout << "测试: wscat -c ws://localhost:8080/ws" << std::endl;
        std::cout << std::endl;
        
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
