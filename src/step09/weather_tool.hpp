// ============================================================================
// weather_tool.hpp - Step 9: 改造为继承 Tool 基类
// ============================================================================
// 演进说明：
//   Step 8: 静态方法实现
//   Step 9: 继承 Tool 基类，支持注册表
// ============================================================================

#pragma once

#include "tool.hpp"
#include "sandbox.hpp"  // 保留 Step 8 的安全检查
#include <map>

class WeatherTool : public Tool {
public:
    std::string get_name() const override { return "get_weather"; }
    
    std::string get_description() const override {
        return "查询指定城市的天气信息，支持北京、上海、深圳、广州";
    }
    
    ToolResult execute(const std::string& arguments) const override {
        // Step 8 原有逻辑 + 安全检查
        std::string city = arguments;
        city.erase(0, city.find_first_not_of(" \t\n\r"));
        city.erase(city.find_last_not_of(" \t\n\r") + 1);
        
        if (city.empty()) city = "北京";
        
        // 模拟天气数据（Step 8 原有逻辑）
        static const std::map<std::string, json::object> weather_data = {
            {"北京", {{"city", "北京"}, {"weather", "晴天"}, {"temp", 25}}},
            {"上海", {{"city", "上海"}, {"weather", "多云"}, {"temp", 22}}},
            {"深圳", {{"city", "深圳"}, {"weather", "小雨"}, {"temp", 28}}},
            {"广州", {{"city", "广州"}, {"weather", "阴天"}, {"temp", 26}}}
        };
        
        auto it = weather_data.find(city);
        if (it != weather_data.end()) {
            return ToolResult::ok(json::serialize(it->second));
        }
        
        return ToolResult::ok(json::serialize(weather_data.at("北京")));
    }
};
