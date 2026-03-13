// ============================================================================
// calc_tool.hpp - 计算器工具（Step 9 改造版）
// ============================================================================

#pragma once

#include "tool.hpp"
#include <sstream>

class CalcTool : public Tool {
public:
    std::string get_name() const override { return "calculate"; }
    
    std::string get_description() const override {
        return "数学计算，支持 + - * /，如：1+2、10*5";
    }
    
    ToolResult execute(const std::string& expression) const override {
        try {
            double result = evaluate(expression);
            
            json::object data;
            data["expression"] = expression;
            data["result"] = result;
            
            return ToolResult::ok(json::serialize(data));
        } catch (const std::exception& e) {
            return ToolResult::fail(std::string("计算错误: ") + e.what());
        }
    }

private:
    double evaluate(const std::string& expr) const {
        // 简化实现：解析 "a op b" 格式
        std::istringstream iss(expr);
        double a, b;
        char op;
        
        iss >> a >> op >> b;
        
        switch (op) {
            case '+': return a + b;
            case '-': return a - b;
            case '*': return a * b;
            case '/': 
                if (b == 0) throw std::runtime_error("除数不能为零");
                return a / b;
            default: throw std::runtime_error("不支持的操作符");
        }
    }
};
