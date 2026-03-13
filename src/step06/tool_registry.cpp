// ============================================================================
// tool_registry.cpp - 工具注册表实现
// ============================================================================

#include "tool_registry.hpp"
#include <iostream>

void ToolRegistry::register_tool(ToolPtr tool) {
    auto schema = tool->get_schema();
    tools_[schema.name] = tool;
    schemas_[schema.name] = schema;
    std::cout << "[+] Tool registered: " << schema.name << "\n";
}

ToolRegistry::ToolPtr ToolRegistry::get_tool(const std::string& name) {
    auto it = tools_.find(name);
    return (it != tools_.end()) ? it->second : nullptr;
}

json::array ToolRegistry::get_all_schemas() const {
    json::array arr;
    for (const auto& [name, schema] : schemas_) {
        arr.push_back(schema_to_json(schema));
    }
    return arr;
}

json::value ToolRegistry::execute(const std::string& tool_name, const json::object& args) {
    auto tool = get_tool(tool_name);
    if (!tool) {
        json::object error;
        error["success"] = false;
        error["error"] = "Tool not found: " + tool_name;
        return error;
    }
    return tool->execute_safe(args);
}

json::object ToolRegistry::schema_to_json(const ToolSchema& schema) {
    json::object obj;
    obj["name"] = schema.name;
    obj["description"] = schema.description;
    json::object params;
    for (const auto& p : schema.parameters) {
        json::object param_obj;
        param_obj["type"] = ParameterValidator::type_to_string(p.type);
        param_obj["description"] = p.description;
        param_obj["required"] = p.required;
        params[p.name] = param_obj;
    }
    obj["parameters"] = params;
    return obj;
}
