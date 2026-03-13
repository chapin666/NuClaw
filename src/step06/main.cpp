// ============================================================================
// Step 6: Tools 系统（上）- 同步工具、注册表、参数校验
// ============================================================================
//
// 本章核心：构建 Tool 系统的基础框架
//
// 编译: mkdir build && cd build && cmake .. && make
// 运行: ./nuclaw_step06
// 测试: wscat -c ws://localhost:8081
// ============================================================================

#include "registry.hpp"
#include "calculator.hpp"
#include "file_tool.hpp"
#include "system_tool.hpp"
#include "server.hpp"

#include <iostream>
#include <thread>
#include <vector>

int main() {
    try {
        std::cout << "========================================\n";
        std::cout << "  NuClaw Step 6 - Tool System (Sync)\n";
        std::cout << "========================================\n\n";
        
        // 创建注册表并注册工具
        ToolRegistry registry;
        registry.register_tool(std::make_shared<CalculatorTool>());
        registry.register_tool(std::make_shared<FileReadTool>());
        registry.register_tool(std::make_shared<SystemInfoTool>());
        
        std::cout << "[✓] " << registry.list_tools().size() << " tools registered\n\n";
        
        // 创建 Agent 和 Server
        asio::io_context io;
        AgentLoop agent(registry);
        Server server(io, 8081, agent);
        
        std::cout << "[✓] WebSocket server started on ws://localhost:8081\n\n";
        std::cout << "Example requests:\n";
        std::cout << "  {\"tool\": \"calculator\", \"args\": {\"expression\": \"3.14\"}}\n";
        std::cout << "  {\"tool\": \"system_info\", \"args\": {\"type\": \"time\"}}\n\n";
        
        // 启动服务器
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
