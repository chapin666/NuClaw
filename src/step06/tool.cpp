// ============================================================================
// tool.cpp - Tool 系统基础实现
// ============================================================================

#include "tool.hpp"

// ----------------------------------------------------------------------------
// 参数校验器实现
// ----------------------------------------------------------------------------
bool ParameterValidator::validate_type(const json::value& value, ParamType expected) {
    switch (expected) {
        case ParamType::STRING:   return value.is_string();
        case ParamType::INTEGER:  return value.is_int64();
        case ParamType::NUMBER:   return value.is_number();
        case ParamType::BOOLEAN:  return value.is_bool();
        case ParamType::ARRAY:    return value.is_array();
        case ParamType::OBJECT:   return value.is_object();
        default: return false;
    }
}

std::string ParameterValidator::type_to_string(ParamType type) {
    switch (type) {
        case ParamType::STRING:   return "string";
        case ParamType::INTEGER:  return "integer";
        case ParamType::NUMBER:   return "number";
        case ParamType::BOOLEAN:  return "boolean";
        case ParamType::ARRAY:    return "array";
        case ParamType::OBJECT:   return "object";
        default: return "unknown";
    }
}

std::vector<std::string> ParameterValidator::validate(
    const json::object& args,
    const std::vector<Parameter>& schema) {
    
    std::vector<std::string> errors;
    
    // 1. 检查必需参数是否缺失
    for (const auto& param : schema) {
        auto it = args.find(param.name);
        if (param.required && it == args.end()) {
            errors.push_back("Missing required parameter: " + param.name);
            continue;
        }
        
        // 2. 检查参数类型
        if (it != args.end() && !validate_type(it->value(), param.type)) {
            errors.push_back("Parameter '" + param.name + "' should be " +
                           type_to_string(param.type) + ", got " +
                           json::serialize(it->value()));
        }
    }
    
    // 3. 检查是否有未知参数
    for (const auto& kv : args) {
        bool found = false;
        for (const auto& param : schema) {
            if (param.name == kv.key()) { found = true; break; }
        }
        if (!found) {
            errors.push_back("Unknown parameter: " + std::string(kv.key()));
        }
    }
    
    return errors;
}

// ----------------------------------------------------------------------------
// Tool 基类实现
// ----------------------------------------------------------------------------
json::value Tool::execute_safe(const json::object& args) {
    auto schema = get_schema();
    auto errors = ParameterValidator::validate(args, schema.parameters);
    
    if (!errors.empty()) {
        json::object error_result;
        error_result["success"] = false;
        error_result["error"] = "Validation failed";
        json::array error_list;
        for (const auto& e : errors) error_list.push_back(json::value(e));
        error_result["details"] = error_list;
        return error_result;
    }
    
    return execute(args);
}
