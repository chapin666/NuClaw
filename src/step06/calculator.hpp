// ============================================================================
// calculator.hpp - 计算器工具
// ============================================================================

#pragma once

#include "tool.hpp"
#include <cmath>

class CalculatorTool : public Tool {
public:
    ToolSchema get_schema() const override {
        return {
            "calculator",
            "执行数学计算",
            {
                {"expression", ParamType::STRING, "数学表达式，如 1+2*3", true},
                {"precision", ParamType::INTEGER, "结果精度（小数位）", false, 2}
            }
        };
    }
    
    json::value execute(const json::object& args) override {
        std::string expr(args.at("expression").as_string());
        double result = evaluate_simple(expr);
        
        int precision = args.contains("precision") 
            ? static_cast<int>(args.at("precision").as_int64()) : 2;
        
        json::object result_obj;
        result_obj["success"] = true;
        result_obj["expression"] = expr;
        result_obj["result"] = std::round(result * std::pow(10, precision)) 
                               / std::pow(10, precision);
        return result_obj;
    }

private:
    double evaluate_simple(const std::string& expr) {
        try { return std::stod(expr); } 
        catch (...) { return 0.0; }
    }
};
