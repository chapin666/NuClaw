// ============================================================================
// Step 8: Tools 系统（下）- 工具生态：HTTP、文件操作、安全沙箱
// ============================================================================
//
// 本文件：程序入口
// 其他文件：
//   - sandbox.hpp       : 安全沙箱
//   - tool.hpp          : Tool 基类
//   - http_tool.hpp     : HttpGetTool
//   - file_tool.hpp     : FileTool
//   - code_tool.hpp     : CodeExecuteTool
//   - registry.hpp      : ToolRegistry
//   - server.hpp/cpp    : WebSocket Server
//
// 编译: mkdir build && cd build && cmake .. && make
// 运行: ./nuclaw_step08
// ============================================================================

#include "server.hpp"
#include <iostream>
#include <thread>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

int main() {
    try {
        std::cout << "========================================\n";
        std::cout << "  NuClaw Step 8 - Tool Ecosystem\n";
        std::cout << "========================================\n\n";
        
        asio::io_context io;
        Agent agent;
        Server server(io, 8081, agent);
        
        std::cout << "[✓] WebSocket server started on ws://localhost:8081\n\n";
        std::cout << "Available tools:\n";
        std::cout << "  - http_get     : Send HTTP GET request (SSRF protected)\n";
        std::cout << "  - file         : File operations (read/write/list)\n";
        std::cout << "  - code_execute : Execute Python code (blacklist filter)\n\n";
        std::cout << "Send JSON: {\"tool\": \"name\", \"args\": {...}}\n\n";
        
        fs::create_directories("data");
        
        std::vector<std::thread> threads;
        for (unsigned i = 0; i < std::thread::hardware_concurrency(); ++i) {
            threads.emplace_back([&io]() { io.run(); });
        }
        for (auto& t : threads) t.join();
        
    } catch (std::exception& e) {
        std::cerr << "[✗] Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
