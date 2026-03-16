# Step 18: 核心服务拆分 —— 从单体到微服务

> 目标：将 Step 17 的 CustomerServiceAgent 拆分为三个独立服务
> 难度：⭐⭐⭐⭐
> 代码量：约 1500 行（较 Step 17 新增/拆分 300 行）

---

## 问题引入

### Step 17 的局限

Step 17 的 `CustomerServiceAgent` 功能完整，但所有逻辑都在一个类里：

```cpp
// Step 17: 单体设计
class CustomerServiceAgent : public StatefulAgent {
    // 这一个人类负责：
    // - WebSocket 连接管理
    // - 会话状态维护
    // - LLM 调用和回复生成
    // - 知识库检索
    // - 情感/记忆/关系管理
};
```

**问题：**
1. **无法独立扩展** - 如果 WebSocket 连接数增加，必须整体扩容
2. **技术栈耦合** - 向量数据库升级会影响整个 Agent
3. **团队协作困难** - 所有人修改同一个文件，容易冲突
4. **单点故障** - 一个 Bug 可能导致整个服务崩溃

### 本章目标

**服务拆分**：将单体 Agent 拆分为三个独立服务，每个服务职责单一。

---

## 解决方案

### 服务拆分设计

```
Step 17（单体）:
┌─────────────────────────────────────────────────┐
│         CustomerServiceAgent                     │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐        │
│  │ 连接管理  │ │ LLM 调用 │ │ 知识检索 │        │
│  └──────────┘ └──────────┘ └──────────┘        │
└─────────────────────────────────────────────────┘

Step 18（微服务）:
┌─────────────────────────────────────────────────────────────┐
│                                                             │
│  ┌──────────────────┐                                       │
│  │   ChatService    │ ← 负责：WebSocket、会话管理            │
│  │                  │                                       │
│  │ 连接管理、消息路由 │                                       │
│  └────────┬─────────┘                                       │
│           │ 调用                                             │
│           ▼                                                  │
│  ┌──────────────────┐                                       │
│  │   AIService      │ ← 负责：LLM、智能回复、情感记忆        │
│  │                  │                                       │
│  │ 复用 CustomerServiceAgent                                 │
│  └────────┬─────────┘                                       │
│           │ 调用                                             │
│           ▼                                                  │
│  ┌──────────────────┐                                       │
│  │ KnowledgeService │ ← 负责：向量检索、知识库管理           │
│  │                  │                                       │
│  │ 复用 Step 10 VectorStore                                  │
│  └──────────────────┘                                       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 代码对比

#### Step 17 的代码（单体）

```cpp
// Step 17: 单体 Agent 处理所有逻辑
CustomerServiceAgent agent(tenant);
std::string response = agent.handle_customer_message(user_id, user_name, message);
// 内部：连接管理 + LLM 调用 + 知识检索 + 情感计算
```

#### Step 18 的修改（服务拆分）

```cpp
// Step 18: 分层调用

// 1. ChatService 管理连接
std::string conn_id = chat_service->connect_customer(tenant_id, user_id, user_name);

// 2. 处理消息（ChatService 内部委托给 AIService）
std::string response = chat_service->handle_message(conn_id, message);

// 3. AIService 内部调用 KnowledgeService 做 RAG
// 4. 最终生成回复
```

### 服务职责划分

| 服务 | 职责 | 复用来源 |
|:---|:---|:---|
| **ChatService** | WebSocket 连接管理、会话生命周期、消息路由 | 新增 |
| **AIService** | LLM 调用、智能回复生成、情感/记忆/关系管理 | Step 17 CustomerServiceAgent |
| **KnowledgeService** | 向量数据库、文档索引、RAG 检索 | Step 10 VectorStore |

### 新增代码详解

#### 1. ChatService - 会话管理服务

```cpp
// include/nuclaw/services/chat_service.hpp
class ChatService {
public:
    // 客户连接（模拟 WebSocket 连接建立）
    std::string connect_customer(const std::string& tenant_id,
                                  const std::string& user_id,
                                  const std::string& user_name);
    
    // 处理消息（核心：委托给 AIService）
    std::string handle_message(const std::string& connection_id,
                                const std::string& message);
    
private:
    std::shared_ptr<AIService> ai_service_;  // 依赖注入
    std::map<std::string, std::unique_ptr<WebSocketConnection>> connections_;
};
```

#### 2. AIService - AI 智能回复服务

```cpp
// include/nuclaw/services/ai_service.hpp
class AIService {
public:
    // 生成智能回复（复用 CustomerServiceAgent）
    std::string generate_response(const std::string& tenant_id,
                                   const std::string& user_id,
                                   const std::string& message,
                                   const std::vector<... >& history);
    
    // 注册租户 Agent（每个租户一个 CustomerServiceAgent 实例）
    void register_tenant_agent(const TenantContext& tenant);

private:
    // 复用 Step 17：每个租户一个 CustomerServiceAgent
    std::map<std::string, std::unique_ptr<CustomerServiceAgent>> tenant_agents_;
    std::shared_ptr<KnowledgeService> knowledge_service_;
};
```

#### 3. KnowledgeService - 知识库服务

```cpp
// include/nuclaw/services/knowledge_service.hpp
class KnowledgeService {
public:
    // 初始化租户知识库（数据隔离）
    void initialize_tenant_kb(const std::string& tenant_id);
    
    // 添加文档
    void add_document(const std::string& tenant_id,
                      const std::string& doc_id,
                      const std::string& content);
    
    // RAG 检索
    std::vector<RetrievalResult> retrieve(const std::string& tenant_id,
                                           const std::string& query,
                                           int top_k = 5);

private:
    // 复用 Step 10：每个租户一个 VectorStore
    std::map<std::string, std::unique_ptr<VectorStore>> tenant_stores_;
};
```

### 文件变更清单

| 文件 | 变更类型 | 说明 |
|:---|:---|:---|
| `include/nuclaw/customer_service_agent.hpp` | 复用 | 从 Step 17 复用，无修改 |
| `include/nuclaw/tenant.hpp` | 复用 | 从 Step 17 复用，无修改 |
| `include/nuclaw/vector_store.hpp` | 复用 | 从 Step 10 复用，无修改 |
| `include/nuclaw/services/chat_service.hpp` | **新增** | 会话管理服务 |
| `include/nuclaw/services/ai_service.hpp` | **新增** | AI 智能回复服务 |
| `include/nuclaw/services/knowledge_service.hpp` | **新增** | 知识库服务 |
| `src/main.cpp` | **修改** | 演示服务拆分效果 |

---

## 完整源码

### 目录结构

```
src/step18/
├── CMakeLists.txt
├── include/
│   └── nuclaw/
│       ├── customer_service_agent.hpp      # 复用 Step 17
│       ├── tenant.hpp                      # 复用 Step 17
│       ├── vector_store.hpp                # 复用 Step 10
│       └── services/
│           ├── chat_service.hpp            # 新增
│           ├── ai_service.hpp              # 新增
│           └── knowledge_service.hpp       # 新增
└── src/
    └── main.cpp                            # 演示
```

### 服务初始化流程

```cpp
// main.cpp 核心逻辑

// 1. 初始化 KnowledgeService（最底层，无依赖）
auto knowledge_service = std::make_shared<KnowledgeService>(kb_config);

// 2. 初始化 AIService（依赖 KnowledgeService）
auto ai_service = std::make_shared<AIService>(io, llm_config, knowledge_service);

// 3. 初始化 ChatService（依赖 AIService）
auto chat_service = std::make_shared<ChatService>(io, ai_service);

// 4. 注册租户
tenant_a.agent_persona = {{"name", "小快"}};
knowledge_service->initialize_tenant_kb(tenant_a.tenant_id);
ai_service->register_tenant_agent(tenant_a);

// 5. 添加知识库文档
knowledge_service->add_document(tenant_a.tenant_id, "faq_shipping", "...");

// 6. 处理客户连接
std::string conn_id = chat_service->connect_customer(tenant_id, user_id, user_name);
std::string response = chat_service->handle_message(conn_id, message);
```

---

## 编译运行

```bash
# 进入 Step 18 目录
cd src/step18

# 创建构建目录
mkdir build && cd build

# 配置
cmake ..

# 编译
make -j

# 运行
./step18_demo
```

**预期输出：**

```
========================================
Step 18: 核心服务拆分
========================================
演进：Step 17 单体 Agent → 3 个微服务

【初始化】KnowledgeService
【初始化】AIService
【初始化】ChatService

服务初始化完成！

【演示 1】初始化租户数据
----------------------------------------
[KnowledgeService] 初始化知识库: tenant_ecommerce
[初始化] 租户: 快购商城 (tenant_ecommerce)
[AIService] 注册租户 Agent: 快购商城
...

【演示 2】客户连接和对话流程
----------------------------------------
[ChatService] 客户连接: conn_1 (租户: tenant_ecommerce, 用户: 小明)
小明: 你好，请问发货要多久？
小快: [模拟回复] 收到消息: 你好，请问发货要多久？

【演示 4】服务统计
----------------------------------------
[ChatService 统计]
  活跃连接: 2
  总会话: 2

[KnowledgeService 统计]
  租户 tenant_ecommerce: 2 文档
  租户 tenant_edu: 1 文档
```

---

## 本章总结

- ✅ **解决了 Step 17 的问题**：单体架构无法独立扩展
- ✅ **实现了服务拆分**：ChatService + AIService + KnowledgeService
- ✅ **保持了演进连续性**：复用 CustomerServiceAgent 和 VectorStore
- ✅ **实现了职责分离**：每个服务可以独立开发、部署、扩展
- ✅ **保持了多租户隔离**：每个服务都支持租户隔离

---

## 课后思考

当前实现还有什么问题？

<details>
<summary>点击查看下一章要解决的问题 💡</summary>

**问题 1：缺少人工客服**

现在的 `should_escalate_to_human()` 只是判断，没有真正转接人工客服的功能。如何实现完整的人机协作？

**问题 2：缺少租户管理 API**

租户创建、配置修改、状态查询都需要直接操作数据库。如何提供 REST API 管理租户？

**问题 3：缺少计费系统**

SaaS 平台需要按量计费（消息数、Token 数、存储量）。如何实现计费统计和套餐限制？

**Step 19 预告：高级功能**

我们将添加三个高级服务：

- **HumanService**：人工客服工作台、转接队列、会话接管
- **TenantService**：租户管理 REST API、配置管理
- **BillingService**：用量统计、套餐管理、计费告警

</details>
