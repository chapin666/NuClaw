// ============================================================================
// system_tool.hpp - 系统信息工具
// ============================================================================

#pragma once

#include "tool.hpp"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <thread>

class SystemInfoTool : public Tool {
public:
    ToolSchema get_schema() const override {
        return {
            "system_info",
            "获取系统信息",
            {{"type", ParamType::STRING, "信息类型: time/memory/cpu", true}}
        };
    }
    
    json::value execute(const json::object& args) override {
        std::string type(args.at("type").as_string());
        json::object result;
        result["success"] = true;
        result["type"] = type;
        
        if (type == "time") {
            auto now = std::chrono::system_clock::now();
            auto time = std::chrono::system_clock::to_time_t(now);
            std::stringstream ss;
            ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
            result["time"] = ss.str();
            result["timestamp"] = static_cast<int64_t>(time);
        }
        else if (type == "cpu") {
            result["cores"] = static_cast<int>(std::thread::hardware_concurrency());
        }
        else if (type == "memory") {
            result["note"] = "Memory info not implemented in demo";
        }
        else {
            return make_error("Unknown type: " + type);
        }
        
        return result;
    }

private:
    json::value make_error(const std::string& msg) {
        json::object error;
        error["success"] = false;
        error["error"] = msg;
        return error;
    }
};
