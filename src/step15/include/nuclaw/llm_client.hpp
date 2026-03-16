// ============================================================================
// llm_client.hpp - LLM 客户端（简化版）
// ============================================================================

#pragma once

#include "tool.hpp"
#include "tool_registry.hpp"
#include <string>
#include <vector>

struct Message {
    std::string role;      // system, user, assistant
    std::string content;
};

class LLMClient {
public:
    // 判断是否需要工具（关键词匹配简化版）
    bool needs_tool(const std::string& input) const {
        std::vector<std::string> tool_keywords = {
            "天气", "温度", "下雨", "晴天",
            "时间", "几点", "日期", "现在",
            "计算", "等于", "+", "-", "*", "/"
        };
        
        for (const auto& kw : tool_keywords) {
            if (input.find(kw) != std::string::npos) {
                return true;
            }
        }
        return false;
    }
    
    // 解析工具调用
    ToolCall parse_tool_call(const std::string& input) const {
        ToolCall call;
        
        // 简单关键词匹配
        if (input.find("天气") != std::string::npos) {
            call.name = "get_weather";
            if (input.find("北京") != std::string::npos) call.arguments = "北京";
            else if (input.find("上海") != std::string::npos) call.arguments = "上海";
            else call.arguments = "北京";
        }
        else if (input.find("时间") != std::string::npos) {
            call.name = "get_time";
        }
        else if (input.find("计算") != std::string::npos ||
                 input.find("+") != std::string::npos ||
                 input.find("-") != std::string::npos) {
            call.name = "calculate";
            // 提取表达式
            call.arguments = extract_expression(input);
        }
        
        return call;
    }
    
    // 生成回复
    std::string generate_response(const std::string& user_input,
                                   const ToolResult& result,
                                   const ToolCall& call) const {
        if (!result.success) {
            return "抱歉，工具执行失败: " + result.error;
        }
        
        // 解析工具返回的 JSON
        json::value data = json::parse(result.data);
        
        if (call.name == "get_weather") {
            std::string city = data["city"].as_string();
            std::string weather = data["weather"].as_string();
            int temp = static_cast<int>(data["temp"].as_int64());
            return city + "今天" + weather + "，气温" + std::to_string(temp) + "°C。";
        }
        else if (call.name == "get_time") {
            return "当前时间是 " + data["datetime"].as_string();
        }
        else if (call.name == "calculate") {
            return data["expression"].as_string() + " = " + 
                   std::to_string(data["result"].as_double());
        }
        
        return "工具执行成功";
    }
    
    // 直接回复
    std::string direct_reply(const std::string& input) const {
        if (input.find("你好") != std::string::npos ||
            input.find("您好") != std::string::npos) {
            return "你好！我是基于注册表模式的 AI Agent。我可以帮你查询天气、时间，或者进行数学计算。";
        }
        if (input.find("工具") != std::string::npos) {
            return ToolRegistry::instance().get_tools_description();
        }
        return "我不确定如何回答。试试问我天气、时间，或者让我进行计算。";
    }

private:
    std::string extract_expression(const std::string& input) const {
        // 简化：直接返回整个输入
        return input;
    }
};
