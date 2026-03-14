# Step 19: SaaS 化 AI 助手平台

> **目标**：构建支持多租户、多用户的生产级 AI 助手平台
> 
> **难度**：⭐⭐⭐⭐⭐ (架构实战)
> 
> **代码量**：约 1800 行
> 
003e **预计时间**：8-10 小时

---

## 🎯 项目概述

### 为什么要做 SaaS 化？

前面 18 个 Step 我们构建了一个强大的 AI Agent 框架：
- ✅ 工具调用
- ✅ RAG 检索
- ✅ 记忆系统
- ✅ 多 Agent 协作

**但这只是一个单用户、单实例的应用。**

如果要把它变成**商业产品**，你需要解决：

| 问题 | 说明 |
|:---|:---|
| 🏢 **多租户隔离** | A 公司的数据不能让 B 公司看到 |
| 👥 **用户权限** | 管理员、操作员、访客，权限不同 |
| 🔐 **安全认证** | API Key、OAuth2、JWT |
| 📊 **运营分析** | 谁在用？用多少？付多少钱？ |
| 💰 **计费系统** | 按量计费、套餐管理、发票 |
| 🚀 **弹性扩展** | 高峰期自动扩容 |
| 🔧 **运维监控** | 告警、日志、备份 |

### 我们要实现什么

一个**智能客服 SaaS 平台**，服务企业客户：

```
平台运营方（你）
├── 多租户管理
├── 计费结算
├── 系统监控
└── 客户支持

租户 A（电商公司）
├── 管理员：张总
├── 客服组：小李、小王
├── 知识库：产品 FAQ、退货政策
├── 接入渠道：官网、App、微信公众号
└── 数据隔离：完全独立

租户 B（教育机构）
├── 管理员：李老师
├── 教师组：张老师、王老师
├── 知识库：课程资料、招生信息
├── 接入渠道：官网、钉钉群
└── 数据隔离：完全独立
```

---

## 📐 系统架构

### 整体架构

```
┌──────────────────────────────────────────────────────────────────────────────┐
│                              客户端层                                         │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐      │
│  │  Web     │  │  Mobile  │  │  微信    │  │  钉钉    │  │  飞书    │      │
│  │  前端    │  │  App     │  │  小程序  │  │  机器人  │  │  机器人  │      │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘  └────┬─────┘  └────┬─────┘      │
│       │             │             │             │             │            │
└───────┼─────────────┼─────────────┼─────────────┼─────────────┼────────────┘
        │             │             │             │             │
        └─────────────┴─────────────┴──────┬──────┴─────────────┘
                                           │
┌──────────────────────────────────────────┼──────────────────────────────────┐
│                              接入层       │                                  │
│  ┌───────────────────────────────────────┴──────────────────────────────┐   │
│  │                          API Gateway (Kong/Nginx)                     │   │
│  │  ┌────────────────────────────────────────────────────────────────┐  │   │
│  │  │  • 路由分发    • 负载均衡    • SSL 终止    • 静态资源缓存       │  │   │
│  │  └────────────────────────────────────────────────────────────────┘  │   │
│  │  ┌────────────────────────────────────────────────────────────────┐  │   │
│  │  │  • 限流控制    • 熔断降级    • 灰度发布    • A/B 测试         │  │   │
│  │  └────────────────────────────────────────────────────────────────┘  │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
└────────────────────────────────────────────────────────────────────────────┘
                                           │
┌──────────────────────────────────────────┼──────────────────────────────────┐
│                              服务层       │                                  │
│                                          ▼                                  │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                      Authentication Service                          │   │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐              │   │
│  │  │   OAuth2     │  │    JWT       │  │   API Key    │              │   │
│  │  │   认证       │  │   令牌       │  │   管理       │              │   │
│  │  └──────────────┘  └──────────────┘  └──────────────┘              │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                                                          │   │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                      Tenant Management Service                       │   │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐              │   │
│  │  │   租户 CRUD  │  │   配额管理   │  │   套餐管理   │              │   │
│  │  └──────────────┘  └──────────────┘  └──────────────┘              │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                                                          │   │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                      Agent Runtime Service                           │   │
│  │                                                                      │   │
│  │   ┌────────────┐  ┌────────────┐  ┌────────────┐  ┌────────────┐  │   │
│  │   │  租户 A    │  │  租户 B    │  │  租户 C    │  │    ...     │  │   │
│  │   │  Agent 池  │  │  Agent 池  │  │  Agent 池  │  │            │  │   │
│  │   │            │  │            │  │            │  │            │  │   │
│  │   │ • 独立 LLM │  │ • 独立 LLM │  │ • 独立 LLM │  │            │  │   │
│  │   │ • 独立知识 │  │ • 独立知识 │  │ • 独立知识 │  │            │  │   │
│  │   │ • 独立记忆 │  │ • 独立记忆 │  │ • 独立记忆 │  │            │  │   │
│  │   │ • 独立用户 │  │ • 独立用户 │  │ • 独立用户 │  │            │  │   │
│  │   └────────────┘  └────────────┘  └────────────┘  └────────────┘  │   │
│  │                                                                      │   │
│  │   每个租户：独立配置、独立数据、独立资源配额                        │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                                                          │   │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                      Billing Service                                 │   │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐              │   │
│  │  │   用量统计   │  │   计费计算   │  │   发票管理   │              │   │
│  │  └──────────────┘  └──────────────┘  └──────────────┘              │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                                                          │   │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                      Admin Portal Service                            │   │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐              │   │
│  │  │   租户管理   │  │   系统监控   │  │   财务报表   │              │   │
│  │  └──────────────┘  └──────────────┘  └──────────────┘              │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
└────────────────────────────────────────────────────────────────────────────┘
                                           │
┌──────────────────────────────────────────┼──────────────────────────────────┐
│                              数据层       │                                  │
│                                          ▼                                  │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                     多租户数据存储                                   │   │
│  │                                                                      │   │
│  │   PostgreSQL (按租户分 Schema)                                      │   │
│  │   ├── tenant_a.users                                                │   │
│  │   ├── tenant_a.conversations                                        │   │
│  │   ├── tenant_a.memories                                             │   │
│  │   ├── tenant_b.users                                                │   │
│  │   └── ...                                                           │   │
│  │                                                                      │   │
│  │   Milvus (向量数据库，按租户分 Collection)                           │   │
│  │   ├── tenant_a_memories                                             │   │
│  │   ├── tenant_b_memories                                             │   │
│  │   └── ...                                                           │   │
│  │                                                                      │   │
│  │   Redis (按租户分 Namespace)                                        │   │
│  │   ├── tenant_a:session:*                                            │   │
│  │   ├── tenant_a:cache:*                                              │   │
│  │   └── ...                                                           │   │
│  │                                                                      │   │
│  │   MinIO (对象存储，按租户分 Bucket)                                  │   │
│  │   ├── tenant-a-documents                                            │   │
│  │   ├── tenant-b-documents                                            │   │
│  │   └── ...                                                           │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                                                          │   │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                     共享基础设施                                     │   │
│  │   • Kafka (消息队列)    • Prometheus (监控)    • ELK (日志)         │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
└────────────────────────────────────────────────────────────────────────────┘
```

---

## 🗂️ 项目结构

```
step19/
├── README.md
├── docker-compose.yml          # 完整部署配置
├── kubernetes/
│   ├── namespace.yaml
│   ├── configmap.yaml
│   ├── secret.yaml
│   ├── api-gateway.yaml
│   ├── auth-service.yaml
│   ├── tenant-service.yaml
│   ├── agent-service.yaml
│   ├── billing-service.yaml
│   └── admin-portal.yaml
├── services/
│   ├── api-gateway/
│   │   ├── Dockerfile
│   │   ├── nginx.conf
│   │   └── kong.yml
│   ├── auth-service/
│   │   ├── include/saas/auth/
│   │   ├── src/
│   │   └── tests/
│   ├── tenant-service/
│   │   ├── include/saas/tenant/
│   │   ├── src/
│   │   └── tests/
│   ├── agent-service/
│   │   ├── include/saas/agent/
│   │   ├── src/
│   │   └── tests/
│   ├── billing-service/
│   │   ├── include/saas/billing/
│   │   ├── src/
│   │   └── tests/
│   └── admin-portal/
│       └── ...
├── shared/
│   └── include/saas/common/
│       ├── tenant_context.hpp
│       ├── data_isolation.hpp
│       ├── security.hpp
│       └── ...
└── scripts/
    ├── init-db.sh
    ├── create-tenant.sh
    └── backup.sh
```

---

## 📝 核心实现详解

### 1. 租户上下文（贯穿所有请求）

```cpp
// shared/include/saas/common/tenant_context.hpp
#pragma once
#include <string>
#include <vector>
#include <chrono>

namespace saas {

// 租户信息
struct Tenant {
    std::string tenant_id;          // UUID
    std::string name;               // 租户名称
    std::string slug;               // URL 标识
    
    // 套餐信息
    enum class Plan {
        FREE,       // 免费版
        BASIC,      // 基础版
        PRO,        // 专业版
        ENTERPRISE  // 企业版
    } plan = Plan::BASIC;
    
    // 资源配额
    struct Quota {
        int max_users = 5;
        int max_conversations_per_month = 1000;
        int max_tokens_per_month = 1000000;
        int max_storage_mb = 100;
        int max_api_calls_per_minute = 60;
        int max_agents = 1;
    } quota;
    
    // 计费信息
    struct Billing {
        std::string currency = "CNY";
        double price_per_1k_tokens = 0.02;
        double monthly_base_fee = 99.0;
        bool auto_renew = true;
    } billing;
    
    // 状态
    bool is_active = true;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point expires_at;
    
    // 扩展配置
    nlohmann::json settings;
};

// 用户信息
struct User {
    std::string user_id;
    std::string tenant_id;
    std::string email;
    std::string name;
    std::string avatar_url;
    
    // 角色
    enum class Role {
        OWNER,      // 租户所有者
        ADMIN,      // 管理员
        OPERATOR,   // 操作员
        VIEWER      // 访客
    } role = Role::VIEWER;
    
    // 权限列表（具体权限）
    std::vector<std::string> permissions;
    
    // API Key（用于程序化访问）
    struct APIKey {
        std::string key_id;
        std::string key_secret;  // 只显示一次
        std::string name;
        std::vector<std::string> scopes;  // ["chat", "read", "write"]
        std::chrono::system_clock::time_point created_at;
        std::optional<std::chrono::system_clock::time_point> expires_at;
        bool is_active = true;
    };
    std::vector<APIKey> api_keys;
};

// 租户上下文 - 每个请求都有
class TenantContext {
public:
    // 基本信息
    std::string tenant_id;
    std::string user_id;
    std::vector<std::string> roles;
    std::vector<std::string> permissions;
    
    // 请求信息
    std::string request_id;
    std::chrono::system_clock::time_point request_time;
    std::string client_ip;
    std::string user_agent;
    
    // 认证方式
    enum class AuthMethod {
        SESSION,    // Cookie Session
        JWT,        // JWT Token
        API_KEY     // API Key
    } auth_method;
    
    // 检查权限
    bool has_permission(const std::string& permission) const {
        // 所有者拥有所有权限
        if (std::find(roles.begin(), roles.end(), "owner") != roles.end()) {
            return true;
        }
        
        // 检查具体权限
        return std::find(permissions.begin(), permissions.end(), permission) 
               != permissions.end();
    }
    
    // 检查资源配额
    bool check_quota(const std::string& resource_type, int usage);
};

// 上下文管理器（从请求中提取上下文）
class TenantContextManager {
public:
    // 从 HTTP 请求中提取租户上下文
    static TenantContext extract(const httplib::Request& req) {
        TenantContext ctx;
        
        // 1. 提取 Tenant ID
        ctx.tenant_id = extract_tenant_id(req);
        
        // 2. 提取认证信息
        auto auth_header = req.get_header_value("Authorization");
        if (auth_header.starts_with("Bearer ")) {
            auto token = auth_header.substr(7);
            ctx = parse_jwt_token(token);
        } else if (auth_header.starts_with("ApiKey ")) {
            auto api_key = auth_header.substr(7);
            ctx = validate_api_key(api_key);
        }
        
        // 3. 提取请求信息
        ctx.request_id = req.get_header_value("X-Request-ID");
        if (ctx.request_id.empty()) {
            ctx.request_id = generate_uuid();
        }
        ctx.request_time = std::chrono::system_clock::now();
        ctx.client_ip = req.remote_addr;
        
        return ctx;
    }

private:
    static std::string extract_tenant_id(const httplib::Request& req) {
        // 优先级：Header > Subdomain > Path
        
        // 1. 从 Header 提取
        auto tenant_id = req.get_header_value("X-Tenant-ID");
        if (!tenant_id.empty()) {
            return tenant_id;
        }
        
        // 2. 从 Subdomain 提取（tenant-a.example.com）
        auto host = req.get_header_value("Host");
        // 解析 subdomain...
        
        // 3. 从 Path 提取（/api/v1/tenant-a/...）
        // 解析 path...
        
        throw std::runtime_error("Tenant ID not found");
    }
    
    static TenantContext parse_jwt_token(const std::string& token);
    static TenantContext validate_api_key(const std::string& api_key);
};

} // namespace saas
```

---

### 2. 数据隔离层

```cpp
// shared/include/saas/common/data_isolation.hpp
#pragma once
#include <tenant_context.hpp>
#include <memory>
#include <string>

namespace saas {

// 数据隔离策略
enum class IsolationStrategy {
    DATABASE,    // 每个租户独立数据库
    SCHEMA,      // 共享数据库，独立 Schema
    TABLE_PREFIX // 共享 Schema，表名加租户前缀
};

// 租户隔离的数据库连接
class TenantDatabase {
public:
    TenantDatabase(
        IsolationStrategy strategy,
        std::shared_ptr<DatabasePool> pool
    ) : strategy_(strategy), pool_(pool) {}
    
    // 获取租户专属连接
    std::shared_ptr<DatabaseConnection> get_connection(
        const TenantContext& ctx
    ) {
        switch (strategy_) {
            case IsolationStrategy::DATABASE:
                return get_dedicated_db_connection(ctx.tenant_id);
            case IsolationStrategy::SCHEMA:
                return get_schema_connection(ctx.tenant_id);
            case IsolationStrategy::TABLE_PREFIX:
                return get_prefixed_connection(ctx.tenant_id);
        }
    }
    
    // 执行 SQL（自动添加租户过滤）
    template<typename T>
    async::Task<std::vector<T>> query(
        const TenantContext& ctx,
        const std::string& sql,
        const std::vector<std::string>& params
    ) {
        auto conn = get_connection(ctx);
        
        // 根据策略修改 SQL
        auto tenant_sql = apply_tenant_filter(ctx, sql);
        
        co_return co_await conn->query<T>(tenant_sql, params);
    }
    
    // 插入数据（自动添加 tenant_id）
    template<typename T>
    async::Task<void> insert(
        const TenantContext& ctx,
        const std::string& table,
        const T& data
    ) {
        auto conn = get_connection(ctx);
        
        // 添加 tenant_id
        auto data_with_tenant = data;
        data_with_tenant.tenant_id = ctx.tenant_id;
        
        co_await conn->insert(table, data_with_tenant);
    }

private:
    IsolationStrategy strategy_;
    std::shared_ptr<DatabasePool> pool_;
    
    std::string apply_tenant_filter(
        const TenantContext& ctx,
        const std::string& sql
    ) {
        switch (strategy_) {
            case IsolationStrategy::TABLE_PREFIX: {
                // 替换表名：users -> tenant_a_users
                auto tenant_table = fmt::format("tenant_{}_{}", 
                    ctx.tenant_id, extract_table_name(sql));
                return replace_table_name(sql, tenant_table);
            }
            case IsolationStrategy::SCHEMA: {
                // 添加 schema：SELECT * FROM users
                // -> SELECT * FROM tenant_a.users
                return fmt::format("SET search_path TO tenant_{}; {}",
                    ctx.tenant_id, sql);
            }
            default:
                // DATABASE 级别不需要修改 SQL
                return sql;
        }
    }
};

// 向量数据库隔离
class TenantVectorStore {
public:
    // 获取租户专属的 Collection 名称
    std::string get_collection_name(
        const std::string& tenant_id,
        const std::string& base_name
    ) {
        return fmt::format("tenant_{}_{}", tenant_id, base_name);
    }
    
    // 创建租户 Collection
    async::Task<void> create_tenant_collection(
        const std::string& tenant_id,
        const std::string& base_name,
        const VectorSchema& schema
    ) {
        auto collection_name = get_collection_name(tenant_id, base_name);
        co_await milvus_client_.create_collection(collection_name, schema);
    }
    
    // 插入向量（自动归属租户）
    async::Task<void> insert(
        const TenantContext& ctx,
        const std::string& base_name,
        const std::vector<VectorRecord>& records
    ) {
        auto collection = get_collection_name(ctx.tenant_id, base_name);
        
        // 添加 tenant_id 到 metadata
        auto records_with_tenant = records;
        for (auto& r : records_with_tenant) {
            r.metadata["tenant_id"] = ctx.tenant_id;
        }
        
        co_await milvus_client_.insert(collection, records_with_tenant);
    }
    
    // 搜索（自动过滤租户）
    async::Task<std::vector<VectorRecord>> search(
        const TenantContext& ctx,
        const std::string& base_name,
        const std::vector<float>& query_vector,
        size_t top_k
    ) {
        auto collection = get_collection_name(ctx.tenant_id, base_name);
        
        // 添加租户过滤条件
        auto filter = fmt::format("tenant_id == '{}'", ctx.tenant_id);
        
        co_return co_await milvus_client_.search(
            collection, query_vector, top_k, filter
        );
    }

private:
    MilvusClient milvus_client_;
};

} // namespace saas
```

---

### 3. Agent 运行时服务

```cpp
// services/agent-service/include/saas/agent/runtime.hpp
#pragma once
#include <saas/common/tenant_context.hpp>
#include <saas/common/data_isolation.hpp>
#include <nuclaw/chat_engine.hpp>
#include <nuclaw/memory_manager.hpp>
#include <map>
#include <mutex>

namespace saas {

// 租户的 Agent 运行时
class TenantAgentRuntime {
public:
    TenantAgentRuntime(
        const Tenant& tenant,
        std::shared_ptr<TenantDatabase> db,
        std::shared_ptr<TenantVectorStore> vector_store
    ) : tenant_(tenant), db_(db), vector_store_(vector_store) {
        
        initialize();
    }
    
    // 处理对话请求
    async::Task<ChatResponse> chat(
        const TenantContext& ctx,
        const ChatRequest& request
    ) {
        // 1. 检查配额
        if (!check_quota(ctx, "conversation")) {
            co_return ChatResponse{
                .error = "Quota exceeded. Please upgrade your plan."
            };
        }
        
        // 2. 记录请求
        log_request(ctx, request);
        
        // 3. 加载或创建会话
        auto session = co_await get_or_create_session(ctx, request.session_id);
        
        // 4. 检索相关记忆
        auto memories = co_await retrieve_memories(ctx, request.message);
        
        // 5. 调用 LLM
        auto response = co_await chat_engine_->process(
            request.message,
            memories,
            session->get_history()
        );
        
        // 6. 更新会话历史
        session->add_message("user", request.message);
        session->add_message("assistant", response);
        
        // 7. 存储新记忆
        co_await store_memory(ctx, request.message, response);
        
        // 8. 记录用量
        record_usage(ctx, "conversation", 1);
        record_usage(ctx, "tokens", estimate_tokens(request.message, response));
        
        co_return ChatResponse{
            .message = response,
            .session_id = session->id(),
            .usage = {
                .prompt_tokens = estimate_tokens(request.message),
                .completion_tokens = estimate_tokens(response),
                .total_tokens = estimate_tokens(request.message, response)
            }
        };
    }
    
    // 管理知识库
    async::Task<void> upload_document(
        const TenantContext& ctx,
        const Document& doc
    ) {
        // 检查权限
        if (!ctx.has_permission("knowledge.write")) {
            throw PermissionDenied("No permission to upload documents");
        }
        
        // 检查存储配额
        auto current_storage = get_storage_usage(ctx.tenant_id);
        if (current_storage + doc.size > tenant_.quota.max_storage_mb * 1024 * 1024) {
            throw QuotaExceeded("Storage quota exceeded");
        }
        
        // 存储文档
        auto doc_id = co_await document_store_.save(ctx, doc);
        
        // 提取文本并分块
        auto chunks = document_processor_.extract_and_chunk(doc);
        
        // 生成 embedding 并存储
        for (const auto& chunk : chunks) {
            auto embedding = co_await embedding_service_.encode(chunk.text);
            
            co_await vector_store_->insert(ctx, "knowledge", {
                .id = generate_uuid(),
                .vector = embedding,
                .metadata = {
                    {"tenant_id", ctx.tenant_id},
                    {"doc_id", doc_id},
                    {"chunk_id", chunk.id},
                    {"text", chunk.text}
                }
            });
        }
        
        // 更新存储用量
        update_storage_usage(ctx.tenant_id, doc.size);
    }

private:
    Tenant tenant_;
    std::shared_ptr<TenantDatabase> db_;
    std::shared_ptr<TenantVectorStore> vector_store_;
    
    std::unique_ptr<ChatEngine> chat_engine_;
    std::unique_ptr<MemoryManager> memory_manager_;
    DocumentStore document_store_;
    DocumentProcessor document_processor_;
    EmbeddingService embedding_service_;
    
    // 会话缓存
    std::map<std::string, std::shared_ptr<ChatSession>> sessions_;
    std::mutex sessions_mutex_;
    
    void initialize();
    bool check_quota(const TenantContext& ctx, const std::string& resource);
    async::Task<std::shared_ptr<ChatSession>> get_or_create_session(
        const TenantContext& ctx,
        const std::string& session_id
    );
    async::Task<std::vector<Memory>> retrieve_memories(
        const TenantContext& ctx,
        const std::string& query
    );
    async::Task<void> store_memory(
        const TenantContext& ctx,
        const std::string& user_msg,
        const std::string& assistant_msg
    );
};

// Agent 运行时管理器（管理所有租户）
class AgentRuntimeManager {
public:
    std::shared_ptr<TenantAgentRuntime> get_runtime(
        const std::string& tenant_id
    ) {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        
        auto it = runtimes_.find(tenant_id);
        if (it != runtimes_.end()) {
            return it->second;
        }
        
        lock.unlock();
        
        // 创建新运行时
        std::unique_lock<std::shared_mutex> unique_lock(mutex_);
        
        auto tenant = tenant_manager_.get(tenant_id);
        auto runtime = std::make_shared<TenantAgentRuntime>(
            tenant, db_pool_, vector_store_
        );
        
        runtimes_[tenant_id] = runtime;
        return runtime;
    }
    
    // 热重载租户配置
    void reload_tenant(const std::string& tenant_id) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        runtimes_.erase(tenant_id);
    }

private:
    std::shared_mutex mutex_;
    std::map<std::string, std::shared_ptr<TenantAgentRuntime>> runtimes_;
    TenantManager tenant_manager_;
    std::shared_ptr<TenantDatabase> db_pool_;
    std::shared_ptr<TenantVectorStore> vector_store_;
};

} // namespace saas
```

---

### 4. 计费服务

```cpp
// services/billing-service/include/saas/billing/service.hpp
#pragma once
#include <saas/common/tenant_context.hpp>
#include <vector>
#include <map>

namespace saas {

// 计费记录
struct UsageRecord {
    std::string record_id;
    std::string tenant_id;
    std::string user_id;
    std::string resource_type;  // "conversation", "token", "storage"
    int quantity;
    double cost;
    std::chrono::system_clock::time_point timestamp;
    std::map<std::string, std::string> metadata;
};

// 计费服务
class BillingService {
public:
    // 记录用量（实时计费）
    void record_usage(const UsageRecord& record) {
        // 写入时序数据库
        usage_db_.insert(record);
        
        // 如果是预付费，实时扣减余额
        auto tenant = tenant_manager_.get(record.tenant_id);
        if (tenant.billing.auto_renew) {
            // 后付费模式，只记录
        } else {
            // 预付费模式，扣减余额
            deduct_balance(record.tenant_id, record.cost);
        }
        
        // 检查阈值告警
        check_threshold_alert(record.tenant_id);
    }
    
    // 生成月账单
    async::Task<nlohmann::json> generate_monthly_bill(
        const std::string& tenant_id,
        int year,
        int month
    ) {
        auto start_date = fmt::format("{}-{}-01", year, month);
        auto end_date = get_month_end(year, month);
        
        // 查询用量
        auto records = co_await usage_db_.query(tenant_id, start_date, end_date);
        
        // 按资源类型汇总
        std::map<std::string, std::pair<int, double>> summary;
        for (const auto& r : records) {
            summary[r.resource_type].first += r.quantity;
            summary[r.resource_type].second += r.cost;
        }
        
        // 构建账单
        nlohmann::json bill = {
            {"bill_id", generate_uuid()},
            {"tenant_id", tenant_id},
            {"period", fmt::format("{}-{}", year, month)},
            {"start_date", start_date},
            {"end_date", end_date},
            {"items", nlohmann::json::array()},
            {"subtotal", 0.0},
            {"tax", 0.0},
            {"total", 0.0}
        };
        
        double subtotal = 0;
        for (const auto& [type, data] : summary) {
            bill["items"].push_back({
                {"resource_type", type},
                {"quantity", data.first},
                {"unit_price", get_unit_price(type)},
                {"cost", data.second}
            });
            subtotal += data.second;
        }
        
        // 添加基础费用
        auto tenant = tenant_manager_.get(tenant_id);
        bill["items"].push_back({
            {"resource_type", "base_fee"},
            {"description", "Monthly base fee"},
            {"cost", tenant.billing.monthly_base_fee}
        });
        subtotal += tenant.billing.monthly_base_fee;
        
        bill["subtotal"] = subtotal;
        bill["tax"] = subtotal * 0.13;  // 13% 增值税
        bill["total"] = subtotal + bill["tax"].get<double>();
        
        // 保存账单
        co_await bill_db_.save(bill);
        
        co_return bill;
    }
    
    // 运营报表（平台级）
    async::Task<nlohmann::json> generate_platform_report(
        int year,
        int month
    ) {
        auto stats = co_await usage_db_.aggregate_by_tenant(year, month);
        
        nlohmann::json report = {
            {"period", fmt::format("{}-{}", year, month)},
            {"total_revenue", 0.0},
            {"total_active_tenants", stats.size()},
            {"total_conversations", 0},
            {"total_tokens", 0},
            {"top_tenants", nlohmann::json::array()},
            {"plan_distribution", {
                {"free", 0},
                {"basic", 0},
                {"pro", 0},
                {"enterprise", 0}
            }}
        };
        
        // 计算总收入和用量
        double total_revenue = 0;
        int total_conversations = 0;
        int64_t total_tokens = 0;
        
        std::vector<std::pair<std::string, double>> revenue_ranking;
        
        for (const auto& [tenant_id, data] : stats) {
            total_revenue += data.revenue;
            total_conversations += data.conversations;
            total_tokens += data.tokens;
            
            revenue_ranking.push_back({tenant_id, data.revenue});
            
            // 统计套餐分布
            auto tenant = tenant_manager_.get(tenant_id);
            auto plan_str = plan_to_string(tenant.plan);
            report["plan_distribution"][plan_str] = 
                report["plan_distribution"][plan_str].get<int>() + 1;
        }
        
        report["total_revenue"] = total_revenue;
        report["total_conversations"] = total_conversations;
        report["total_tokens"] = total_tokens;
        
        // Top 10 租户
        std::sort(revenue_ranking.begin(), revenue_ranking.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });
        
        for (size_t i = 0; i < std::min(size_t(10), revenue_ranking.size()); ++i) {
            auto tenant = tenant_manager_.get(revenue_ranking[i].first);
            report["top_tenants"].push_back({
                {"tenant_id", revenue_ranking[i].first},
                {"tenant_name", tenant.name},
                {"revenue", revenue_ranking[i].second},
                {"plan", plan_to_string(tenant.plan)}
            });
        }
        
        co_return report;
    }

private:
    UsageDatabase usage_db_;
    BillDatabase bill_db_;
    TenantManager tenant_manager_;
};

} // namespace saas
```

---

## 💬 API 使用示例

### 场景 1：创建租户

```bash
# 平台管理员创建新租户
curl -X POST https://api.nuclaw-saas.com/v1/admin/tenants \
  -H "Authorization: Bearer $PLATFORM_ADMIN_TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "name": "示例科技有限公司",
    "slug": "example-tech",
    "plan": "pro",
    "admin_email": "admin@example.com"
  }'

# 响应
{
  "tenant_id": "tenant_abc123",
  "name": "示例科技有限公司",
  "slug": "example-tech",
  "plan": "pro",
  "api_endpoint": "https://example-tech.nuclaw-saas.com",
  "created_at": "2024-01-15T10:30:00Z",
  "admin_invite_link": "https://..."
}
```

### 场景 2：租户用户对话

```bash
# 租户用户调用对话接口
curl -X POST https://example-tech.nuclaw-saas.com/v1/chat \
  -H "Authorization: ApiKey sk_example_xxx" \
  -H "X-Tenant-ID: tenant_abc123" \
  -H "Content-Type: application/json" \
  -d '{
    "message": "你们的产品支持退款吗？",
    "session_id": "session_xyz789"
  }'

# 响应
{
  "message": "支持的呢！我们在购买后 7 天内支持无理由退款...",
  "session_id": "session_xyz789",
  "usage": {
    "prompt_tokens": 45,
    "completion_tokens": 128,
    "total_tokens": 173
  }
}
```

### 场景 3：查看用量

```bash
# 租户管理员查看本月用量
curl -X GET "https://api.nuclaw-saas.com/v1/tenants/tenant_abc123/usage?month=2024-01" \
  -H "Authorization: Bearer $TENANT_ADMIN_TOKEN"

# 响应
{
  "period": "2024-01",
  "quota": {
    "conversations_limit": 10000,
    "tokens_limit": 1000000
  },
  "usage": {
    "conversations": 3456,
    "tokens": 523400,
    "storage_mb": 45.2
  },
  "remaining": {
    "conversations": 6544,
    "tokens": 476600
  },
  "projected_cost": 245.80
}
```

---

## 🎓 学习要点

| 知识点 | 说明 |
|:---|:---|
| **多租户架构** | 数据隔离策略、资源配额管理 |
| **认证授权** | OAuth2、JWT、API Key、RBAC |
| **服务拆分** | 微服务边界划分、服务间通信 |
| **数据安全** | 加密存储、审计日志、合规 |
| **计费设计** | 按量计费、套餐设计、税务处理 |
| **运维监控** | 日志聚合、指标监控、告警 |

---

**恭喜！你完成了 NuClaw 全部 20 个 Step！** 🎉

从 89 行的 Echo 服务器，到 1800+ 行的 SaaS 化 AI 平台——

你已经掌握了构建**生产级 AI Agent 系统**的全部技能：

- ✅ C++ 高性能网络编程
- ✅ AI Agent 架构设计
- ✅ LLM 应用开发
- ✅ 多租户 SaaS 架构

**现在，去构建你的产品吧！** 🚀
