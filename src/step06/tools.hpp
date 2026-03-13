// ============================================================================
// tools.hpp - 具体工具实现
// ============================================================================

#pragma once

#include "tool.hpp"
#include <fstream>
#include <math>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <thread>

// ----------------------------------------------------------------------------
// 计算器工具
// ----------------------------------------------------------------------------
class CalculatorTool : public Tool {
public:
    ToolSchema get_schema() const override;
    json::value execute(const json::object& args) override;

private:
    double evaluate_simple(const std::string& expr);
};

// ----------------------------------------------------------------------------
// 文件读取工具（带安全检查）
// ----------------------------------------------------------------------------
class FileReadTool : public Tool {
public:
    ToolSchema get_schema() const override;
    json::value execute(const json::object& args) override;
};

// ----------------------------------------------------------------------------
// 系统信息工具
// ----------------------------------------------------------------------------
class SystemInfoTool : public Tool {
public:
    ToolSchema get_schema() const override;
    json::value execute(const json::object& args) override;
};
