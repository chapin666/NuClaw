// ============================================================================
// tool.hpp - 工具抽象基类和基础定义
// ============================================================================
// Step 9 新增：工具抽象基类，定义统一接口
// 所有具体工具必须继承此基类
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

// ============================================================================
// 工具抽象基类 - Step 9 核心新增
// ============================================================================
class Tool {
public:
    virtual ~Tool() = default;
    
    // 获取工具名称（唯一标识）
    virtual std::string get_name() const = 0;
    
    // 获取工具描述（用于 LLM 理解）
    virtual std::string get_description() const = 0;
    
    // 执行工具
    virtual ToolResult execute(const std::string& arguments) const = 0;
};
