// ============================================================================
// tool.hpp - Step 9: 添加抽象基类（基于 Step 8 演进）
// ============================================================================
// 演进说明：
//   Step 8: tool.hpp 只有 ToolResult 和 ToolCall 结构体
//   Step 9: 添加 Tool 抽象基类，支持多态
// ============================================================================

#pragma once

#include <string>
#include <boost/json.hpp>

namespace json = boost::json;

// 工具执行结果（与 Step 8 相同）
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

// 工具调用请求（与 Step 8 相同）
struct ToolCall {
    std::string name;
    std::string arguments;
};

// ========== Step 9 新增：工具抽象基类 ==========
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
