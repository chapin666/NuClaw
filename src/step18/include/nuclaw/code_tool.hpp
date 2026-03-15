// ============================================================================
// code_tool.hpp - Step 9: 改造为继承 Tool 基类（从 Step 8 演进）
// ============================================================================

#pragma once

#include "tool.hpp"
#include "sandbox.hpp"

class CodeTool : public Tool {
public:
    std::string get_name() const override { return "code_execute"; }
    
    std::string get_description() const override {
        return "执行 Python 代码（带注入防护）";
    }
    
    ToolResult execute(const std::string& code) const override {
        // Step 8 原有：安全检查
        if (!Sandbox::is_safe_code(code)) {
            return ToolResult::fail("代码安全检查失败：包含危险操作");
        }
        
        // 模拟执行结果
        json::object data;
        data["code"] = code;
        data["result"] = "模拟执行结果";
        data["status"] = "success";
        
        return ToolResult::ok(json::serialize(data));
    }
};
