// ============================================================================
// test_tool_registry.cpp
// ============================================================================

#include <gtest/gtest.h>
#include "nuclaw/tool_registry.hpp"
#include "nuclaw/weather_tool.hpp"
#include "nuclaw/time_tool.hpp"

TEST(ToolRegistryTest, SingletonInstance) {
    auto& registry1 = ToolRegistry::instance();
    auto& registry2 = ToolRegistry::instance();
    EXPECT_EQ(&registry1, &registry2);
}

TEST(ToolRegistryTest, RegisterAndGetTool) {
    auto& registry = ToolRegistry::instance();
    
    auto weather = std::make_shared<WeatherTool>();
    registry.register_tool(weather);
    
    auto retrieved = registry.get_tool("get_weather");
    EXPECT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->get_name(), "get_weather");
}

TEST(ToolRegistryTest, GetUnknownToolReturnsNull) {
    auto& registry = ToolRegistry::instance();
    auto tool = registry.get_tool("unknown_tool");
    EXPECT_EQ(tool, nullptr);
}
