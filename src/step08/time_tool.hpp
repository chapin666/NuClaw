// ============================================================================
// time_tool.hpp - 时间查询工具
// ============================================================================

#pragma once

#include "tool.hpp"
#include <boost/json.hpp>
#include <chrono>
#include <time>
#include <sstream>
#include <iomanip>

namespace json = boost::json;

class TimeTool {
public:
    static std::string get_name() { return "get_time"; }
    
    static std::string get_description() {
        return "获取当前时间";
    }
    
    static ToolResult execute(const std::string& timezone = "") {
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        
        char buf[100];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
        
        json::object data;
        data["datetime"] = buf;
        data["timestamp"] = static_cast<int64_t>(t);
        data["timezone"] = timezone.empty() ? "本地时间" : timezone;
        
        return ToolResult::ok(json::serialize(data));
    }
};
