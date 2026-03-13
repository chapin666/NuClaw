// ============================================================================
// code_tool.hpp - 代码执行工具（沙箱）
// ============================================================================

#pragma once

#include "tool.hpp"
#include <cstdio>
#include <array>

class CodeExecuteTool : public Tool {
public:
    std::string get_name() const override { return "code_execute"; }
    std::string get_description() const override { 
        return "Execute Python code (sandboxed with blacklist)"; 
    }
    
    json::value execute(const json::object& args) override {
        std::string code(args.at("code").as_string());
        std::string language = args.contains("language") 
            ? std::string(args.at("language").as_string()) : "python";
        
        auto check = security_check(code, language);
        if (!check.first) {
            json::object error;
            error["success"] = false;
            error["error"] = "Security check failed: " + check.second;
            return error;
        }
        
        std::string cmd = "timeout 5 python3 -c '" + escape_shell(code) + "' 2>&1";
        
        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) {
            return make_error("Failed to execute code");
        }
        
        std::array<char, 4096> buffer;
        std::string output;
        while (fgets(buffer.data(), buffer.size(), pipe)) {
            output += buffer.data();
        }
        
        int status = pclose(pipe);
        
        json::object result;
        result["success"] = (status == 0);
        result["output"] = output;
        result["exit_code"] = status;
        return result;
    }

private:
    std::pair<bool, std::string> security_check(
        const std::string& code, 
        const std::string& language) {
        
        std::vector<std::string> blacklist = {
            "import os", "import sys", "__import__",
            "open(", "file(", "exec(", "eval(",
            "subprocess", "socket", "urllib",
            "rm -rf", "mkfs", "dd if"
        };
        
        for (const auto& bad : blacklist) {
            if (code.find(bad) != std::string::npos) {
                return {false, "Forbidden keyword: " + bad};
            }
        }
        
        if (code.length() > 10000) {
            return {false, "Code too long"};
        }
        
        return {true, ""};
    }
    
    std::string escape_shell(const std::string& s) {
        std::string result;
        for (char c : s) {
            if (c == '\'') result += "'\"'\"'";
            else result += c;
        }
        return result;
    }
    
    json::value make_error(const std::string& msg) {
        json::object error;
        error["success"] = false;
        error["error"] = msg;
        return error;
    }
};
