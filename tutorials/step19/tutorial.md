# Step 19: 智能客服 SaaS 平台 —— 高级功能实现

> 目标：实现人机协作、多租户隔离、计费系统、管理后台
> 
003e 难度：⭐⭐⭐⭐⭐ | 代码量：约 1200 行 | 预计学习时间：4-5 小时

---

## 一、Human Service —— 人工客服

### 1.1 人机协作流程

```
用户发起对话
    │
    ▼
┌─────────────────┐
│   AI 处理消息   │
└────────┬────────┘
         │ 置信度低/用户要求
         ▼
┌─────────────────┐
│  排队等待人工   │ ◀── 用户看到：正在为您转接人工客服...
└────────┬────────┘
         │ 客服上线
         ▼
┌─────────────────┐
│ 人工客服接管    │ ◀── 用户和客服直接对话
└────────┬────────┘
         │ 问题解决
         ▼
┌─────────────────┐
│  会话结束/转回 AI │
└─────────────────┘
```

### 1.2 人工客服服务实现

```cpp
// src/services/human/service.hpp

namespace smartsupport::human {

// 客服状态
enum class AgentStatus {
    OFFLINE,    // 离线
    ONLINE,     // 在线（可接新会话）
    BUSY,       // 忙碌（会话数已满）
    AWAY        // 离开
};

// 人工客服
struct HumanAgent {
    std::string id;
    std::string name;
    std::string tenant_id;
    AgentStatus status = AgentStatus::OFFLINE;
    int max_sessions = 5;           // 最大并发会话数
    int current_sessions = 0;       // 当前会话数
    std::vector<std::string> skills; // 技能标签（销售/技术支持/售后）
};

// 排队中的会话
struct QueuedSession {
    std::string session_id;
    std::string tenant_id;
    std::string user_id;
    std::chrono::steady_clock::time_point queued_at;
    int priority = 0;               // 优先级（VIP 用户优先）
    std::vector<std::string> required_skills;
};

class HumanService {
public:
    HumanService(asio::io_context& io);
    
    // 客服登录/登出
    void agent_login(const HumanAgent& agent);
    void agent_logout(const std::string& agent_id);
    void update_status(const std::string& agent_id, AgentStatus status);
    
    // 请求人工服务
    void request_human(const QueuedSession& session);
    
    // 客服接管会话
    void agent_take_session(const std::string& agent_id,
                            const std::string& session_id);
    
    // 客服发送消息
    void agent_send_message(const std::string& agent_id,
                            const std::string& session_id,
                            const std::string& content);
    
    // 客服结束会话
    void agent_end_session(const std::string& agent_id,
                           const std::string& session_id);
    
    // 获取客服工作台数据
    struct Dashboard {
        std::vector<SessionInfo> active_sessions;
        std::vector<QueuedSession> waiting_sessions;
        AgentStats stats;
    };
    Dashboard get_agent_dashboard(const std::string& agent_id);

private:
    // 分配算法
    std::optional<std::string> find_best_agent(const QueuedSession& session);
    
    // 排队处理定时器
    void process_queue();
    
    std::map<std::string, HumanAgent> agents_;
    std::map<std::string, std::vector<std::string>> agent_sessions_;  // agent_id -> session_ids
    std::queue<QueuedSession> waiting_queue_;
    std::map<std::string, std::string> session_agent_;  // session_id -> agent_id
    
    asio::steady_timer queue_timer_;
    std::mutex mutex_;
};

} // namespace
```

```cpp
// src/services/human/service.cpp

void HumanService::request_human(const QueuedSession& session) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 1. 尝试立即分配
    auto agent_id = find_best_agent(session);
    if (agent_id) {
        // 立即分配
        assign_session(*agent_id, session.session_id);
        notify_user_assigned(session.session_id, *agent_id);
    } else {
        // 2. 加入等待队列
        waiting_queue_.push(session);
        notify_user_queued(session.session_id, waiting_queue_.size());
    }
}

std::optional<std::string> HumanService::find_best_agent(
    const QueuedSession& session) {
    
    std::optional<std::string> best_agent;
    int min_load = INT_MAX;
    
    for (const auto& [id, agent] : agents_) {
        // 检查基本可用性
        if (agent.status != AgentStatus::ONLINE) continue;
        if (agent.current_sessions >= agent.max_sessions) continue;
        if (agent.tenant_id != session.tenant_id) continue;
        
        // 检查技能匹配
        bool skills_match = true;
        for (const auto& skill : session.required_skills) {
            if (std::find(agent.skills.begin(), agent.skills.end(), skill) 
                == agent.skills.end()) {
                skills_match = false;
                break;
            }
        }
        if (!skills_match) continue;
        
        // 选择负载最低的
        if (agent.current_sessions < min_load) {
            min_load = agent.current_sessions;
            best_agent = id;
        }
    }
    
    return best_agent;
}

void HumanService::process_queue() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::queue<QueuedSession> new_queue;
    
    while (!waiting_queue_.empty()) {
        auto session = waiting_queue_.front();
        waiting_queue_.pop();
        
        // 尝试分配
        auto agent_id = find_best_agent(session);
        if (agent_id) {
            assign_session(*agent_id, session.session_id);
            notify_user_assigned(session.session_id, *agent_id);
        } else {
            // 检查是否等待超时（超过 5 分钟）
            auto wait_time = std::chrono::steady_clock::now() - session.queued_at;
            if (wait_time > std::chrono::minutes(5)) {
                // 超时，通知用户
                notify_user_timeout(session.session_id);
            } else {
                new_queue.push(session);
            }
        }
    }
    
    waiting_queue_ = std::move(new_queue);
    
    // 重新定时
    queue_timer_.expires_after(std::chrono::seconds(5));
    queue_timer_.async_wait([this](auto) { process_queue(); });
}
```

---

## 二、Tenant Service —— 多租户隔离

### 2.1 数据隔离实现

```cpp
// src/services/tenant/isolator.hpp

namespace smartsupport::tenant {

// 租户上下文（Thread-local）
class TenantContext {
public:
    static void set_current(const std::string& tenant_id) {
        current_tenant_ = tenant_id;
    }
    
    static std::string get_current() {
        return current_tenant_;
    }
    
    static void clear() {
        current_tenant_.clear();
    }

private:
    static thread_local std::string current_tenant_;
};

// 租户隔离数据库连接
class TenantDatabase {
public:
    TenantDatabase(std::shared_ptr<pqxx::connection_pool> pool);
    
    // 执行查询（自动注入 tenant_id）
    template<typename... Args>
    pqxx::result query(const std::string& sql, Args... args) {
        auto tenant_id = TenantContext::get_current();
        if (tenant_id.empty()) {
            throw std::runtime_error("No tenant context");
        }
        
        // 获取连接
        auto conn = pool_>acquire();
        
        // 设置当前租户（用于 RLS）
        pqxx::work txn(*conn);
        txn.exec("SET app.current_tenant = '" + tenant_id + "'");
        
        // 执行查询
        auto result = txn.exec_params(sql, args...);
        txn.commit();
        
        return result;
    }
    
    // 带租户过滤的查询构建
    std::string add_tenant_filter(const std::string& sql) {
        auto tenant_id = TenantContext::get_current();
        
        // 如果 SQL 已有 WHERE，添加 AND；否则添加 WHERE
        if (sql.find("WHERE") != std::string::npos) {
            return sql + " AND tenant_id = '" + tenant_id + "'";
        } else {
            size_t order_by = sql.find("ORDER BY");
            if (order_by != std::string::npos) {
                return sql.substr(0, order_by) + 
                       " WHERE tenant_id = '" + tenant_id + "' " +
                       sql.substr(order_by);
            }
            return sql + " WHERE tenant_id = '" + tenant_id + "'";
        }
    }

private:
    std::shared_ptr<pqxx::connection_pool> pool_;
};

// 资源配额管理
class QuotaManager {
public:
    struct Quota {
        int max_conversations_per_day;
        int max_ai_calls_per_day;
        int max_storage_mb;
        int max_agents;
    };
    
    bool check_quota(const std::string& tenant_id,
                     ResourceType type,
                     int amount);
    
    void record_usage(const std::string& tenant_id,
                      ResourceType type,
                      int amount);
    
    Quota get_quota(const std::string& tenant_id);

private:
    std::map<std::string, Quota> plan_quotas_ = {
        {"free", {100, 500, 100, 1}},
        {"pro", {1000, 5000, 1000, 5}},
        {"enterprise", {10000, 50000, 10000, -1}}  // -1 = 无限制
    };
};

} // namespace
```

### 2.2 租户服务实现

```cpp
// src/services/tenant/service.hpp

class TenantService {
public:
    struct Tenant {
        std::string id;
        std::string name;
        std::string plan;
        std::chrono::timestamp created_at;
        std::map<std::string, std::string> config;
    };
    
    TenantService(std::shared_ptr<TenantDatabase> db);
    
    // 租户 CRUD
    Tenant create_tenant(const std::string& name, const std::string& plan);
    Tenant get_tenant(const std::string& tenant_id);
    void update_tenant(const Tenant& tenant);
    void delete_tenant(const std::string& tenant_id);
    
    // 配置管理
    std::string get_config(const std::string& tenant_id,
                           const std::string& key,
                           const std::string& default_value = "");
    void set_config(const std::string& tenant_id,
                    const std::string& key,
                    const std::string& value);
    
    // 成员管理
    void add_member(const std::string& tenant_id,
                    const std::string& user_id,
                    const std::string& role);
    void remove_member(const std::string& tenant_id,
                       const std::string& user_id);
    std::vector<Member> list_members(const std::string& tenant_id);

private:
    std::shared_ptr<TenantDatabase> db_;
    QuotaManager quota_mgr_;
};
```

---

## 三、Billing Service —— 计费系统

```cpp
// src/services/billing/service.hpp

namespace smartsupport::billing {

// 计费项
enum class BillingItem {
    AI_CALL,        // AI 调用次数
    MESSAGE,        // 消息数
    STORAGE,        // 存储空间（MB）
    HUMAN_AGENT     // 人工客服席位数
};

// 套餐定义
struct Plan {
    std::string id;
    std::string name;
    std::string description;
    double monthly_price;
    std::map<BillingItem, int> quotas;  // 包含额度
    std::map<BillingItem, double> overage_prices;  // 超额单价
};

// 用量记录
struct UsageRecord {
    std::string tenant_id;
    BillingItem item;
    int amount;
    std::chrono::timestamp timestamp;
    std::string session_id;  // 关联会话
};

class BillingService {
public:
    BillingService(std::shared_ptr<Database> db);
    
    // 记录用量（实时）
    void record_usage(const std::string& tenant_id,
                      BillingItem item,
                      int amount,
                      const std::string& session_id = "");
    
    // 获取当前周期用量
    struct UsageSummary {
        std::map<BillingItem, int> used;
        std::map<BillingItem, int> quota;
        std::map<BillingItem, double> overage_cost;
        double total_cost;
    };
    UsageSummary get_current_usage(const std::string& tenant_id);
    
    // 生成账单（月度）
    struct Invoice {
        std::string id;
        std::string tenant_id;
        std::string period;  // 2024-03
        double amount;
        std::map<BillingItem, int> breakdown;
        bool paid;
    };
    Invoice generate_invoice(const std::string& tenant_id,
                             const std::string& period);
    
    // 检查是否欠费（用于限流）
    bool is_payment_overdue(const std::string& tenant_id);

private:
    // 聚合用量（按小时批量写入，减少数据库压力）
    void flush_usage_buffer();
    
    std::shared_ptr<Database> db_;
    std::vector<UsageRecord> usage_buffer_;
    std::mutex buffer_mutex_;
    asio::steady_timer flush_timer_;
};

} // namespace
```

```cpp
// src/services/billing/service.cpp

void BillingService::record_usage(const std::string& tenant_id,
                                  BillingItem item,
                                  int amount,
                                  const std::string& session_id) {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    
    usage_buffer_.push_back({
        .tenant_id = tenant_id,
        .item = item,
        .amount = amount,
        .timestamp = std::chrono::system_clock::now(),
        .session_id = session_id
    });
    
    // 缓冲区满，立即刷新
    if (usage_buffer_.size() >= 1000) {
        flush_usage_buffer();
    }
}

void BillingService::flush_usage_buffer() {
    std::vector<UsageRecord> to_flush;
    {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        to_flush = std::move(usage_buffer_);
        usage_buffer_.clear();
    }
    
    // 按租户和计费项聚合
    std::map<std::string, std::map<BillingItem, int>> aggregated;
    for (const auto& record : to_flush) {
        aggregated[record.tenant_id][record.item] += record.amount;
    }
    
    // 批量写入数据库
    pqxx::work txn(*db_>connection());
    for (const auto& [tenant_id, items] : aggregated) {
        for (const auto& [item, amount] : items) {
            txn.exec_params(
                "INSERT INTO usage_stats (tenant_id, item, amount, hour) "
                "VALUES ($1, $2, $3, date_trunc('hour', now())) "
                "ON CONFLICT (tenant_id, item, hour) "
                "DO UPDATE SET amount = usage_stats.amount + $3",
                tenant_id,
                billing_item_to_string(item),
                amount
            );
        }
    }
    txn.commit();
}

BillingService::UsageSummary BillingService::get_current_usage(
    const std::string& tenant_id) {
    
    // 刷新缓冲区确保数据最新
    flush_usage_buffer();
    
    // 查询本月用量
    auto result = db_
003equery(
        "SELECT item, SUM(amount) FROM usage_stats "
        "WHERE tenant_id = $1 AND hour >= date_trunc('month', now()) "
        "GROUP BY item",
        tenant_id
    );
    
    // 获取套餐额度
    auto plan = db_
003equery(
        "SELECT plan FROM tenants WHERE id = $1",
        tenant_id
    );
    auto quotas = get_plan_quotas(plan[0][0].as<std::string>());
    
    // 计算用量和超额费用
    UsageSummary summary;
    for (const auto& row : result) {
        auto item = string_to_billing_item(row[0].as<std::string>());
        int used = row[1].as<int>();
        int quota = quotas[item];
        
        summary.used[item] = used;
        summary.quota[item] = quota;
        
        if (used > quota) {
            int overage = used - quota;
            double cost = overage * get_overage_price(item);
            summary.overage_cost[item] = cost;
            summary.total_cost += cost;
        }
    }
    
    return summary;
}
```

---

## 四、Admin Dashboard —— 管理后台

### 4.1 REST API 设计

```cpp
// src/admin/api.hpp

namespace smartsupport::admin {

// 平台管理 API（仅超级管理员）
class AdminAPI {
public:
    // 租户管理
    void register_routes(Router& router);
    
private:
    // GET /api/admin/tenants - 租户列表
    void list_tenants(Request& req, Response& res);
    
    // POST /api/admin/tenants - 创建租户
    void create_tenant(Request& req, Response& res);
    
    // GET /api/admin/tenants/:id - 租户详情
    void get_tenant(Request& req, Response& res);
    
    // PUT /api/admin/tenants/:id - 更新租户
    void update_tenant(Request& req, Response& res);
    
    // GET /api/admin/stats - 平台统计
    void get_platform_stats(Request& req, Response& res);
    
    // GET /api/admin/usage - 用量统计
    void get_usage_report(Request& req, Response& res);
};

// 租户管理 API（租户管理员）
class TenantAdminAPI {
public:
    void register_routes(Router& router);
    
private:
    // GET /api/tenant/config - 获取配置
    void get_config(Request& req, Response& res);
    
    // PUT /api/tenant/config - 更新配置
    void update_config(Request& req, Response& res);
    
    // GET /api/tenant/members - 成员列表
    void list_members(Request& req, Response& res);
    
    // POST /api/tenant/members - 添加成员
    void add_member(Request& req, Response& res);
    
    // GET /api/tenant/stats - 租户统计
    void get_tenant_stats(Request& req, Response& res);
    
    // GET /api/tenant/conversations - 会话记录
    void list_conversations(Request& req, Response& res);
    
    // GET /api/tenant/knowledge - 知识库管理
    void manage_knowledge(Request& req, Response& res);
};

// 客服工作台 API（人工客服）
class AgentAPI {
public:
    void register_routes(Router& router);
    
private:
    // WebSocket /ws/agent - 客服工作台实时连接
    void handle_websocket(WebSocket& ws);
    
    // POST /api/agent/sessions/:id/take - 接管会话
    void take_session(Request& req, Response& res);
    
    // POST /api/agent/sessions/:id/message - 发送消息
    void send_message(Request& req, Response& res);
    
    // POST /api/agent/sessions/:id/close - 结束会话
    void close_session(Request& req, Response& res);
    
    // POST /api/agent/status - 更新状态
    void update_status(Request& req, Response& res);
};

} // namespace
```

### 4.2 关键 API 实现示例

```cpp
// src/admin/api.cpp

void AdminAPI::get_platform_stats(Request& req, Response& res) {
    // 验证超级管理员权限
    if (!is_super_admin(req)) {
        res.status = 403;
        res.set_json({{"error", "Forbidden"}});
        return;
    }
    
    // 统计指标
    auto stats = db_.query(R"(
        SELECT 
            (SELECT COUNT(*) FROM tenants WHERE status = 'active') as active_tenants,
            (SELECT COUNT(*) FROM conversations WHERE created_at >= NOW() - INTERVAL '24 hours') as conversations_24h,
            (SELECT COUNT(*) FROM messages WHERE created_at >= NOW() - INTERVAL '24 hours') as messages_24h,
            (SELECT SUM(amount) FROM invoices WHERE period = TO_CHAR(NOW(), 'YYYY-MM') AND paid = true) as revenue_month
    )");
    
    json response = {
        {"active_tenants", stats[0][0].as<int>()},
        {"conversations_24h", stats[0][1].as<int>()},
        {"messages_24h", stats[0][2].as<int>()},
        {"revenue_month", stats[0][3].as<double>()}
    };
    
    res.set_json(response);
}

void TenantAdminAPI::get_tenant_stats(Request& req, Response& res) {
    // 从 JWT 中获取当前租户 ID
    auto tenant_id = req.context<std::string>("tenant_id");
    
    // 设置租户上下文
    TenantContext::set_current(tenant_id);
    
    auto today = date::format("%F", std::chrono::system_clock::now());
    
    auto stats = db_.query(R"(
        SELECT 
            (SELECT COUNT(*) FROM conversations WHERE tenant_id = $1 AND DATE(created_at) = $2) as conversations_today,
            (SELECT COUNT(*) FROM messages WHERE tenant_id = $1 AND DATE(created_at) = $2 AND sender_type = 'user') as user_messages_today,
            (SELECT AVG(confidence) FROM messages WHERE tenant_id = $1 AND DATE(created_at) = $2 AND sender_type = 'ai') as avg_confidence,
            (SELECT COUNT(*) FROM human_sessions WHERE tenant_id = $1 AND DATE(created_at) = $2) as human_escalations_today
    )", tenant_id, today);
    
    res.set_json({
        {"conversations_today", stats[0][0].as<int>()},
        {"user_messages_today", stats[0][1].as<int>()},
        {"avg_confidence", stats[0][2].as<double>()},
        {"human_escalations_today", stats[0][3].as<int>()}
    });
    
    TenantContext::clear();
}
```

---

## 五、服务集成

### 5.1 更新主程序

```cpp
// src/main.cpp（更新版）

int main(int argc, char* argv[]) {
    // ... 初始化代码 ...
    
    // 初始化所有服务
    auto db = std::make_shared<Database>(config.database);
    auto tenant_db = std::make_shared<tenant::TenantDatabase>(db);
    
    // 基础服务
    auto knowledge_service = std::make_shared<knowledge::KnowledgeService>(...);
    auto ai_service = std::make_shared<ai::AIService>(...);
    auto chat_service = std::make_shared<chat::ChatService>(...);
    
    // 新增服务
    auto human_service = std::make_shared<human::HumanService>(io);
    auto tenant_service = std::make_shared<tenant::TenantService>(tenant_db);
    auto billing_service = std::make_shared<billing::BillingService>(db);
    
    // 注册服务间依赖
    chat_service->set_human_service(human_service);
    chat_service->set_tenant_service(tenant_service);
    
    // 注册管理 API
    admin::AdminAPI admin_api;
    admin_api.register_routes(router);
    
    admin::TenantAdminAPI tenant_admin_api;
    tenant_admin_api.register_routes(router);
    
    admin::AgentAPI agent_api;
    agent_api.register_routes(router);
    
    // ... 启动服务器 ...
}
```

---

## 六、本章小结

**核心收获：**

1. **Human Service**：
   - 人机协作流程
   - 智能分配算法
   - 客服工作台

2. **Tenant Service**：
   - 数据隔离（RLS + 上下文）
   - 资源配额管理
   - 成员权限

3. **Billing Service**：
   - 用量采集与聚合
   - 套餐与超额计费
   - 账单生成

4. **Admin Dashboard**：
   - REST API 设计
   - 平台级与租户级管理
   - 运营统计

---

## 七、引出的问题

### 7.1 部署问题

所有功能已经实现，需要：

```
• Docker 容器化
• K8s 部署配置
• 监控告警
• 性能优化
```

---

**下一章预告（Step 20）：**

我们将完成生产部署：
- Dockerfile 多阶段构建
- K8s Deployment/Service/Ingress 配置
- Prometheus/Grafana 监控
- 性能压测与优化

从代码到上线，完成最后一公里。
