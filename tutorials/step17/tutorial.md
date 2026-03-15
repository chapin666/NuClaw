# Step 17: 智能客服 SaaS 平台 —— 需求分析与架构设计

> 目标：基于前面所有章节代码，设计一个完整的智能客服 SaaS 平台
> 
003e 难度：⭐⭐⭐⭐⭐ | 代码量：约 2000 行（总计）| 预计学习时间：6-8 小时

---

## 一、项目概述

### 1.1 为什么选智能客服 SaaS？

这是一个**综合运用**前面所有章节知识的大型项目：

```
需要的知识（对应章节）：
├── 网络通信 (Step 0-2)
├── WebSocket 实时对话 (Step 4)
├── LLM 接入 (Step 5)
├── 工具调用 (Step 6-9)
├── RAG 知识库 (Step 10)
├── 多 Agent 协作 (Step 11)
├── 配置管理 (Step 12)
├── 监控告警 (Step 13)
├── 部署运维 (Step 14)
├── IM 平台接入 (Step 15)
└── 状态与记忆 (Step 16)

商业价值：
• 真实需求：每家企业都需要客服
• 可落地：从 MVP 到 SaaS 产品
• 可扩展：支持多租户、多场景
```

### 1.2 产品定位

**SmartSupport** —— 智能客服 SaaS 平台

```
目标用户：
├── 中小企业（没有技术团队）
├── 电商平台（咨询量大）
├── 教育机构（招生咨询）
└── 其他需要客服的企业

核心价值：
• 7×24 小时 AI 自动回复
• 降低 80% 人工客服成本
• 多平台统一接入（网站/微信/飞书）
• 零代码配置知识库
```

---

## 二、需求分析

### 2.1 功能需求

```
┌─────────────────────────────────────────────────────────────────────┐
│                         功能需求全景                                 │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  租户侧功能（企业管理员）                                             │
│  ┌────────────────────────────────────────────────────────────────┐ │
│  │ • 知识库管理：上传文档、FAQ、自动向量化                          │ │
│  │ • 客服配置：欢迎语、转人工规则、工作时间                         │ │
│  │ • 渠道接入：网站挂件、微信公众号、飞书 Bot                       │ │
│  │ • 人工客服：工作台、会话接管、快捷回复                           │ │
│  │ • 数据分析：会话量、满意度、热点问题                             │ │
│  │ • 成员管理：客服账号、权限分配                                   │ │
│  └────────────────────────────────────────────────────────────────┘ │
│                                                                      │
│  平台侧功能（SaaS 运营商）                                            │
│  ┌────────────────────────────────────────────────────────────────┐ │
│  │ • 租户管理：开通、套餐、计费                                     │ │
│  │ • 系统监控：全局健康度、资源使用                                 │ │
│  │ • 运营分析：租户活跃度、收入统计                                 │ │
│  │ • 系统配置：LLM 模型、全局参数                                   │ │
│  └────────────────────────────────────────────────────────────────┘ │
│                                                                      │
│  终端用户功能（消费者）                                               │
│  ┌────────────────────────────────────────────────────────────────┐ │
│  │ • 多渠道接入：网页、H5、微信、飞书                               │ │
│  │ • 智能对话：7×24 自动回复、多轮交互                              │ │
│  │ • 人工转接：复杂问题一键转人工                                   │ │
│  │ • 满意度评价：服务打分、反馈                                     │ │
│  └────────────────────────────────────────────────────────────────┘ │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

### 2.2 技术需求

| 需求 | 说明 | 对应章节 |
|:---|:---|:---:|
| 多租户隔离 | 数据、配置、资源完全隔离 | Step 12, 16 |
| 高并发 | 支持 1000+ 并发会话 | Step 1-2, 7 |
| 水平扩展 | 无状态设计，支持扩容 | Step 14 |
| 实时通信 | WebSocket 长连接 | Step 4 |
| 知识检索 | RAG + 向量数据库 | Step 10 |
| 工具调用 | 查订单、预约等 API | Step 6-9 |
| 人机协作 | AI 无法处理时转人工 | Step 11 |
| 全链路监控 | Metrics + Logs + Traces | Step 13 |

### 2.3 非功能需求

```
性能：
• 首屏响应 < 2s
• 消息回复 < 3s（AI）
• 支持 10k 并发连接

可用性：
• SLA 99.9%
• 自动故障恢复
• 数据多副本备份

安全：
• 数据加密存储
• 传输 TLS 1.3
• API 访问控制
• 审计日志
```

---

## 三、系统架构设计

### 3.1 整体架构

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           SmartSupport 架构                                  │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │                          接入层 (Gateway)                            │   │
│   │  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐       │   │
│   │  │  Web    │ │WebSocket│ │ WeChat  │ │ Feishu  │ │  Ding   │       │   │
│   │  │  HTTP   │ │  Realtime│ │   Bot   │ │   Bot   │ │   Bot   │       │   │
│   │  └────┬────┘ └────┬────┘ └────┬────┘ └────┬────┘ └────┬────┘       │   │
│   │       └───────────┴───────────┴───────────┴───────────┘              │   │
│   │                          │                                           │   │
│   │                    ┌─────┴─────┐                                     │   │
│   │                    │   Nginx   │  负载均衡 + SSL + 限流                │   │
│   │                    └─────┬─────┘                                     │   │
│   └──────────────────────────┼──────────────────────────────────────────┘   │
│                              │                                               │
│                              ▼                                               │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │                          服务层 (Services)                           │   │
│   │                                                                      │   │
│   │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐│   │
│   │  │   Chat      │  │   AI        │  │  Knowledge  │  │   Human     ││   │
│   │  │   Service   │  │   Service   │  │   Service   │  │   Service   ││   │
│   │  │             │  │             │  │             │  │             ││   │
│   │  │• 会话管理   │  │• LLM调用   │  │• RAG检索   │  │• 人工队列   ││   │
│   │  │• 消息路由   │  │• Agent循环 │  │• 文档处理   │  │• 客服工作台 ││   │
│   │  │• 状态保持   │  │• 工具执行   │  │• 向量索引   │  │• 会话接管   ││   │
│   │  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘│   │
│   │         │                │                │                │       │   │
│   │         └────────────────┴────────────────┴────────────────┘       │   │
│   │                          │                                         │   │
│   │                    ┌─────┴─────┐                                   │   │
│   │                    │  Message  │  服务间通信（RPC/Event）           │   │
│   │                    │   Bus     │                                   │   │
│   │                    └───────────┘                                   │   │
│   │                                                                     │   │
│   │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐                 │   │
│   │  │   Tenant    │  │   Billing   │  │   Admin     │                 │   │
│   │  │   Service   │  │   Service   │  │   Service   │                 │   │
│   │  │             │  │             │  │             │                 │   │
│   │  │• 租户管理   │  │• 套餐计费   │  │• 平台管理   │                 │   │
│   │  │• 权限控制   │  │• 用量统计   │  │• 监控告警   │                 │   │
│   │  │• 配置隔离   │  │• 发票生成   │  │• 运营分析   │                 │   │
│   │  └─────────────┘  └─────────────┘  └─────────────┘                 │   │
│   │                                                                     │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                              │                                               │
│                              ▼                                               │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │                          数据层 (Data)                               │   │
│   │                                                                      │   │
│   │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐│   │
│   │  │  PostgreSQL │  │   Redis     │  │ Chroma/     │  │    MinIO    ││   │
│   │  │  (主数据库)  │  │  (缓存/会话) │  │ pgvector    │  │  (文件存储)  ││   │
│   │  │             │  │             │  │ (向量数据库) │  │             ││   │
│   │  │• 租户数据   │  │• 会话状态   │  │• 知识向量   │  │• 上传文档   ││   │
│   │  │• 用户数据   │  │• 分布式锁   │  │• 语义检索   │  │• 备份文件   ││   │
│   │  │• 消息记录   │  │• 热点缓存   │  │             │  │             ││   │
│   │  └─────────────┘  └─────────────┘  └─────────────┘  └─────────────┘│   │
│   │                                                                     │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                                                                              │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │                        基础设施 (Infrastructure)                     │   │
│   │  • Kubernetes 容器编排                                              │   │
│   │  • Prometheus + Grafana 监控                                         │   │
│   │  • ELK/Loki 日志聚合                                                 │   │
│   │  • Jaeger 分布式追踪                                                 │   │
│   │  • CI/CD (GitHub Actions / GitLab CI)                                │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 3.2 核心服务详解

#### 3.2.1 Chat Service（会话服务）

```cpp
// 会话服务职责：
// 1. 管理用户连接（WebSocket）
// 2. 维护会话状态
// 3. 消息路由（AI / 人工）
// 4. 消息持久化

class ChatService {
public:
    // 用户连接接入
    void on_client_connect(const std::string& connection_id,
                           const std::string& tenant_id,
                           const std::string& user_id);
    
    // 接收用户消息
    void on_user_message(const std::string& connection_id,
                         const ChatMessage& message);
    
    // 发送消息给用户
    void send_message(const std::string& connection_id,
                      const ChatMessage& message);

private:
    // 决定消息路由（AI or 人工）
    RouteDecision route_message(const ChatMessage& message);
    
    // 获取或创建会话
    Session& get_or_create_session(const std::string& tenant_id,
                                    const std::string& user_id);
    
    std::map<std::string, std::shared_ptr<Session>> sessions_;
    std::map<std::string, std::shared_ptr<WebSocketConnection>> connections_;
};
```

#### 3.2.2 AI Service（智能服务）

```cpp
// AI 服务职责：
// 1. Agent 核心逻辑
// 2. LLM 调用
// 3. RAG 检索
// 4. 工具执行

class AIService {
public:
    struct AIResponse {
        std::string content;
        bool needs_human;      // 是否需要转人工
        float confidence;      // 置信度
        std::vector<MemoryPtr> used_memories;
    };
    
    // 处理用户消息
    void process_message(const std::string& tenant_id,
                         const std::string& session_id,
                         const std::string& user_input,
                         std::function<void(AIResponse)> callback);

private:
    // 构建个性化 Agent
    Agent& get_tenant_agent(const std::string& tenant_id);
    
    // 租户级知识库
    KnowledgeBase& get_tenant_kb(const std::string& tenant_id);
    
    // 评估是否需要转人工
    bool should_escalate_to_human(const std::string& response,
                                   float confidence);
    
    std::map<std::string, std::unique_ptr<Agent>> tenant_agents_;
    std::map<std::string, std::unique_ptr<KnowledgeBase>> tenant_kbs_;
};
```

#### 3.2.3 Tenant Service（租户服务）

```cpp
// 租户服务职责：
// 1. 租户数据隔离
// 2. 配置管理
// 3. 资源配额

class TenantService {
public:
    struct Tenant {
        std::string id;
        std::string name;
        std::string plan;          // free/pro/enterprise
        std::map<std::string, std::string> config;
        ResourceQuota quota;
        UsageStats usage;
    };
    
    // 获取租户信息
    Tenant get_tenant(const std::string& tenant_id);
    
    // 检查资源配额
    bool check_quota(const std::string& tenant_id,
                     ResourceType type,
                     int amount);
    
    // 记录资源使用
    void record_usage(const std::string& tenant_id,
                      ResourceType type,
                      int amount);

private:
    // 数据隔离：表名加前缀 或 独立 Schema
    std::string get_tenant_table_name(const std::string& tenant_id,
                                      const std::string& table);
};
```

---

## 四、数据模型设计

### 4.1 多租户数据隔离策略

```
策略对比：

1. 独立数据库（Database-per-tenant）
   租户 A: db_tenant_a
   租户 B: db_tenant_b
   
   优点：完全隔离，安全最高
   缺点：成本高，难维护
   适用：大型企业客户

2. 共享数据库，独立 Schema（Schema-per-tenant）
   数据库: smartsupport
   ├── schema_tenant_a
   │   ├── conversations
   │   └── messages
   └── schema_tenant_b
       ├── conversations
       └── messages
   
   优点：平衡隔离和成本
   缺点：Schema 变更复杂
   适用：中型客户

3. 共享数据库，共享 Schema，租户 ID 隔离（ our choice ）
   数据库: smartsupport
   ├── conversations (tenant_id 列)
   └── messages (tenant_id 列)
   
   优点：成本低，易扩展
   缺点：需要严格查询过滤
   适用：SaaS 标准方案

我们的选择：方案 3 + 行级安全策略（RLS）
```

### 4.2 核心表结构

```sql
-- 租户表
CREATE TABLE tenants (
    id UUID PRIMARY KEY,
    name VARCHAR(255) NOT NULL,
    plan VARCHAR(50) DEFAULT 'free',  -- free/pro/enterprise
    config JSONB DEFAULT '{}',
    quota JSONB DEFAULT '{}',
    created_at TIMESTAMP DEFAULT NOW(),
    status VARCHAR(20) DEFAULT 'active'
);

-- 会话表（按 tenant_id 分片）
CREATE TABLE conversations (
    id UUID PRIMARY KEY,
    tenant_id UUID REFERENCES tenants(id),
    user_id VARCHAR(255) NOT NULL,
    channel VARCHAR(50),  -- web/wechat/feishu
    status VARCHAR(20),   -- active/closed/waiting_human
    metadata JSONB,       -- 用户画像、来源页面等
    created_at TIMESTAMP DEFAULT NOW(),
    updated_at TIMESTAMP DEFAULT NOW()
);

-- 消息表（按 tenant_id 分片）
CREATE TABLE messages (
    id UUID PRIMARY KEY,
    tenant_id UUID REFERENCES tenants(id),
    conversation_id UUID REFERENCES conversations(id),
    sender_type VARCHAR(20),  -- user/ai/human
    sender_id VARCHAR(255),
    content TEXT,
    content_type VARCHAR(20), -- text/image/card
    metadata JSONB,           -- AI 置信度、引用知识等
    created_at TIMESTAMP DEFAULT NOW()
);

-- 知识库文档表
CREATE TABLE knowledge_docs (
    id UUID PRIMARY KEY,
    tenant_id UUID REFERENCES tenants(id),
    title VARCHAR(500),
    content TEXT,
    embedding VECTOR(1536),  -- pgvector 扩展
    doc_type VARCHAR(50),    -- faq/product/policy
    metadata JSONB,
    created_at TIMESTAMP DEFAULT NOW()
);

-- 行级安全策略（RLS）
ALTER TABLE conversations ENABLE ROW LEVEL SECURITY;

CREATE POLICY tenant_isolation ON conversations
    USING (tenant_id = current_setting('app.current_tenant')::UUID);
```

---

## 五、技术选型

| 组件 | 选型 | 原因 |
|:---|:---|:---|
| 网关 | Nginx + Lua | 高性能、灵活限流 |
| 服务框架 | 自研（基于 Step 0-16） | 教学用，可控 |
| 数据库 | PostgreSQL 15 + pgvector | 关系 + 向量一体 |
| 缓存 | Redis Cluster | 会话、热点数据 |
| 消息队列 | Redis Stream | 轻量，无需额外组件 |
| 对象存储 | MinIO | 兼容 S3，私有化部署 |
| 监控 | Prometheus + Grafana | 云原生标准 |
| 日志 | Loki | 轻量，K8s 友好 |
| 部署 | Docker Compose / K8s | 渐进式 |

---

## 六、开发计划

```
Step 17（本章）：需求分析与架构设计
├── 完成需求分析 ✅
├── 完成架构设计 ✅
└── 完成技术选型 ✅

Step 18：核心功能实现
├── 搭建项目骨架
├── 实现 Chat Service（会话管理）
├── 实现 AI Service（Agent 核心）
└── 实现 Knowledge Service（RAG）

Step 19：高级功能
├── 实现 Human Service（人工客服）
├── 实现 Tenant Service（多租户）
├── 实现 Billing Service（计费）
└── 实现 Admin Dashboard（管理后台）

Step 20：生产部署
├── Docker 容器化
├── K8s 部署配置
├── 监控告警配置
└── 性能优化
```

---

## 七、本章小结

**核心收获：**

1. **需求分析**：明确 SaaS 平台的功能边界和目标用户
2. **架构设计**：微服务架构，前后端分离，数据隔离
3. **数据模型**：多租户策略选择，核心表结构设计
4. **技术选型**：基于前面章节，选择生产级组件

---

## 八、引出的问题

### 8.1 实现问题

架构已经设计完成，需要开始编码实现：

```
下一步：
• 如何组织大型项目的代码结构？
• 如何实现服务间通信？
• 如何确保数据隔离安全？
```

---

**下一章预告（Step 18）：**

我们将开始实现核心功能：
- 项目骨架搭建（CMake 多模块）
- Chat Service：WebSocket 会话管理
- AI Service：Agent + RAG + 工具
- Knowledge Service：文档处理与检索

从设计到实现，把架构落地为代码。
