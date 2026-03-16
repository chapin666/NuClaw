#pragma once

#include <string>
#include <map>
#include <memory>

namespace smartsupport::services::tenant {

class TenantDatabase;

struct Tenant {
    std::string id;
    std::string name;
    std::string plan;  // free/pro/enterprise
    std::chrono::system_clock::time_point created_at;
    std::map<std::string, std::string> config;
};

struct Member {
    std::string id;
    std::string name;
    std::string role;  // admin/agent/viewer
};

class TenantService {
public:
    TenantService(std::shared_ptr<TenantDatabase> db);
    
    Tenant create_tenant(const std::string& name, const std::string& plan);
    Tenant get_tenant(const std::string& tenant_id);
    void update_tenant(const Tenant& tenant);
    void delete_tenant(const std::string& tenant_id);
    
    std::string get_config(const std::string& tenant_id, const std::string& key,
                           const std::string& default_value = "");
    void set_config(const std::string& tenant_id, const std::string& key,
                    const std::string& value);
    
    void add_member(const std::string& tenant_id, const std::string& user_id,
                    const std::string& role);
    void remove_member(const std::string& tenant_id, const std::string& user_id);
    std::vector<Member> list_members(const std::string& tenant_id);

private:
    std::shared_ptr<TenantDatabase> db_;
};

} // namespace smartsupport::services::tenant
