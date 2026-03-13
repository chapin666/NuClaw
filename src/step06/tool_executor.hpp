// ============================================================================
// tool_executor.hpp/cpp - 工具执行器（硬编码版）
// ============================================================================

#pragma once

#include "tool.hpp"
#include "weather_tool.hpp"
#include "time_tool.hpp"
#include "calc_tool.hpp"
#include <iostream>
#include <vector>
#include <string>

// 硬编码的工具执行器
// 注意：每加一个工具都要修改这里！这是硬编码的痛苦。
class ToolExecutor {
public:
    // 执行工具调用
    static ToolResult execute(const ToolCall& call) {
        std::cout << "[⚙️ Executing tool] " << call.name 
                  << "(" << call.arguments << ")" << std::endl;
        
        // 硬编码分发 - 这就是问题所在！
        // 每增加一个新工具，都要在这里添加 if 分支
        if (call.name == "get_weather" || call.name == "天气") {
            return WeatherTool::execute(call.arguments);
        }
        else if (call.name == "get_time" || call.name == "时间") {
            return TimeTool::execute(call.arguments);
        }
        else if (call.name == "calculate" || call.name == "计算") {
            return CalcTool::execute(call.arguments);
        }
        
        return ToolResult::fail(
            "未知工具: " + call.name + "\n"
            "可用工具: get_weather, get_time, calculate\n"
            "提示：每加一个工具都要改这里（硬编码问题，Step 7 解决）"
        );
    }
    
    // 获取工具描述（用于提示 LLM）
    static std::string get_tools_description() {
        return R"(可用工具：
1. get_weather(city: string) - 查询城市天气
   支持：北京、上海、深圳、广州
   
2. get_time() - 获取当前时间
   返回：日期时间字符串
   
3. calculate(expression: string) - 数学计算
   支持：+ - * /，如：1+2、10*5

使用方式：
如果用户问题需要实时数据，请回复：TOOL_CALL: {"tool": "工具名", "args": "参数"}
否则直接回答。)";
    }
};
