// ============================================================================
// calc_tool.hpp - 计算器工具
// ============================================================================

#pragma once

#include "tool.hpp"
#include <boost/json.hpp>
#include <sstream>
#include <cmath>

namespace json = boost::json;

class CalcTool {
public:
    static std::string get_name() { return "calculate"; }
    
    static std::string get_description() {
        return "执行数学计算，如：1 + 2、10 * 5";
    }
    
    static ToolResult execute(const std::string& expression) {
        try {
            // 简化版计算器，解析 "a op b" 格式
            double a, b;
            char op;
            
            std::string expr = normalize_expression(expression);
            std::istringstream iss(expr);
            iss >> a >> op >> b;
            
            double result = 0;
            switch (op) {
                case '+': result = a + b; break;
                case '-': result = a - b; break;
                case '*': result = a * b; break;
                case '/': 
                    if (b == 0) {
                        return ToolResult::fail("除数不能为零");
                    }
                    result = a / b; 
                    break;
                default:
                    return ToolResult::fail("不支持的操作符: " + std::string(1, op));
            }
            
            json::object data;
            data["expression"] = expression;
            data["result"] = result;
            data["operator"] = std::string(1, op);
            
            return ToolResult::ok(json::serialize(data));
            
        } catch (const std::exception& e) {
            return ToolResult::fail("计算错误: " + std::string(e.what()));
        }
    }

private:
    static std::string normalize_expression(const std::string& input) {
        std::string expr;
        for (char c : input) {
            if (std::isdigit(c) || c == '+' || c == '-' || c == '*' || c == '/' || c == '.' || c == ' ') {
                expr += c;
            }
        }
        
        // 替换中文运算符
        size_t pos;
        while ((pos = expr.find("加")) != std::string::npos) expr.replace(pos, 3, "+");
        while ((pos = expr.find("减")) != std::string::npos) expr.replace(pos, 3, "-");
        while ((pos = expr.find("乘")) != std::string::npos) expr.replace(pos, 3, "*");
        while ((pos = expr.find("除")) != std::string::npos) expr.replace(pos, 3, "/");
        while ((pos = expr.find("等于")) != std::string::npos) expr.replace(pos, 6, "");
        
        return expr.empty() ? "1 + 1" : expr;
    }
};
