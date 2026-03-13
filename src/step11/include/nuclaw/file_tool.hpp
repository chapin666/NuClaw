// ============================================================================
// file_tool.hpp - Step 9: 改造为继承 Tool 基类（从 Step 8 演进）
// ============================================================================

#pragma once

#include "tool.hpp"
#include "sandbox.hpp"
#include <filesystem>

namespace fs = std::filesystem;

class FileTool : public Tool {
public:
    std::string get_name() const override { return "file"; }
    
    std::string get_description() const override {
        return "文件操作：读/列表（带路径遍历防护）";
    }
    
    ToolResult execute(const std::string& path) const override {
        // Step 8 原有：安全检查
        if (!Sandbox::is_safe_path(path)) {
            return ToolResult::fail("路径安全检查失败：禁止 .. 或绝对路径");
        }
        
        try {
            json::object data;
            data["path"] = path;
            
            if (fs::exists(path) && fs::is_directory(path)) {
                json::array files;
                for (const auto& entry : fs::directory_iterator(path)) {
                    json::object file_info;
                    file_info["name"] = entry.path().filename().string();
                    file_info["is_directory"] = entry.is_directory();
                    files.push_back(file_info);
                }
                data["files"] = files;
            }
            
            return ToolResult::ok(json::serialize(data));
        } catch (const std::exception& e) {
            return ToolResult::fail(std::string("文件操作失败: ") + e.what());
        }
    }
};
