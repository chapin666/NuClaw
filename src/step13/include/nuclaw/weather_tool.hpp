// ============================================================================
// weather_tool.hpp - 天气工具（Step 9 改造版）
// ============================================================================
// 改造：继承 Tool 基类，实现虚函数
// ============================================================================

#pragma once

#include "tool.hpp"
#include <boost/json.hpp>
#include <map>

namespace json = boost::json;

class WeatherTool : public Tool {
public:
    std::string get_name() const override { return "get_weather"; }
    
    std::string get_description() const override {
        return "查询指定城市的天气信息，支持北京、上海、深圳、广州";
    }
    
    ToolResult execute(const std::string& arguments) const override {
        // 模拟天气数据
        static const std::map<std::string, json::object> weather_data = {
            {"北京", {{"city", "北京"}, {"weather", "晴天"}, {"temp", 25}, {"aqi", 50}}},
            {"上海", {{"city", "上海"}, {"weather", "多云"}, {"temp", 22}, {"aqi", 45}}},
            {"深圳", {{"city", "深圳"}, {"weather", "小雨"}, {"temp", 28}, {"aqi", 30}}},
            {"广州", {{"city", "广州"}, {"weather", "阴天"}, {"temp", 26}, {"aqi", 55}}}
        };
        
        std::string city = arguments;
        // 去除空白
        city.erase(0, city.find_first_not_of(" \t\n\r"));
        city.erase(city.find_last_not_of(" \t\n\r") + 1);
        
        auto it = weather_data.find(city);
        if (it != weather_data.end()) {
            return ToolResult::ok(json::serialize(it->second));
        }
        
        // 默认返回北京天气
        return ToolResult::ok(json::serialize(weather_data.at("北京")));
    }
};
