// ============================================================================
// main.cpp - Step 9: 工具注册表模式
// ============================================================================
// 演示注册表模式的使用
// ============================================================================

#include "tool_registry.hpp"
#include "weather_tool.hpp"
#include "time_tool.hpp"
#include "calc_tool.hpp"
#include "chat_engine.hpp"
#include <iostream>

// 初始化所有工具
void init_tools() {
    auto& registry = ToolRegistry::instance();
    
    // 注册工具 - 新增工具只需在这里添加一行！
    registry.register_tool(std::make_shared<WeatherTool>());
    registry.register_tool(std::make_shared<TimeTool>());
    registry.register_tool(std::make_shared<CalcTool>());
    
    std::cout << "[✓] 已注册 " << registry.size() << " 个工具" << std::endl;
}

int main() {
    std::cout << "========================================\n";
    std::cout << "   NuClaw Step 9: 工具注册表模式\n";
    std::cout << "========================================\n\n";
    
    // 1. 初始化工具
    init_tools();
    
    // 2. 显示工具列表
    std::cout << ToolRegistry::instance().get_tools_description() << std::endl;
    
    // 3. 创建 ChatEngine
    ChatEngine engine;
    ChatContext ctx;
    
    std::cout << "\n" << engine.get_welcome_message() << "\n\n";
    
    // 4. 演示交互
    std::vector<std::string> test_inputs = {
        "你好",
        "北京天气如何？",
        "现在几点？",
        "25 * 4 等于多少？",
        "有哪些工具？"
    };
    
    for (const auto& input : test_inputs) {
        std::cout << "\n----------------------------------------\n";
        std::cout << "👤 用户: " << input << std::endl;
        std::string reply = engine.process(input, ctx);
        std::cout << "🤖 AI: " << reply << std::endl;
    }
    
    std::cout << "\n========================================\n";
    std::cout << "演示完成！\n";
    std::cout << "========================================\n";
    
    return 0;
}
