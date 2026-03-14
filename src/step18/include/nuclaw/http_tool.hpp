// ============================================================================
// http_tool.hpp - Step 9: 改造为继承 Tool 基类（从 Step 8 演进）
// ============================================================================

#pragma once

#include "tool.hpp"
#include "sandbox.hpp"

class HttpTool : public Tool {
public:
    std::string get_name() const override { return "http_get"; }
    
    std::string get_description() const override {
        return "发送 HTTP GET 请求（带 SSRF 防护）";
    }
    
    ToolResult execute(const std::string& url) const override {
        // Step 8 原有：安全检查
        if (!Sandbox::is_safe_url(url)) {
            return ToolResult::fail("URL 安全检查失败：禁止访问内网或非 HTTP 协议");
        }
        
        // 模拟 HTTP 响应
        json::object data;
        data["url"] = url;
        data["status"] = 200;
        data["body"] = "模拟 HTTP 响应内容";
        data["safety"] = "✓ SSRF check passed";
        
        return ToolResult::ok(json::serialize(data));
    }
};
