// ============================================================================
// time_tool.hpp - 时间工具（Step 9 改造版）
// ============================================================================

#pragma once

#include "tool.hpp"
#include <chrono>
#include <iomanip>
#include <sstream>

class TimeTool : public Tool {
public:
    std::string get_name() const override { return "get_time"; }
    
    std::string get_description() const override {
        return "获取当前系统时间";
    }
    
    ToolResult execute(const std::string& /*arguments*/) const override {
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        
        std::stringstream ss;
        ss << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S");
        
        json::object data;
        data["datetime"] = ss.str();
        data["timezone"] = "本地时间";
        
        return ToolResult::ok(json::serialize(data));
    }
};
