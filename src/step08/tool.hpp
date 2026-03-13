// ============================================================================
// tool.hpp - 工具基类和结果定义
// ============================================================================

#pragma once

#include <string>
#include <boost/json.hpp>

namespace json = boost::json;

// 工具执行结果
struct ToolResult {
    bool success;
    std::string data;      // JSON 格式字符串
    std::string error;
    
    static ToolResult ok(const std::string& json_data) {
        return {true, json_data, ""};
    }
    
    static ToolResult fail(const std::string& err) {
        return {false, "", err};
    }
};

// 工具调用请求
struct ToolCall {
    std::string name;
    std::string arguments;
};
