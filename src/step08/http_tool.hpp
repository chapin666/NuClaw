// ============================================================================
// http_tool.hpp - HTTP 请求工具（Step 8 新增）
// ============================================================================
// 演进说明：
//   Step 8 新增：HTTP GET 工具，带 SSRF 防护
// ============================================================================

#pragma once

#include "tool.hpp"
#include "sandbox.hpp"
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/json.hpp>

namespace beast = boost::beast;
namespace http = beast::http;
namespace asio = boost::asio;
using tcp = asio::ip::tcp;

class HttpTool {
public:
    static std::string get_name() { return "http_get"; }
    
    static std::string get_description() {
        return "发送 HTTP GET 请求（带 SSRF 防护）";
    }
    
    static ToolResult execute(const std::string& url) {
        // 安全检查
        if (!Sandbox::is_safe_url(url)) {
            return ToolResult::fail("URL 安全检查失败：禁止访问内网或非 HTTP 协议");
        }
        
        try {
            // 简化版：实际应该使用 Boost.Beast 发送 HTTP 请求
            // 这里模拟成功响应
            json::object data;
            data["url"] = url;
            data["status"] = 200;
            data["body"] = "模拟 HTTP 响应内容（实际应调用真实 HTTP 客户端）";
            data["safety"] = "✓ SSRF check passed";
            
            return ToolResult::ok(json::serialize(data));
            
        } catch (const std::exception& e) {
            return ToolResult::fail(std::string("HTTP 请求失败: ") + e.what());
        }
    }
};
