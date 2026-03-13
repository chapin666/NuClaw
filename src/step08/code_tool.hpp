// ============================================================================
// code_tool.hpp - 代码执行工具（Step 8 新增）
// ============================================================================
// 演进说明：
//   Step 8 新增：Python 代码执行，带安全黑名单
// ============================================================================

#pragma once

#include "tool.hpp"
#include "sandbox.hpp"
#include <boost/json.hpp>
#include <cstdio>
#include <array>
#include <memory>

namespace json = boost::json;

class CodeTool {
public:
    static std::string get_name() { return "code_execute"; }
    
    static std::string get_description() {
        return "执行 Python 代码（带安全限制）";
    }
    
    static ToolResult execute(const std::string& code, 
                               const std::string& language = "python") {
        // 安全检查
        if (!Sandbox::is_safe_code(code)) {
            return ToolResult::fail("代码安全检查失败：包含危险操作");
        }
        
        try {
            // 使用受限方式执行（实际应该使用更安全的方式）
            // 这里简化处理
            
            std::string cmd = "timeout 5 python3 -c '" + escape_shell(code) + "' 2>&1";
            
            FILE* pipe = popen(cmd.c_str(), "r");
            if (!pipe) {
                return ToolResult::fail("无法执行代码");
            }
            
            std::array<char, 4096> buffer;
            std::string output;
            while (fgets(buffer.data(), buffer.size(), pipe)) {
                output += buffer.data();
            }
            
            int status = pclose(pipe);
            
            json::object data;
            data["success"] = (status == 0);
            data["output"] = output;
            data["exit_code"] = status;
            data["safety"] = "✓ Code check passed";
            
            return ToolResult::ok(json::serialize(data));
            
        } catch (const std::exception& e) {
            return ToolResult::fail(std::string("代码执行失败: ") + e.what());
        }
    }

private:
    static std::string escape_shell(const std::string& s) {
        std::string result;
        for (char c : s) {
            if (c == '\'') result += "'\"'\"'";
            else result += c;
        }
        return result;
    }
};
