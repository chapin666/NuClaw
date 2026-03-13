// ============================================================================
// Step 8: 工具安全与沙箱 - 基于 Step 7 演进
// ============================================================================
//
// 演进说明：
//   Step 7: 异步执行，并发控制
//   Step 8: + 安全沙箱，新工具（http, file, code）
//
// 新增：
//   - sandbox.hpp:     安全沙箱（SSRF/路径遍历/代码注入防护）
//   - http_tool.hpp:   HTTP 工具（带 SSRF 防护）
//   - file_tool.hpp:   文件工具（带路径遍历防护）
//   - code_tool.hpp:   代码执行（带黑名单防护）
//
// 编译: mkdir build && cd build && cmake .. && make
// 运行: ./nuclaw_step08
// 测试: wscat -c ws://localhost:8080/ws
//
// ============================================================================

#include "chat_engine.hpp"
#include "server.hpp"
#include "sandbox.hpp"
#include <iostream>
#include <thread>
#include <vector>

int main() {
    try {
        asio::io_context io;
        
        std::cout << "========================================" << std::endl;
        std::cout << "  NuClaw Step 8 - Tool Security" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << std::endl;
        
        std::cout << "🛡️  演进对比：" << std::endl;
        std::cout << "  Step 7: 异步执行 + 并发控制" << std::endl;
        std::cout << "  Step 8: + 安全沙箱 + 新工具" << std::endl;
        std::cout << std::endl;
        
        std::cout << "📁 新增文件：" << std::endl;
        std::cout << "  - sandbox.hpp       安全沙箱" << std::endl;
        std::cout << "  - http_tool.hpp     HTTP工具(SSRF防护)" << std::endl;
        std::cout << "  - file_tool.hpp     文件工具(路径防护)" << std::endl;
        std::cout << "  - code_tool.hpp     代码执行(黑名单防护)" << std::endl;
        std::cout << std::endl;
        
        std::cout << Sandbox::get_sandbox_info() << std::endl;
        
        ChatEngine ai;
        Server server(io, 8080, ai);
        
        std::cout << "✅ Server: ws://localhost:8080" << std::endl;
        std::cout << std::endl;
        
        std::cout << "🧪 安全测试示例：" << std::endl;
        std::cout << "  安全: http_get https://api.example.com/data" << std::endl;
        std::cout << "  危险: http_get http://localhost/admin  ← 会被拦截" << std::endl;
        std::cout << "  危险: file ../../../etc/passwd         ← 会被拦截" << std::endl;
        std::cout << "  危险: code_execute 'import os'         ← 会被拦截" << std::endl;
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
