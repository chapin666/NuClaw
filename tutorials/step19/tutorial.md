# Step 19: SaaS 化 AI 助手平台

> 目标：构建支持多租户、多用户的生产级 AI 助手平台
> 
> 难度：⭐⭐⭐⭐⭐ (架构实战)
> 
> 代码量：约 1800 行

## 项目简介

**SaaS 化 AI 助手平台**是 NuClaw 的最终章。你将把前面所有知识整合，构建一个可以服务多个企业、多个用户的生产级平台。

### 核心能力

- 🏢 **多租户隔离**：每个企业独立数据、独立配置
- 👥 **多用户管理**：用户权限、角色、配额
- 🔐 **安全认证**：OAuth2/JWT、API Key 管理
- 📊 **运营分析**：租户级、用户级使用统计
- 💰 **计费系统**：按量计费、套餐管理

## 业务场景

假设你在构建一个「智能客服 SaaS 平台」，服务企业客户：

```
租户 A（电商公司）
├── 管理员：张总
├── 客服组：小李、小王
├── 知识库：电商产品 FAQ
└── 接入渠道：官网、App、微信公众号

租户 B（教育机构）
├── 管理员：李老师
├── 教师组：张老师、王老师
├── 知识库：课程资料、招生信息
└── 接入渠道：官网、钉钉群

平台运营方（你）
├── 监控所有租户健康度
├── 查看收入报表
└── 管理套餐和价格
```

## 系统架构

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        SaaS AI 助手平台                                  │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                      API Gateway (Kong/Nginx)                    │   │
│  │  • 路由    • 限流    • 认证    • 租户识别                        │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                    │                                    │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                      租户管理器 (Tenant Manager)                 │   │
│  │  • 租户生命周期    • 配置隔离    • 资源配额                       │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                    │                                    │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                      用户认证中心 (Auth Center)                  │   │
│  │  • OAuth2/JWT    • 角色权限    • API Key 管理                    │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                    │                                    │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  ┌─────────────┐│
│  │   租户 A     │  │   租户 B     │  │   租户 C     │  │    ...      ││
│  │  Agent 实例  │  │  Agent 实例  │  │  Agent 实例  │  │             ││
│  │              │  │              │  │              │  │             ││
│  │ • 独立知识库 │  │ • 独立知识库 │  │ • 独立知识库 │  │             ││
│  │ • 独立记忆   │  │ • 独立记忆   │  │ • 独立记忆   │  │             ││
│  │ • 独立用户   │  │ • 独立用户   │  │ • 独立用户   │  │             ││
│  └──────────────┘  └──────────────┘  └──────────────┘  └─────────────┘│
│                                                                         │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                      共享基础设施                                │   │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐           │   │
│  │  │  向量DB  │ │  关系DB  │ │  消息队列 │ │  对象存储 │           │   │
│  │  │(分租户) │ │(分租户) │ │(分租户) │ │(分租户) │           │   │
│  │  └──────────┘ └──────────┘ └──────────┘ └──────────┘           │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                         │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                      运营平台 (Admin Portal)                     │   │
│  │  • 租户管理    • 计费报表    • 系统监控    • 全局配置            │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

## 核心实现

### 1. 多租户数据隔离

```cpp
// tenant.hpp
#pragma once
#include <string>
#include <map>
#include <memory>
#include <nuclaw/agent_state.hpp>

namespace saas {

// 租户信息
struct Tenant {
    std::string tenant_id;          // 租户唯一标识
    std::string name;               // 租户名称
    std::string plan;               // 套餐：basic/pro/enterprise
    
    // 资源配额
    struct Quota {
        int max_users = 10;         // 最大用户数
        int max_conversations = 1000; // 月对话数上限
        int storage_mb = 100;       // 存储空间
        int api_calls_per_min = 60; // API 限流
    } quota;
    
    // 租户配置
    nlohmann::json config;
    
    // 状态
    bool is_active = true;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point expires_at;
};

// 租户上下文（贯穿请求生命周期）
class TenantContext {
public:
    std::string tenant_id;
    std::string user_id;
    std::vector<std::string> roles;  // ["admin", "agent_operator"]
    
    // 检查权限
    bool has_permission(const std::string& permission) const {
        // 根据角色检查权限
        static const std::map<std::string, std::vector<std::string>> role_permissions = {
            {"admin", {"all"}},
            {"operator", {"chat", "view_stats"}},
            {"viewer", {"view_stats"}}
        };
        
        for (const auto& role : roles) {
            auto it = role_permissions.find(role);
            if (it != role_permissions.end()) {
                if (std::find(it->second.begin(), it->second.end(), "all") != it->second.end() ||
                    std::find(it->second.begin(), it->second.end(), permission) != it->second.end()) {
                    return true;
                }
            }
        }
        return false;
    }
};

// 租户隔离的数据访问层
class TenantDataStore {
public:
    // 所有数据库表名都带租户前缀
    std::string get_table_name(const std::string& tenant_id, 
                               const std::string& table) {
        return fmt::format("tenant_{}_{}", tenant_id, table);
    }
    
    // 存储记忆（自动隔离）
    async::Task<void> store_memory(
        const TenantContext& ctx,
        const nuclaw::Memory& memory) {
        
        auto table = get_table_name(ctx.tenant_id, "memories");
        // 存储到 tenant_{id}_memories 表
        co_await db_->insert(table, memory.to_json());
    }
    
    // 检索记忆（自动过滤租户）
    async::Task<std::vector<nuclaw::Memory>> retrieve_memories(
        const TenantContext& ctx,
        const std::string& query,
        size_t limit = 5) {
        
        auto table = get_table_name(ctx.tenant_id, "memories");
        // 只搜索该租户的记忆
        co_return co_await vector_db_->search(table, query, limit);
    }

private:
    std::shared_ptr<Database> db_;
    std::shared_ptr<VectorDatabase> vector_db_;
};

} // namespace saas
```

### 2. 租户级 Agent 实例管理

```cpp
// tenant_agent_manager.hpp
#pragma once
#include <tenant.hpp>
#include <nuclaw/chat_engine.hpp>

namespace saas {

// 每个租户有独立的 Agent 配置和状态
class TenantAgentInstance {
public:
    TenantAgentInstance(const Tenant& tenant)
        : tenant_(tenant) {
        
        // 根据租户配置初始化 Agent
        initialize_from_config(tenant.config);
    }
    
    async::Task<std::string> chat(
        const TenantContext& ctx,
        const std::string& message) {
        
        // 检查配额
        if (!check_quota(ctx)) {
            co_return "已达到本月对话上限，请联系管理员升级套餐。";
        }
        
        // 记录使用
        record_usage(ctx);
        
        // 使用租户隔离的数据存储
        auto memories = co_await data_store_->retrieve_memories(ctx, message, 5);
        
        // 调用 ChatEngine（注入租户上下文）
        auto response = co_await chat_engine_->process(message, memories);
        
        co_return response;
    }

private:
    bool check_quota(const TenantContext& ctx) {
        auto usage = usage_tracker_.get_monthly_usage(tenant_.tenant_id);
        return usage.conversation_count < tenant_.quota.max_conversations;
    }
    
    void record_usage(const TenantContext& ctx) {
        usage_tracker_.record(tenant_.tenant_id, ctx.user_id, {
            .timestamp = std::chrono::system_clock::now(),
            .type = "chat",
            .tokens_used = 0  // 实际需要计算
        });
    }
    
    Tenant tenant_;
    std::unique_ptr<nuclaw::ChatEngine> chat_engine_;
    std::unique_ptr<TenantDataStore> data_store_;
};

// Agent 实例池（管理所有租户的 Agent）
class TenantAgentPool {
public:
    // 获取或创建租户的 Agent 实例
    std::shared_ptr<TenantAgentInstance> get_or_create(
        const std::string& tenant_id) {
        
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = instances_.find(tenant_id);
        if (it != instances_.end()) {
            return it->second;
        }
        lock.unlock();
        
        // 创建新实例
        std::unique_lock<std::shared_mutex> unique_lock(mutex_);
        auto tenant = tenant_manager_.get(tenant_id);
        auto instance = std::make_shared<TenantAgentInstance>(tenant);
        instances_[tenant_id] = instance;
        return instance;
    }
    
    // 热更新租户配置
    void reload_tenant_config(const std::string& tenant_id) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        instances_.erase(tenant_id);  // 下次访问时重新创建
    }

private:
    std::shared_mutex mutex_;
    std::map<std::string, std::shared_ptr<TenantAgentInstance>> instances_;
    TenantManager tenant_manager_;
};

} // namespace saas
```

### 3. API 网关与认证

```cpp
// api_gateway.hpp
#pragma once
#include <tenant.hpp>
#include <cpp-httplib/httplib.h>

namespace saas {

class APIGateway {
public:
    APIGateway(std::shared_ptr<TenantAgentPool> pool)
        : agent_pool_(pool) {}
    
    void start(int port = 8080) {
        using namespace httplib;
        
        // 认证中间件
        server_.set_base_handler([this](const Request& req, Response& res) {
            // 从 Header 提取租户信息
            auto tenant_id = req.get_header_value("X-Tenant-ID");
            auto api_key = req.get_header_value("X-API-Key");
            
            // 验证 API Key
            if (!authenticate(tenant_id, api_key)) {
                res.status = 401;
                res.set_content(R"({"error": "Unauthorized"})", "application/json");
                return false;
            }
            
            // 限流检查
            if (!rate_limit_check(tenant_id)) {
                res.status = 429;
                res.set_content(R"({"error": "Rate limit exceeded"})", "application/json");
                return false;
            }
            
            return true;
        });
        
        // 对话接口
        server_.Post("/api/v1/chat", [this](const Request& req, Response& res) {
            auto tenant_id = req.get_header_value("X-Tenant-ID");
            auto user_id = req.get_header_value("X-User-ID");
            
            auto body = nlohmann::json::parse(req.body);
            auto message = body.value("message", "");
            
            // 获取租户 Agent 实例
            auto agent = agent_pool_->get_or_create(tenant_id);
            
            // 构建租户上下文
            TenantContext ctx{
                .tenant_id = tenant_id,
                .user_id = user_id,
                .roles = get_user_roles(tenant_id, user_id)
            };
            
            // 调用 Agent
            auto response = agent->chat(ctx, message).get();
            
            nlohmann::json result = {
                {"response", response},
                {"tenant_id", tenant_id},
                {"timestamp", std::chrono::system_clock::now()}
            };
            
            res.set_content(result.dump(), "application/json");
        });
        
        // 管理接口（仅管理员）
        server_.Get("/api/v1/admin/tenants", [this](const Request& req, Response& res) {
            // 验证平台管理员权限
            if (!is_platform_admin(req)) {
                res.status = 403;
                return;
            }
            
            auto tenants = list_all_tenants();
            res.set_content(tenants.dump(), "application/json");
        });
        
        server_.listen("0.0.0.0", port);
    }

private:
    bool authenticate(const std::string& tenant_id, 
                     const std::string& api_key) {
        // 验证 API Key 是否属于该租户
        return api_key_manager_.verify(tenant_id, api_key);
    }
    
    bool rate_limit_check(const std::string& tenant_id) {
        auto tenant = tenant_manager_.get(tenant_id);
        auto current = rate_limiter_.get_current_usage(tenant_id);
        return current < tenant.quota.api_calls_per_min;
    }
    
    httplib::Server server_;
    std::shared_ptr<TenantAgentPool> agent_pool_;
    APIKeyManager api_key_manager_;
    RateLimiter rate_limiter_;
    TenantManager tenant_manager_;
};

} // namespace saas
```

### 4. 计费与报表

```cpp
// billing.hpp
#pragma once
#include <tenant.hpp>

namespace saas {

// 计费记录
struct BillingRecord {
    std::string tenant_id;
    std::string user_id;
    std::string event_type;     // "chat", "storage", "api_call"
    int quantity;               // 数量（token 数、存储字节等）
    double cost;                // 费用
    std::chrono::system_clock::time_point timestamp;
};

class BillingSystem {
public:
    // 记录使用（实时计费）
    void record_usage(const BillingRecord& record) {
        // 写入计费日志
        billing_log_.insert(record);
        
        // 更新实时余额（预付费模式）
        if (is_prepaid(record.tenant_id)) {
            deduct_balance(record.tenant_id, record.cost);
        }
    }
    
    // 生成月账单
    async::Task<nlohmann::json> generate_monthly_bill(
        const std::string& tenant_id,
        int year, int month) {
        
        auto records = co_await billing_log_.query(
            tenant_id, year, month
        );
        
        nlohmann::json bill = {
            {"tenant_id", tenant_id},
            {"period", fmt::format("{}-{}", year, month)},
            {"items", nlohmann::json::array()},
            {"total", 0.0}
        };
        
        // 按事件类型汇总
        std::map<std::string, std::pair<int, double>> summary;
        for (const auto& r : records) {
            summary[r.event_type].first += r.quantity;
            summary[r.event_type].second += r.cost;
        }
        
        double total = 0;
        for (const auto& [type, data] : summary) {
            bill["items"].push_back({
                {"type", type},
                {"quantity", data.first},
                {"cost", data.second}
            });
            total += data.second;
        }
        
        bill["total"] = total;
        co_return bill;
    }
    
    // 运营报表（平台级）
    async::Task<nlohmann::json> generate_platform_report(
        int year, int month) {
        
        auto stats = co_await billing_log_.aggregate_by_tenant(
            year, month
        );
        
        nlohmann::json report = {
            {"period", fmt::format("{}-{}", year, month)},
            {"total_revenue", 0.0},
            {"total_tenants", stats.size()},
            {"top_tenants", nlohmann::json::array()}
        };
        
        // 计算总收入
        double total_revenue = 0;
        for (const auto& [tenant_id, revenue] : stats) {
            total_revenue += revenue;
        }
        report["total_revenue"] = total_revenue;
        
        // Top 10 租户
        std::vector<std::pair<std::string, double>> sorted(stats.begin(), stats.end());
        std::sort(sorted.begin(), sorted.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });
        
        for (size_t i = 0; i < std::min(size_t(10), sorted.size()); ++i) {
            report["top_tenants"].push_back({
                {"tenant_id", sorted[i].first},
                {"revenue", sorted[i].second}
            });
        }
        
        co_return report;
    }

private:
    BillingLog billing_log_;
};

} // namespace saas
```

## 关键技术点

### 1. 租户隔离策略

| 隔离级别 | 实现方式 | 适用场景 |
|:---:|:---|:---|
| **数据库级** | 每个租户独立数据库 | 企业级大客户 |
| **Schema 级** | 共享数据库，独立 Schema | 中型客户 |
| **表级** | 共享 Schema，租户 ID 字段 | 小微客户 |

### 2. 数据安全

- **加密**：租户数据 AES-256 加密存储
- **审计**：所有数据访问记录审计日志
- **备份**：按租户隔离备份，支持单租户恢复

### 3. 性能优化

- **连接池**：按租户隔离数据库连接池
- **缓存**：租户级 Redis 缓存
- **限流**：租户级 + 用户级双层限流

## 部署架构

```yaml
# docker-compose.yml
version: '3.8'

services:
  api-gateway:
    build: ./api-gateway
    ports:
      - "8080:8080"
    environment:
      - REDIS_URL=redis://redis:6379
      - DB_URL=postgres://postgres:5432/nuclaw_saas
      
  tenant-agent-pool:
    build: ./tenant-agent
    replicas: 3
    environment:
      - VECTOR_DB_URL=http://milvus:19530
      
  redis:
    image: redis:7-alpine
    
  postgres:
    image: postgres:15
    volumes:
      - postgres_data:/var/lib/postgresql/data
      
  milvus:
    image: milvusdb/milvus:latest
    
  admin-portal:
    build: ./admin-portal
    ports:
      - "3000:3000"
```

---

**这是 NuClaw 的最终章！**

从 Step 0 的 89 行 Echo 服务器，到 Step 19 的 SaaS 化 AI 平台——你已经掌握了构建生产级 AI 系统的全部技能。
