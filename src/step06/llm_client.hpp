// ============================================================================
// llm_client.hpp - LLM 客户端（简化版）
// ============================================================================

#pragma once

#include "tool.hpp"
#include <string>
#include <vector>
#include <utility>
#include <sstream>
#include <boost/json.hpp>

namespace json = boost::json;

// 前向声明
class ToolExecutor;

class LLMClient {
public:
    LLMClient(const std::string& api_key = "") : api_key_(api_key) {}
    
    // 判断是否需要调用工具
    bool needs_tool(const std::string& input) const {
        // 简化版：关键词匹配
        // 真实 LLM 会基于语义判断
        std::vector<std::string> tool_keywords = {
            "天气", "温度", "下雨", "晴天",
            "时间", "几点", "日期",
            "计算", "等于", "+", "-", "*", "/", "加", "减", "乘", "除"
        };
        
        for (const auto& kw : tool_keywords) {
            if (input.find(kw) != std::string::npos) {
                return true;
            }
        }
        return false;
    }
    
    // 解析工具调用（简化版）
    ToolCall parse_tool_call(const std::string& user_input) {
        ToolCall call;
        
        // 超简化解析，实际应该用 NLP
        if (user_input.find("天气") != std::string::npos ||
            user_input.find("温度") != std::string::npos) {
            call.name = "get_weather";
            if (user_input.find("北京") != std::string::npos) call.arguments = "北京";
            else if (user_input.find("上海") != std::string::npos) call.arguments = "上海";
            else if (user_input.find("深圳") != std::string::npos) call.arguments = "深圳";
            else if (user_input.find("广州") != std::string::npos) call.arguments = "广州";
            else call.arguments = "北京";
        }
        else if (user_input.find("时间") != std::string::npos ||
                 user_input.find("几点") != std::string::npos) {
            call.name = "get_time";
            call.arguments = "";
        }
        else if (user_input.find("计算") != std::string::npos ||
                 user_input.find("等于") != std::string::npos ||
                 user_input.find("+") != std::string::npos) {
            call.name = "calculate";
            call.arguments = extract_expression(user_input);
        }
        
        return call;
    }
    
    // 根据工具结果生成回复
    std::string generate_response(const std::string& user_input,
                                   const ToolResult& tool_result,
                                   const ToolCall& call) {
        if (!tool_result.success) {
            return "抱歉，工具调用失败: " + tool_result.error;
        }
        
        try {
            json::value data = json::parse(tool_result.data);
            
            if (call.name == "get_weather") {
                std::string city = std::string(data.as_object()["city"].as_string());
                std::string weather = std::string(data.as_object()["weather"].as_string());
                int temp = static_cast<int>(data.as_object()["temp"].as_int64());
                return city + "今天" + weather + "，气温" + std::to_string(temp) + "°C。";
            }
            else if (call.name == "get_time") {
                std::string dt = std::string(data.as_object()["datetime"].as_string());
                return "当前时间是 " + dt;
            }
            else if (call.name == "calculate") {
                std::string expr = std::string(data.as_object()["expression"].as_string());
                double res = data.as_object()["result"].as_double();
                
                std::ostringstream oss;
                oss << expr << " = " << res;
                return oss.str();
            }
        } catch (const std::exception& e) {
            return "工具执行成功，但解析结果出错";
        }
        
        return "工具执行成功";
    }
    
    // 直接回答（不需要工具）
    std::string direct_reply(const std::string& user_input) {
        if (user_input.find("你好") != std::string::npos ||
            user_input.find("hello") != std::string::npos) {
            return "你好！我是带工具调用能力的 AI。\n"
                   "我可以帮你：查天气、算数学、看时间。\n"
                   "试试问我：北京天气如何？";
        }
        
        if (user_input.find("工具") != std::string::npos ||
            user_input.find("能做什么") != std::string::npos) {
            return "我可以使用以下工具：\n"
                   "  - get_weather: 查询城市天气\n"
                   "  - get_time: 获取当前时间\n"
                   "  - calculate: 数学计算";
        }
        
        return "我可以帮你查天气、算数学、看时间。\n"
               "或者问'你能做什么'了解更多。";
    }
    
    bool is_configured() const { return !api_key_.empty(); }

private:
    std::string api_key_;
    
    std::string extract_expression(const std::string& input) {
        std::string expr;
        for (char c : input) {
            if (std::isdigit(c) || c == '+' || c == '-' || c == '*' || c == '/' || c == '.' || c == ' ') {
                expr += c;
            }
        }
        return expr.empty() ? "1 + 1" : expr;
    }
};

// 前向声明
class ToolExecutor;
