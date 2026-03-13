// ============================================================================
// agent_loop.cpp - Agent Loop with Tool System 实现
// ============================================================================

#include "agent_loop.hpp"
#include <iostream>

AgentLoop::AgentLoop() {
    registry_.register_tool(std::make_shared<CalculatorTool>());
    registry_.register_tool(std::make_shared<FileReadTool>());
    registry_.register_tool(std::make_shared<SystemInfoTool>());
}

std::string AgentLoop::process(const std::string& input) {
    try {
        auto json_input = json::parse(input);
        
        if (json_input.is_object()) {
            auto obj = json_input.as_object();
            std::string tool_name = std::string(obj.at("tool").as_string());
            json::object args = obj.contains("args") ? obj.at("args").as_object() : json::object{};
            
            std::cout << "[⚙️ Executing] " << tool_name << "\n";
            auto result = registry_.execute(tool_name, args);
            return json::serialize(result);
        }
    } catch (const std::exception& e) {
        json::object error;
        error["success"] = false;
        error["error"] = "Parse error: " + std::string(e.what());
        return json::serialize(error);
    }
    
    json::object help;
    help["message"] = "Send JSON: {\"tool\": \"name\", \"args\": {...}}";
    help["available_tools"] = registry_.get_all_schemas();
    return json::serialize(help);
}
