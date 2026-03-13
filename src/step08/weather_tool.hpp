// ============================================================================
// weather_tool.hpp - 天气查询工具（模拟实现）
// ============================================================================

#pragma once

#include "tool.hpp"
#include <boost/json.hpp>

namespace json = boost::json;

class WeatherTool {
public:
    static std::string get_name() { return "get_weather"; }
    
    static std::string get_description() {
        return "查询指定城市的天气信息";
    }
    
    static ToolResult execute(const std::string& city) {
        // 模拟天气数据（实际应该调用天气 API）
        json::object data;
        
        if (city == "北京" || city == "beijing") {
            data["city"] = "北京";
            data["weather"] = "晴天";
            data["temp"] = 25;
            data["aqi"] = 50;
        } 
        else if (city == "上海" || city == "shanghai") {
            data["city"] = "上海";
            data["weather"] = "多云";
            data["temp"] = 22;
            data["aqi"] = 45;
        }
        else if (city == "深圳" || city == "shenzhen") {
            data["city"] = "深圳";
            data["weather"] = "小雨";
            data["temp"] = 28;
            data["humidity"] = 80;
        }
        else if (city == "广州" || city == "guangzhou") {
            data["city"] = "广州";
            data["weather"] = "阴天";
            data["temp"] = 26;
        }
        else {
            return ToolResult::fail(
                "不支持的城市: " + city + "，目前支持：北京、上海、深圳、广州"
            );
        }
        
        return ToolResult::ok(json::serialize(data));
    }
};
