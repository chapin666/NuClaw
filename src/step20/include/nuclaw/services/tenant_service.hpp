// ============================================================================
// tenant_service.hpp - Step 19: 租户管理服务（独立版本）
// ============================================================================
// 注意：此版本不依赖 tenant.hpp，避免 boost::json 链接问题

#pragma once
#include <string>
#include <memory>
#include <map>
#include <vector>
#include <mutex>
#include <chrono>
#include <iostream>

namespace nuclaw {

// 租户套餐类型
enum class TenantPlan {
    FREE, BASIC, PRO, ENTERPRISE
};

// 租户套餐配额
struct TenantQuota {
    int max_concurrent_sessions = 100;
    int max_messages_per_month = 10000;
    int max_knowledge_documents = 50;
    int max_storage_mb = 100;
    int max_human_agents = 5;
};

// 扩展的租户信息（包含管理元数据）
struct TenantInfo {
    std::string tenant_id;
    std::string tenant_name;
    TenantPlan plan;
    TenantQuota quota;
    std::string admin_email;
    bool is_active = true;
    bool is_suspended = false;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point expires_at;
    
    TenantInfo() {
        created_at = std::chrono::system_clock::now();
        expires_at = created_at + std::chrono::hours(24 * 30);
    }
};

// TenantService: 租户管理 API（Step 19 新增）
class TenantService {
public:
    TenantService() = default;
    
    // 创建租户
    std::string create_tenant(const std::string& name,
                               const std::string& admin_email,
                               TenantPlan plan = TenantPlan::FREE) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::string tenant_id = "tenant_" + std::to_string(++tenant_counter_);
        
        auto info = std::make_unique<TenantInfo>();
        info->tenant_id = tenant_id;
        info->tenant_name = name;
        info->admin_email = admin_email;
        info->plan = plan;
        info->quota = get_quota_for_plan(plan);
        
        tenants_[tenant_id] = std::move(info);
        
        std::cout << "[TenantService] 创建租户: " << name 
                  << " (ID: " << tenant_id << ")\n";
        
        return tenant_id;
    }
    
    // 获取租户配额
    TenantQuota get_quota(const std::string& tenant_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = tenants_.find(tenant_id);
        if (it != tenants_.end()) {
            return it->second->quota;
        }
        return TenantQuota{};
    }
    
    // 打印统计
    void print_stats() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        int free = 0, basic = 0, pro = 0, enterprise = 0;
        for (const auto& [id, info] : tenants_) {
            switch (info->plan) {
                case TenantPlan::FREE: free++; break;
                case TenantPlan::BASIC: basic++; break;
                case TenantPlan::PRO: pro++; break;
                case TenantPlan::ENTERPRISE: enterprise++; break;
            }
        }
        
        std::cout << "\n[TenantService 统计]\n";
        std::cout << "  总租户: " << tenants_.size() << "\n";
        std::cout << "  套餐分布: 免费=" << free << " 基础=" << basic 
                  << " 专业=" << pro << " 企业=" << enterprise << "\n";
    }

private:
    TenantQuota get_quota_for_plan(TenantPlan plan) {
        TenantQuota q;
        switch (plan) {
            case TenantPlan::FREE:
                q.max_concurrent_sessions = 10;
                q.max_messages_per_month = 1000;
                q.max_knowledge_documents = 5;
                q.max_storage_mb = 10;
                q.max_human_agents = 0;
                break;
            case TenantPlan::BASIC:
                q.max_concurrent_sessions = 50;
                q.max_messages_per_month = 5000;
                q.max_knowledge_documents = 20;
                q.max_storage_mb = 50;
                q.max_human_agents = 2;
                break;
            case TenantPlan::PRO:
                q.max_concurrent_sessions = 200;
                q.max_messages_per_month = 20000;
                q.max_knowledge_documents = 100;
                q.max_storage_mb = 500;
                q.max_human_agents = 10;
                break;
            case TenantPlan::ENTERPRISE:
                q.max_concurrent_sessions = 1000;
                q.max_messages_per_month = 100000;
                q.max_knowledge_documents = 1000;
                q.max_storage_mb = 5000;
                q.max_human_agents = 50;
                break;
        }
        return q;
    }
    
    std::map<std::string, std::unique_ptr<TenantInfo>> tenants_;
    mutable std::mutex mutex_;
    int tenant_counter_ = 0;
};

} // namespace nuclaw
