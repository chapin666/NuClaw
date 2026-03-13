// ============================================================================
// sandbox.hpp - 安全沙箱（Step 8 新增）
// ============================================================================
// 演进说明：
//   Step 8 新增：工具安全沙箱
//   防止恶意操作：路径遍历、SSRF、代码注入
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <algorithm>

// 安全沙箱
class Sandbox {
public:
    // URL 安全检查（防止 SSRF）
    static bool is_safe_url(const std::string& url) {
        // 禁止内网地址
        if (url.find("localhost") != std::string::npos) return false;
        if (url.find("127.0.") != std::string::npos) return false;
        if (url.find("192.168.") != std::string::npos) return false;
        if (url.find("10.") != std::string::npos) return false;
        
        // 只允许 http/https
        if (url.find("http://") != 0 && url.find("https://") != 0) {
            return false;
        }
        
        return true;
    }
    
    // 路径安全检查（防止路径遍历）
    static bool is_safe_path(const std::string& path) {
        // 禁止 .. 遍历
        if (path.find("..") != std::string::npos) return false;
        
        // 禁止绝对路径（简化）
        if (!path.empty() && path[0] == '/') return false;
        
        return true;
    }
    
    // 代码安全检查（防止代码注入）
    static bool is_safe_code(const std::string& code) {
        // 黑名单检查
        std::vector<std::string> blacklist = {
            "import os", "import sys", "__import__",
            "open(", "file(", "exec(", "eval(",
            "subprocess", "socket", "urllib",
            "rm -rf", "mkfs", "dd if"
        };
        
        for (const auto& bad : blacklist) {
            if (code.find(bad) != std::string::npos) {
                return false;
            }
        }
        
        // 限制代码长度
        if (code.length() > 10000) {
            return false;
        }
        
        return true;
    }
    
    // 获取沙箱说明
    static std::string get_sandbox_info() {
        return "安全限制：\n"
               "- URL: 禁止内网地址\n"
               "- 文件: 禁止路径遍历\n"
               "- 代码: 禁止危险操作\n";
    }
};
