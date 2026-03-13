// ============================================================================
// file_tool.hpp - 文件读取工具（带安全检查）
// ============================================================================

#pragma once

#include "tool.hpp"
#include <fstream>
#include <algorithm>

class FileReadTool : public Tool {
public:
    ToolSchema get_schema() const override {
        return {
            "file_read",
            "读取文件内容（限制访问范围）",
            {
                {"path", ParamType::STRING, "文件路径（相对路径，禁止 ..）", true},
                {"limit", ParamType::INTEGER, "最大读取行数（默认100，最大1000）", false, 100}
            }
        };
    }
    
    json::value execute(const json::object& args) override {
        std::string path(args.at("path").as_string());
        
        // 安全检查
        if (path.find("..") != std::string::npos || 
            (!path.empty() && path[0] == '/')) {
            return make_error("Access denied: invalid path");
        }
        
        int limit = 100;
        if (args.contains("limit")) {
            limit = static_cast<int>(args.at("limit").as_int64());
            limit = std::min(limit, 1000);
        }
        
        std::ifstream file(path);
        if (!file.is_open()) {
            return make_error("Failed to open file: " + path);
        }
        
        std::string content;
        std::string line;
        int lines = 0;
        while (std::getline(file, line) && lines < limit) {
            content += line + "\n";
            lines++;
        }
        
        json::object result;
        result["success"] = true;
        result["path"] = path;
        result["content"] = content;
        result["lines"] = lines;
        return result;
    }

private:
    json::value make_error(const std::string& msg) {
        json::object error;
        error["success"] = false;
        error["error"] = msg;
        return error;
    }
};
