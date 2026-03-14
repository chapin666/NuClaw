// include/saas/simple_tenant.hpp
#pragma once
#include <string>
#include <map>
#include <vector>
#include <mutex>

namespace saas {

// 租户
struct Tenant {
    std::string tenant_id;
    std::string name;
    std::string api_key;
    
    int max_conversations = 1000;
    int used_conversations = 0;
    
    std::vector<std::string> knowledge_base;
    
    bool check_quota() const {
        return used_conversations < max_conversations;
    }
};

// 租户管理器
class TenantManager {
private:
    std::map<std::string, Tenant> tenants_;
    std::mutex mutex_;
    
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
    std::string generate_random_string(int length) {
        const std::string chars = "abcdefghijklmnopqrstuvwxyz0123456789";
        std::string result;
        for (int i = 0; i < length; ++i) {
            result += chars[rand() % chars.length()];
        }
        return result;
    }
};

// 聊天服务
class ChatService {
private:
    TenantManager& tenant_manager_;
    
public:
    ChatService(TenantManager& tm) : tenant_manager_(tm) {}
    
    struct ChatRequest {
        std::string api_key;
        std::string message;
        std::string session_id;
    };
    
    struct ChatResponse {
        bool success;
        std::string message;
        std::string error;
        int remaining_quota;
    };
    
    ChatResponse chat(const ChatRequest& req) {
        if (!tenant_manager_.validate_api_key(req.api_key)) {
            return {false, "", "Invalid API Key", 0};
        }
        
        Tenant* tenant = tenant_manager_.get_tenant(req.api_key);
        if (!tenant) {
            return {false, "", "Tenant not found", 0};
        }
        
        if (!tenant->check_quota()) {
            return {false, "", "Quota exceeded. Please upgrade your plan.", 0};
        }
        
        std::string reply = generate_reply(req.message, *tenant);
        
        tenant_manager_.record_usage(req.api_key);
        
        return {
            true,
            reply,
            "",
            tenant->max_conversations - tenant->used_conversations
        };
    }

private:
    std::string generate_reply(const std::string& msg, const Tenant& tenant) {
        for (const auto& knowledge : tenant.knowledge_base) {
            if (msg.find(knowledge.substr(0, 4)) != std::string::npos) {
                return "根据我们的资料：" + knowledge;
            }
        }
        
        return "您好！我是 " + tenant.name + " 的智能客服。\n"
               "有什么可以帮您的吗？";
    }
};

} // namespace saas