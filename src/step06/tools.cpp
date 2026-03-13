// ============================================================================
// tools.cpp - 具体工具实现
// ============================================================================

#include "tools.hpp"

// ============================================================================
// 计算器工具
// ============================================================================

ToolSchema CalculatorTool::get_schema() const {
    return {
        "calculator",
        "执行数学计算",
        {
            {"expression", ParamType::STRING, "数学表达式，如 1+2*3", true},
            {"precision", ParamType::INTEGER, "结果精度（小数位）", false, 2}
        }
    };
}

json::value CalculatorTool::execute(const json::object& args) {
    std::string expr = args.at("expression").as_string();
    double result = evaluate_simple(expr);
    
    int precision = args.contains("precision") 
        ? static_cast<int>(args.at("precision").as_int64()) 
        : 2;
    
    json::object result_obj;
    result_obj["success"] = true;
    result_obj["expression"] = expr;
    result_obj["result"] = std::round(result * std::pow(10, precision)) / std::pow(10, precision);
    return result_obj;
}

double CalculatorTool::evaluate_simple(const std::string& expr) {
    try { return std::stod(expr); } catch (...) { return 0.0; }
}

// ============================================================================
// 文件读取工具
// ============================================================================

ToolSchema FileReadTool::get_schema() const {
    return {
        "file_read",
        "读取文件内容（限制访问范围）",
        {
            {"path", ParamType::STRING, "文件路径（相对路径，禁止 ..）", true},
            {"limit", ParamType::INTEGER, "最大读取行数（默认100，最大1000）", false, 100}
        }
    };
}

json::value FileReadTool::execute(const json::object& args) {
    std::string path = args.at("path").as_string();
    
    // 安全检查：防止路径遍历攻击
    if (path.find("..") != std::string::npos || path[0] == '/') {
        json::object error;
        error["success"] = false;
        error["error"] = "Access denied: invalid path";
        return error;
    }
    
    int limit = 100;
    if (args.contains("limit")) {
        limit = static_cast<int>(args.at("limit").as_int64());
        limit = std::min(limit, 1000);
    }
    
    std::ifstream file(path);
    json::object result;
    
    if (!file.is_open()) {
        result["success"] = false;
        result["error"] = "Failed to open file: " + path;
        return result;
    }
    
    std::string content;
    std::string line;
    int lines = 0;
    while (std::getline(file, line) && lines < limit) {
        content += line + "\n";
        lines++;
    }
    
    result["success"] = true;
    result["path"] = path;
    result["content"] = content;
    result["lines"] = lines;
    return result;
}

// ============================================================================
// 系统信息工具
// ============================================================================

ToolSchema SystemInfoTool::get_schema() const {
    return {
        "system_info",
        "获取系统信息",
        {{"type", ParamType::STRING, "信息类型: time/memory/cpu", true}}
    };
}

json::value SystemInfoTool::execute(const json::object& args) {
    std::string type = args.at("type").as_string();
    json::object result;
    result["success"] = true;
    result["type"] = type;
    
    if (type == "time") {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
        result["time"] = ss.str();
        result["timestamp"] = static_cast<int64_t>(time);
    }
    else if (type == "cpu") {
        result["cores"] = static_cast<int>(std::thread::hardware_concurrency());
    }
    else {
        result["success"] = false;
        result["error"] = "Unknown type: " + type;
    }
    
    return result;
}
