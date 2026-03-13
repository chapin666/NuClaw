// ============================================================================
// tool_executor.hpp - Step 9: 演进为使用注册表（从 Step 8 演进）
// ============================================================================
// 演进说明：
//   Step 8: 硬编码分发
//   Step 9: 使用注册表，支持动态扩展
// ============================================================================

#pragma once

#include "tool.hpp"
#include "tool_registry.hpp"
#include "weather_tool.hpp"
#include "time_tool.hpp"
#include "calc_tool.hpp"
#include "http_tool.hpp"
#include "file_tool.hpp"
#include "code_tool.hpp"
#include <iostream>

class ToolExecutor {
public:
    // Step 9 演进：使用注册表替代硬编码
    static ToolResult execute_sync(const ToolCall& call) {
        std::cout << "[⚙️ 执行工具] " << call.name << std::endl;
        
        // 从注册表获取工具
        auto tool = ToolRegistry::instance().get_tool(call.name);
        
        if (!tool) {
            return ToolResult::fail(
                "未知工具: " + call.name + "\n" +
                "可用工具: " + get_available_tools()
            );
        }
        
        // 通过虚函数调用，多态执行
        return tool->execute(call.arguments);
    }
    
    // 获取可用工具列表
    static std::string get_available_tools() {
        auto names = ToolRegistry::instance().get_tool_names();
        std::string result;
        for (size_t i = 0; i < names.size(); ++i) {
            if (i > 0) result += ", ";
            result += names[i];
        }
        return result;
    }
    
    // 检查工具是否存在
    static bool has_tool(const std::string& name) {
        return ToolRegistry::instance().has_tool(name);
    }
    
    // Step 9 新增：初始化所有工具到注册表
    static void init_tools() {
        auto& registry = ToolRegistry::instance();
        
        // 注册 Step 8 原有工具
        registry.register_tool(std::make_shared<WeatherTool>());
        registry.register_tool(std::make_shared<TimeTool>());
        registry.register_tool(std::make_shared<CalcTool>());
        
        // 注册 Step 8 新增的安全工具
        registry.register_tool(std::make_shared<HttpTool>());
        registry.register_tool(std::make_shared<FileTool>());
        registry.register_tool(std::make_shared<CodeTool>());
        
        std::cout << "[✓] 已注册 " << registry.size() << " 个工具" << std::endl;
    }
};
