// ============================================================================
// agent_loop.hpp - Agent Loop with Tool System
// ============================================================================

#pragma once

#include "tool_registry.hpp"
#include "tools.hpp"
#include <boost/json.hpp>

namespace json = boost::json;

// ----------------------------------------------------------------------------
// Agent Loop with Tool System
// ----------------------------------------------------------------------------
class AgentLoop {
public:
    AgentLoop();
    std::string process(const std::string& input);

private:
    ToolRegistry registry_;
};
