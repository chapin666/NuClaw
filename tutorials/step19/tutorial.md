# Step 19: 高级功能 —— 人机协作与运营管理

> 目标：在 Step 18 基础上，添加人工客服、租户管理、计费系统
> 难度：⭐⭐⭐⭐
> 代码量：约 1800 行（较 Step 18 新增 600 行）

---

## 问题引入

### Step 18 的局限

Step 18 实现了三个核心服务，但作为 SaaS 平台还缺少关键功能：

1. **没有人机协作** - AI 无法处理复杂问题时，如何转接人工客服？
2. **没有租户管理 API** - 如何创建租户、升级套餐、暂停服务？
3. **没有计费系统** - 如何统计用量、计算费用、配额告警？

### 本章目标

**添加三个高级服务**：
- **HumanService**：人工客服转接、会话接管
- **TenantService**：租户 CRUD、套餐管理
- **BillingService**：用量统计、实时计费、配额告警

---

## 解决方案

### 服务架构演进

```
Step 18（核心服务）:
┌─────────────────────────────────────────────────────────────┐
│  ChatService → AIService → KnowledgeService                  │
└─────────────────────────────────────────────────────────────┘

Step 19（+ 高级服务）:
┌─────────────────────────────────────────────────────────────┐
│                                                             │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────┐ │
│  │  HumanService   │  │  TenantService  │  │BillingService│ │
│  │  人工客服        │  │  租户管理        │  │  计费系统    │ │
│  └────────┬────────┘  └────────┬────────┘  └──────┬──────┘ │
│           │                    │                   │        │
│           └────────────────────┴───────────────────┘        │
│                              │                              │
│           ┌──────────────────┴──────────────────┐          │
│           ▼                                      ▼          │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────┐ │
│  │  ChatService    │  │  AIService      │  │KnowledgeService│
│  │  （Step 18）     │  │  （Step 18）     │  │ （Step 18）   │
│  └─────────────────┘  └─────────────────┘  └─────────────┘ │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 代码对比

#### Step 18 的代码（无高级功能）

```cpp
// Step 18: 只有核心服务
auto chat = std::make_shared<ChatService>(io, ai);
auto ai = std::make_shared<AIService>(io, llm, kb);
auto kb = std::make_shared<KnowledgeService>(config);

// 缺少：
// - 人工客服管理
// - 租户管理 API
// - 计费统计
```

#### Step 19 的修改（添加高级服务）

```cpp
// Step 19: 6 个服务协同工作
// 核心服务（Step 18）
auto kb = std::make_shared<KnowledgeService>(kb_config);
auto ai = std::make_shared<AIService>(io, llm_config, kb);
auto chat = std::make_shared<ChatService>(io, ai);

// 高级服务（Step 19 新增）
auto human = std::make_shared<HumanService>();
auto tenant = std::make_shared<TenantService>();
auto billing = std::make_shared<BillingService>();

// 创建租户
tenant->create_tenant("科技公司", "tech@example.com", TenantPlan::PRO);

// 注册人工客服
human->register_agent(tenant_id, "agent_001", "张客服");
human->set_agent_status("agent_001", HumanAgentStatus::ONLINE);

// 记录用量
billing->record_ai_message(tenant_id, 500, 150);
double cost = billing->calculate_cost(tenant_id);
```

### 新增服务详解

#### 1. TenantService - 租户管理

```cpp
// include/nuclaw/services/tenant_service.hpp
class TenantService {
public:
    // 租户 CRUD
    std::string create_tenant(const std::string& name,
                               const std::string& email,
                               TenantPlan plan);
    bool delete_tenant(const std::string& tenant_id);
    
    // 套餐管理（免费/基础/专业/企业）
    bool upgrade_plan(const std::string& tenant_id, TenantPlan new_plan);
    
    // 配额管理
    TenantQuota get_quota(const std::string& tenant_id);
    
    // 状态管理（激活/暂停）
    bool suspend_tenant(const std::string& tenant_id, const std::string& reason);
    bool resume_tenant(const std::string& tenant_id);
    
private:
    std::map<std::string, std::unique_ptr<TenantInfo>> tenants_;
};
```

**套餐配额对比：**

| 功能 | 免费版 | 基础版 | 专业版 | 企业版 |
|:---|:---:|:---:|:---:|:---:|
| 最大并发会话 | 10 | 50 | 200 | 1000 |
| 每月消息数 | 1,000 | 5,000 | 20,000 | 100,000 |
| 知识库文档 | 5 | 20 | 100 | 1000 |
| 存储空间(MB) | 10 | 50 | 500 | 5000 |
| 人工客服数 | 0 | 2 | 10 | 50 |
| 价格 | ¥0 | ¥99 | ¥499 | 定制 |

#### 2. HumanService - 人工客服

```cpp
// include/nuclaw/services/human_service.hpp
class HumanService {
public:
    // 客服管理
    void register_agent(const std::string& tenant_id,
                        const std::string& agent_id,
                        const std::string& name);
    void set_agent_status(const std::string& agent_id, 
                          HumanAgentStatus status);
    
    // 转接队列
    std::string request_escalation(const std::string& session_id,
                                    const std::string& tenant_id,
                                    const std::string& reason,
                                    int priority);
    
    // 会话接管
    bool handover_to_human(const std::string& session_id,
                           const std::string& agent_id);
    bool return_to_ai(const std::string& session_id);
    
    // 查询状态
    bool is_handled_by_human(const std::string& session_id);
};
```

**人机协作流程：**

```
客户消息 → AI 处理 → 复杂问题？ → 是 → 加入转接队列
                              ↓
                            否
                              ↓
                        AI 直接回复

转接队列 → 客服上线 → 自动分配 → 人工处理 → 问题解决？
                                            ↓
                                          是 → 交回 AI
```

#### 3. BillingService - 计费系统

```cpp
// include/nuclaw/services/billing_service.hpp
class BillingService {
public:
    // 用量记录
    void record_ai_message(const std::string& tenant_id,
                           int input_tokens, int output_tokens);
    void record_human_message(const std::string& tenant_id);
    void record_knowledge_document(const std::string& tenant_id,
                                   int doc_size_mb);
    
    // 费用计算
    double calculate_cost(const std::string& tenant_id);
    
    // 配额检查
    QuotaCheck check_message_quota(const std::string& tenant_id,
                                    int current, int max);
    
    // 告警管理
    std::vector<BillingAlert> get_alerts(const std::string& tenant_id);
};
```

**计费模型：**

| 项目 | 单价 |
|:---|:---|
| AI 输入 Token | $0.0015/1K tokens |
| AI 输出 Token | $0.002/1K tokens |
| AI 消息（固定）| $0.01/条 |
| 人工客服消息 | $0.05/条 |
| 存储 | $0.1/GB/月 |

### 文件变更清单

| 文件 | 变更类型 | 说明 |
|:---|:---|:---|
| `include/nuclaw/services/chat_service.hpp` | 复用 | Step 18 |
| `include/nuclaw/services/ai_service.hpp` | 复用 | Step 18 |
| `include/nuclaw/services/knowledge_service.hpp` | 复用 | Step 18 |
| `include/nuclaw/services/human_service.hpp` | **新增** | 人工客服服务 |
| `include/nuclaw/services/tenant_service.hpp` | **新增** | 租户管理服务 |
| `include/nuclaw/services/billing_service.hpp` | **新增** | 计费服务 |
| `src/main.cpp` | **修改** | 演示 6 个服务协同 |

---

## 完整源码

### 目录结构

```
src/step19/
├── CMakeLists.txt
├── include/
│   └── nuclaw/
│       └── services/
│           ├── chat_service.hpp          # 复用 Step 18
│           ├── ai_service.hpp            # 复用 Step 18
│           ├── knowledge_service.hpp     # 复用 Step 18
│           ├── human_service.hpp         # 新增
│           ├── tenant_service.hpp        # 新增
│           └── billing_service.hpp       # 新增
└── src/
    └── main.cpp                          # 演示
```

### 服务初始化流程

```cpp
// main.cpp 核心逻辑

// ============================================================
// 初始化 6 个服务
// ============================================================

// 核心服务（Step 18）
auto knowledge_service = std::make_shared<KnowledgeService>(kb_config);
auto ai_service = std::make_shared<AIService>(io, llm_config, knowledge_service);
auto chat_service = std::make_shared<ChatService>(io, ai_service);

// 高级服务（Step 19 新增）
auto human_service = std::make_shared<HumanService>();
auto tenant_service = std::make_shared<TenantService>();
auto billing_service = std::make_shared<BillingService>();

// ============================================================
// 演示 1：租户管理
// ============================================================

// 创建不同套餐的租户
std::string tenant_free = tenant_service->create_tenant(
    "小商店", "shop@example.com", TenantPlan::FREE);

std::string tenant_pro = tenant_service->create_tenant(
    "科技公司", "tech@example.com", TenantPlan::PRO);

// 查看配额
auto quota = tenant_service->get_quota(tenant_pro);
std::cout << "专业版配额: 并发=" << quota.max_concurrent_sessions << "\n";

// ============================================================
// 演示 2：人工客服
// ============================================================

// 注册客服
human_service->register_agent(tenant_pro, "agent_001", "张客服");
human_service->set_agent_status("agent_001", HumanAgentStatus::ONLINE);

// 客户要求转人工
std::string request = human_service->request_escalation(
    "session_001", tenant_pro, "user_001", "投诉问题", 8);

// 客服接管会话
human_service->handover_to_human("session_001", "agent_001");

// 问题解决，交回 AI
human_service->return_to_ai("session_001");

// ============================================================
// 演示 3：计费系统
// ============================================================

// 记录用量
billing_service->record_ai_message(tenant_pro, 500, 150);
billing_service->record_human_message(tenant_pro);
billing_service->record_knowledge_document(tenant_pro, 10);

// 计算费用
double cost = billing_service->calculate_cost(tenant_pro);
std::cout << "本期费用: $" << cost << "\n";

// 配额检查
auto check = billing_service->check_message_quota(
    tenant_pro, 1500, quota.max_messages_per_month);
if (!check.allowed) {
    std::cout << "警告: " << check.reason << "\n";
}
```

---

## 编译运行

```bash
# 进入 Step 19 目录
cd src/step19

# 创建构建目录
mkdir build && cd build

# 配置
cmake ..

# 编译
make -j

# 运行
./step19_demo
```

**预期输出：**

```
========================================
Step 19: 高级功能
========================================
演进：Step 18 核心服务 + 3 个高级服务

【演示 1】租户管理
----------------------------------------
[TenantService] 创建租户: 小商店 (tenant_1)
[TenantService] 创建租户: 科技公司 (tenant_2)
[TenantService] 创建租户: 大型企业 (tenant_3)

租户配额对比:
  免费版: 并发=10 消息/月=1000 人工客服=0
  专业版: 并发=200 消息/月=20000 人工客服=10
  企业版: 并发=1000 消息/月=100000 人工客服=50

【演示 2】人工客服管理
----------------------------------------
[HumanService] 注册人工客服: 张客服 (租户: tenant_2)
[HumanService] 客服 张客服 状态更新: 在线

客户A: 我要投诉！转人工！
[HumanService] 转接请求创建: esc_1
张客服: 您好，我是客服小张
[HumanService] 会话接管: session_001 → 客服 张客服
[HumanService] 会话交回 AI: session_001

【演示 3】计费与配额
----------------------------------------
模拟租户使用...

配额检查:
  专业版消息配额: ✅ 正常

[租户 tenant_2 用量报告]
  消息: AI=100 人工=0
  Token: 输入=50000 输出=15000
  本期费用: $0.105

【演示 4】综合统计
----------------------------------------

[TenantService 统计]
  总租户: 3
  套餐分布: 免费=1 基础=0 专业=1 企业=1

[HumanService 统计]
  注册客服: 3
  状态分布: 在线=2 繁忙=0 离开=1 离线=0

[BillingService 统计]
  监控租户数: 2
  总消息数: 620
  预估收入: $1.24
```

---

## 本章总结

- ✅ **完成了 SaaS 平台的三大支柱功能**：
  - **TenantService**：多租户管理、套餐体系、配额控制
  - **HumanService**：人机协作、客服工作台、转接队列
  - **BillingService**：用量统计、实时计费、配额告警

- ✅ **实现了完整的商业闭环**：
  - 免费版获客 → 专业版付费 → 企业版定制
  - AI 降本 → 人工增值 → 按量计费

- ✅ **保持了架构清晰**：
  - 6 个服务职责单一、依赖清晰
  - 高级服务可独立演进

---

## 课后思考

当前系统已经可以商业运营，但还缺少什么？

<details>
<summary>点击查看下一章要解决的问题 💡</summary>

**问题 1：如何部署到生产环境？**

现在有 6 个服务，手动启动管理困难。如何用 Docker Compose 编排？

**问题 2：如何水平扩展？**

用户量增长时，如何扩展 ChatService 和 AIService？如何用 K8s 管理？

**问题 3：如何监控和告警？**

服务健康状态、性能指标、业务数据如何收集和展示？

**Step 20 预告：生产部署**

我们将完成最后一公里：

- **Docker Compose**：多服务容器编排
- **K8s 部署**：生产级水平扩展
- **监控日志**：Prometheus + Grafana + ELK

</details>
