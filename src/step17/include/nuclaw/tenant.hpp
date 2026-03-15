// ============================================================================
// tenant.hpp - 租户上下文（Step 17 新增）
// ============================================================================
// 演进说明：
//   Step 16 的 Agent 只能服务一个用户群
//   Step 17 引入租户概念，实现数据隔离和定制化
// ============================================================================

#pragma once
#include <string>
#include <boost/json.hpp>

namespace json = boost::json;

namespace nuclaw {

// 租户配置
struct TenantConfig {
    std::string welcome_message = "您好，有什么可以帮您？";
    std::string fallback_message = "抱歉，这个问题我需要转接人工客服。";
    int session_timeout_seconds = 1800;  // 30分钟
    bool ai_enabled = true;
    bool human_escalation_enabled = true;
    json::object custom_settings;  // 租户自定义设置
};

// 租户上下文（Step 17 核心新增）
struct TenantContext {
    std::string tenant_id;       // 租户唯一标识
    std::string tenant_name;     // 租户显示名称
    std::string api_key_prefix;  // API Key 前缀（用于隔离）
    TenantConfig config;         // 租户配置
    json::object agent_persona;  // Agent 人设（名称、性格、领域）
    
    // 构造函数
    TenantContext() = default;
    TenantContext(const std::string& id, const std::string& name)
        : tenant_id(id), tenant_name(name), api_key_prefix(id.substr(0, 8)) {}
    
    // 获取 Agent 显示名称
    std::string get_agent_name() const {
        if (agent_persona.contains("name")) {
            return std::string(agent_persona.at("name").as_string());
        }
        return "客服助手";
    }
    
    // 获取 Agent 风格描述
    std::string get_agent_style() const {
        if (agent_persona.contains("style")) {
            return std::string(agent_persona.at("style").as_string());
        }
        return "专业、友好";
    }
    
    // 获取业务领域
    std::string get_domain() const {
        if (agent_persona.contains("domain")) {
            return std::string(agent_persona.at("domain").as_string());
        }
        return "通用";
    }
};

// 租户隔离的用户标识
struct TenantUserId {
    std::string tenant_id;
    std::string user_id;
    
    // 生成隔离后的全局用户ID
    std::string to_global_id() const {
        return tenant_id + ":" + user_id;
    }
    
    // 从全局ID解析
    static TenantUserId from_global(const std::string& global_id) {
        TenantUserId result;
        auto pos = global_id.find(':');
        if (pos != std::string::npos) {
            result.tenant_id = global_id.substr(0, pos);
            result.user_id = global_id.substr(pos + 1);
        } else {
            result.user_id = global_id;
        }
        return result;
    }
};

} // namespace nuclaw
