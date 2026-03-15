// ============================================================================
// tool_registry.hpp - 工具注册表（单例模式）
// ============================================================================
// Step 9 核心：实现注册表模式，解耦工具注册与执行
// - 单例模式确保全局唯一实例
// - 支持动态注册和查询工具
// - 线程安全
// ============================================================================

#pragma once

#include "tool.hpp"
#include <memory>
#include <unordered_map>
#include <mutex>
#include <iostream>

class ToolRegistry {
public:
    // 获取单例实例（线程安全）
    static ToolRegistry& instance() {
        static ToolRegistry instance;
        return instance;
    }
    
    // 删除拷贝和赋值
    ToolRegistry(const ToolRegistry&) = delete;
    ToolRegistry& operator=(const ToolRegistry&) = delete;
    
    // 注册工具
    void register_tool(std::shared_ptr<Tool> tool) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string name = tool->get_name();
        tools_[name] = tool;
        std::cout << "[+] 注册工具: " << name << std::endl;
    }
    
    // 获取工具
    std::shared_ptr<Tool> get_tool(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = tools_.find(name);
        if (it != tools_.end()) {
            return it->second;
        }
        return nullptr;
    }
    
    // 检查工具是否存在
    bool has_tool(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return tools_.find(name) != tools_.end();
    }
    
    // 获取所有工具名称
    std::vector<std::string> get_tool_names() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> names;
        for (const auto& [name, _] : tools_) {
            names.push_back(name);
        }
        return names;
    }
    
    // 生成工具描述（给 LLM 使用）
    std::string get_tools_description() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string desc = "可用工具列表：\n\n";
        
        for (const auto& [name, tool] : tools_) {
            desc += "- " + name + ": " + tool->get_description() + "\n";
        }
        
        return desc;
    }
    
    // 获取已注册工具数量
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return tools_.size();
    }

private:
    ToolRegistry() = default;
    ~ToolRegistry() = default;
    
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<Tool>> tools_;
};
