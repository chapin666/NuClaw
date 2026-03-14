// ============================================================================
// step19_progressive.hpp - Step 19: SaaS 化 AI 助手平台
// ============================================================================
//
// 演进路径：
// Step 18: 多个 Agent ──→ Step 19: 多租户 Agent 池
//                添加租户隔离
// ============================================================================

#pragma once
#include "step18_progressive.hpp"
#include <map>
#include <mutex>
#include <memory>

namespace step19 {

using namespace step18;

// ============================================
// Step 19: 租户
// ============================================

struct Tenant {
    std::string tenant_id;
    std::string name;
    std::string api_key;
    
    // 资源配额
    int max_agents = 5;
    int max_conversations = 1000;
    int used_conversations = 0;
    
    bool check_quota() const {
        return used_conversations < max_conversations;
    }
};

// ============================================
// Step 19: 租户管理器
// ============================================

class TenantManager {
public:
    Tenant create_tenant(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        Tenant tenant;
        tenant.tenant_id = "tenant_" + generate_random_string(8);
        tenant.name = name;
        tenant.api_key = "sk_" + generate_random_string(32);
        
        tenants_[tenant.api_key] = tenant;
        return tenant;
    }
    
    bool validate_api_key(const std::string& api_key) {
        std::lock_guard<std::mutex> lock(mutex_);
        return tenants_.count(api_key) > 0;
    }
    
    Tenant* get_tenant(const std::string& api_key) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = tenants_.find(api_key);
        if (it != tenants_.end()) {
            return &it->second;
        }
        return nullptr;
    }
    
    void record_usage(const std::string& api_key) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = tenants_.find(api_key);
        if (it != tenants_.end()) {
            it->second.used_conversations++;
        }
    }
    
    std::vector<Tenant> list_all() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<Tenant> result;
        for (const auto& [key, tenant] : tenants_) {
            result.push_back(tenant);
        }
        return result;
    }

private:
    std::map<std::string, Tenant> tenants_;
    std::mutex mutex_;
    
    std::string generate_random_string(int length) {
        const std::string chars = "abcdefghijklmnopqrstuvwxyz0123456789";
        std::string result;
        for (int i = 0; i < length; ++i) {
            result += chars[rand() % chars.length()];
        }
        return result;
    }
};

// ============================================
// Step 19: 租户隔离的 Agent 运行时
// ============================================

class TenantAgentRuntime {
public:
    TenantAgentRuntime(
        const Tenant& tenant,
        TenantManager& tenant_manager
    ) : tenant_(tenant), tenant_manager_(tenant_manager) {
        // 为租户创建专属 Agent
        agent_ = std::make_unique<TravelAgent>(tenant.name + "_agent");
    }
    
    // 处理对话（带租户隔离）
    std::string chat(const std::string& user_id,
                     const std::string& user_name,
                     const std::string& message) {
        // 检查配额
        if (!tenant_.check_quota()) {
            return "❌ 配额已用完，请升级套餐";
        }
        
        // 记录用量
        tenant_manager_.record_usage(tenant_.api_key);
        
        // 处理消息（Agent 级别隔离）
        std::string response = agent_->handle_travel_query(
            user_id, user_name, message
        );
        
        return response;
    }
    
    void show_tenant_info() {
        std::cout << "租户: " << tenant_.name << "\n";
        std::cout << "  ID: " << tenant_.tenant_id << "\n";
        std::cout << "  用量: " << tenant_.used_conversations 
                  << "/" << tenant_.max_conversations << "\n";
    }

private:
    Tenant tenant_;
    TenantManager& tenant_manager_;
    std::unique_ptr<TravelAgent> agent_;
};

// ============================================
// Step 19: SaaS 平台
// ============================================

class SaaSPlatform {
public:
    std::shared_ptr<TenantAgentRuntime> get_or_create_runtime(
        const std::string& api_key
    ) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = runtimes_.find(api_key);
        if (it != runtimes_.end()) {
            return it->second;
        }
        
        // 创建新运行时
        Tenant* tenant = tenant_manager_.get_tenant(api_key);
        if (!tenant) {
            return nullptr;
        }
        
        auto runtime = std::make_shared<TenantAgentRuntime>(
            *tenant, tenant_manager_
        );
        runtimes_[api_key] = runtime;
        return runtime;
    }
    
    TenantManager& get_tenant_manager() { return tenant_manager_; }
    
    void show_platform_stats() {
        std::cout << "\n📊 平台统计:\n";
        auto tenants = tenant_manager_.list_all();
        std::cout << "  租户数: " << tenants.size() << "\n";
        
        int total_usage = 0;
        for (const auto& t : tenants) {
            total_usage += t.used_conversations;
        }
        std::cout << "  总对话数: " << total_usage << "\n";
    }

private:
    TenantManager tenant_manager_;
    std::map<std::string, std::shared_ptr<TenantAgentRuntime>> runtimes_;
    std::mutex mutex_;
};

} // namespace step19