// ============================================================================
// tool.hpp - Tool 系统基础定义
// ============================================================================
//
// 核心组件：
//   +-------------------------------------------------------------+
//   |  ParamType                                                   |
//   |  Parameter -- 描述工具参数（名称、类型、必填）               |
//   |  ToolSchema -- 工具的定义（元数据）                          |
//   |       |                                                      |
//   |       v                                                      |
//   |  ParameterValidator -- 参数类型校验                          |
//   |       |                                                      |
//   |       v                                                      |
//   |  Tool（抽象基类）-- execute()                                 |
//   |       |                                                      |
//   |       +-- CalculatorTool -- 数学计算                         |
//   |       +-- FileReadTool -- 文件读取（带安全检查）             |
//   |       +-- SystemInfoTool -- 系统信息                         |
//   +-------------------------------------------------------------+
//
// 关键概念：
//   - Schema：工具的"说明书"，让 LLM 知道如何使用
//   - 校验：在工具执行前验证参数，提前发现错误
//   - 注册表：运行时动态管理工具，支持扩展
// ============================================================================

#pragma once

#include <boost/json.hpp>
#include <string>
#include <vector>

namespace json = boost::json;

// ----------------------------------------------------------------------------
// 参数类型枚举
// ----------------------------------------------------------------------------
enum class ParamType {
    STRING,     // JSON 字符串
    INTEGER,    // 整数（int64）
    NUMBER,     // 数字（整数或浮点）
    BOOLEAN,    // true/false
    ARRAY,      // JSON 数组
    OBJECT      // JSON 对象
};

// ----------------------------------------------------------------------------
// 参数定义
// ----------------------------------------------------------------------------
struct Parameter {
    std::string name;           // 参数名（如 "expression"）
    ParamType type;             // 参数类型
    std::string description;    // 描述（给 LLM 看的）
    bool required = true;       // 是否必填
    json::value default_value;  // 可选参数的默认值
    
    Parameter(std::string n, ParamType t, std::string d, bool r = true, json::value def = {})
        : name(std::move(n)), type(t), description(std::move(d)), required(r), default_value(def) {}
};

// ----------------------------------------------------------------------------
// Tool Schema - 工具的定义
// ----------------------------------------------------------------------------
struct ToolSchema {
    std::string name;                   // 工具名（唯一标识）
    std::string description;            // 工具功能描述
    std::vector<Parameter> parameters;  // 参数列表
};

// ----------------------------------------------------------------------------
// 参数校验器
// ----------------------------------------------------------------------------
class ParameterValidator {
public:
    static bool validate_type(const json::value& value, ParamType expected);
    static std::string type_to_string(ParamType type);
    static std::vector<std::string> validate(
        const json::object& args,
        const std::vector<Parameter>& schema);
};

// ----------------------------------------------------------------------------
// Tool 抽象基类
// ----------------------------------------------------------------------------
class Tool {
public:
    virtual ~Tool() = default;
    virtual ToolSchema get_schema() const = 0;
    virtual json::value execute(const json::object& args) = 0;
    json::value execute_safe(const json::object& args);
};
