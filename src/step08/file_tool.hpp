// ============================================================================
// file_tool.hpp - 文件操作工具（带沙箱）
// ============================================================================

#pragma once

#include "tool.hpp"
#include "sandbox.hpp"
#include <fstream>
#include <sstream>

class FileTool : public Tool {
public:
    FileTool() {
        sandbox_.allow_path(fs::current_path() / "data");
    }
    
    std::string get_name() const override { return "file"; }
    std::string get_description() const override { 
        return "File operations: read/write/list (sandboxed)"; 
    }
    
    json::value execute(const json::object& args) override {
        std::string operation(args.at("operation").as_string());
        
        if (operation == "read") return file_read(args);
        if (operation == "write") return file_write(args);
        if (operation == "list") return file_list(args);
        
        return make_error("Unknown operation: " + operation);
    }

private:
    Sandbox sandbox_;
    
    json::value file_read(const json::object& args) {
        std::string path(args.at("path").as_string());
        
        if (!Sandbox::is_safe_path(path)) {
            return make_error("Path contains unsafe characters (../ or absolute path)");
        }
        if (!sandbox_.is_allowed(path)) {
            return make_error("Access denied: path outside allowed directory");
        }
        
        std::ifstream file(path);
        if (!file.is_open()) {
            return make_error("Failed to open file: " + path);
        }
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        
        json::object result;
        result["success"] = true;
        result["content"] = buffer.str();
        result["size"] = static_cast<int64_t>(buffer.str().length());
        return result;
    }
    
    json::value file_write(const json::object& args) {
        std::string path(args.at("path").as_string());
        std::string content(args.at("content").as_string());
        
        if (!Sandbox::is_safe_path(path) || !sandbox_.is_allowed(path)) {
            return make_error("Access denied");
        }
        
        std::ofstream file(path);
        if (!file.is_open()) {
            return make_error("Failed to create file");
        }
        
        file << content;
        
        json::object result;
        result["success"] = true;
        result["bytes_written"] = static_cast<int64_t>(content.length());
        return result;
    }
    
    json::value file_list(const json::object& args) {
        std::string path = args.contains("path") 
            ? std::string(args.at("path").as_string()) : ".";
        
        if (!Sandbox::is_safe_path(path) || !sandbox_.is_allowed(path)) {
            return make_error("Access denied");
        }
        
        json::object result;
        result["success"] = true;
        json::array files;
        
        for (const auto& entry : fs::directory_iterator(path)) {
            json::object file_info;
            file_info["name"] = entry.path().filename().string();
            file_info["is_directory"] = entry.is_directory();
            file_info["size"] = entry.is_regular_file() 
                ? static_cast<int64_t>(entry.file_size()) : 0;
            files.push_back(file_info);
        }
        
        result["files"] = files;
        return result;
    }
    
    json::value make_error(const std::string& msg) {
        json::object error;
        error["success"] = false;
        error["error"] = msg;
        return error;
    }
};
