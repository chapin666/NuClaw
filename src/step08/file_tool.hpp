// ============================================================================
// file_tool.hpp - 文件操作工具（Step 8 新增）
// ============================================================================
// 演进说明：
//   Step 8 新增：文件读写工具，带路径遍历防护
// ============================================================================

#pragma once

#include "tool.hpp"
#include "sandbox.hpp"
#include <boost/json.hpp>
#include <fstream>
#include <sstream>
#include <filesystem>

namespace json = boost::json;
namespace fs = std::filesystem;

class FileTool {
public:
    static std::string get_name() { return "file"; }
    
    static std::string get_description() {
        return "文件操作：读/写/列表（带沙箱限制）";
    }
    
    // operation: "read", "write", "list"
    static ToolResult execute(const std::string& operation, 
                               const std::string& path,
                               const std::string& content = "") {
        // 安全检查
        if (!Sandbox::is_safe_path(path)) {
            return ToolResult::fail("路径安全检查失败：禁止 .. 或绝对路径");
        }
        
        try {
            if (operation == "read") {
                return file_read(path);
            } else if (operation == "write") {
                return file_write(path, content);
            } else if (operation == "list") {
                return file_list(path);
            }
            
            return ToolResult::fail("未知操作: " + operation);
            
        } catch (const std::exception& e) {
            return ToolResult::fail(std::string("文件操作失败: ") + e.what());
        }
    }

private:
    static ToolResult file_read(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            return ToolResult::fail("无法打开文件: " + path);
        }
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        
        json::object data;
        data["path"] = path;
        data["content"] = buffer.str();
        data["size"] = buffer.str().length();
        
        return ToolResult::ok(json::serialize(data));
    }
    
    static ToolResult file_write(const std::string& path, const std::string& content) {
        std::ofstream file(path);
        if (!file.is_open()) {
            return ToolResult::fail("无法创建文件: " + path);
        }
        
        file << content;
        
        json::object data;
        data["path"] = path;
        data["bytes_written"] = content.length();
        
        return ToolResult::ok(json::serialize(data));
    }
    
    static ToolResult file_list(const std::string& path) {
        json::object data;
        data["path"] = path;
        json::array files;
        
        if (fs::exists(path) && fs::is_directory(path)) {
            for (const auto& entry : fs::directory_iterator(path)) {
                json::object file_info;
                file_info["name"] = entry.path().filename().string();
                file_info["is_directory"] = entry.is_directory();
                files.push_back(file_info);
            }
        }
        
        data["files"] = files;
        return ToolResult::ok(json::serialize(data));
    }
};
