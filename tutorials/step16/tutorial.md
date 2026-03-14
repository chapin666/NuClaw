# Step 16: 期中实战 — 智能客服机器人

> 目标：综合运用 Part 1-5 所学，构建完整的智能客服系统
> 
> 难度：⭐⭐⭐⭐ (综合实战)
> 
> 代码量：约 1200 行

## 项目简介

这是一个**完整的智能客服机器人**，可以：

- 🤖 自动回答产品常见问题（基于 RAG 知识库）
- 📞 复杂问题自动创建工单
- 💬 支持飞书/钉钉双平台接入
- 🧠 多轮对话保持上下文
- 📊 记录对话历史，支持人工接管

## 系统架构

```
┌─────────────────────────────────────────────────────────────────────┐
│                         智能客服机器人                              │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐             │
│  │  飞书适配器  │    │  钉钉适配器  │    │  Web 界面   │             │
│  └──────┬──────┘    └──────┬──────┘    └──────┬──────┘             │
│         └──────────────────┼──────────────────┘                     │
│                            ▼                                        │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │                    IM 统一接入层                             │   │
│  └────────────────────────┬────────────────────────────────────┘   │
│                           ▼                                         │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │                    Agent 核心引擎                            │   │
│  │  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐        │   │
│  │  │ Intent  │→│  LLM    │→│ Tool    │→│ Response│        │   │
│  │  │Classifier│  │ Reasoning│  │Executor │  │Formatter│        │   │
│  │  └─────────┘  └─────────┘  └─────────┘  └─────────┘        │   │
│  └────────────────────────┬────────────────────────────────────┘   │
│                           ▼                                         │
│         ┌─────────────────┴─────────────────┐                      │
│         ▼                                   ▼                      │
│  ┌─────────────┐                    ┌─────────────┐                │
│  │  RAG 知识库  │                    │  工单系统   │                │
│  │ (向量检索)   │                    │ (工具调用)  │                │
│  └─────────────┘                    └─────────────┘                │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

## 项目结构

```
src/step16/
├── main.cpp                      # 入口
├── config/
│   ├── config.yaml              # 主配置
│   └── knowledge_base.json      # 知识库文档
├── include/
│   ├── agent_engine.hpp         # Agent 核心
│   ├── im_manager.hpp           # IM 平台管理
│   ├── knowledge_base.hpp       # 知识库检索
│   ├── ticket_system.hpp        # 工单系统
│   └── intent_classifier.hpp    # 意图识别
└── adapters/
    ├── feishu_adapter.hpp
    └── dingtalk_adapter.hpp
```

## 核心代码实现

### 1. 意图分类器

```cpp
class IntentClassifier {
public:
    enum class Intent {
        FAQ_QUERY,           // 常见问题查询
        TICKET_CREATE,       // 创建工单
        TICKET_QUERY,        // 查询工单状态
        HUMAN_HANDOFF,       // 转人工
        CHITCHAT,            // 闲聊
        UNKNOWN
    };
    
    struct Result {
        Intent intent;
        float confidence;
        std::map<std::string, std::string> entities;
    };
    
    // 使用 LLM 进行意图识别
    async::Task<Result> classify(const std::string& message) {
        auto prompt = R"(
分析用户意图，返回 JSON 格式：
{
    "intent": "faq_query|ticket_create|ticket_query|human_handoff|chitchat",
    "confidence": 0.95,
    "entities": {"product": "xxx", "issue": "xxx"}
}

用户消息：)" + message;
        
        auto response = co_await llm_client_.complete(prompt);
        co_return parse_intent_response(response);
    }
};
```

### 2. 知识库检索

```cpp
class CustomerServiceKB {
public:
    // 加载产品文档
    void load_documents(const std::string& path) {
        auto docs = json::parse(read_file(path));
        for (const auto& doc : docs) {
            // 分块并生成 embedding
            auto chunks = split_document(doc["content"], 500);
            for (auto& chunk : chunks) {
                chunk.embedding = co_await embedding_model_.encode(chunk.text);
                chunks_.push_back(std::move(chunk));
            }
        }
        // 构建向量索引
        index_.build(chunks_);
    }
    
    // 检索相关文档
    async::Task<std::vector<Document>> retrieve(
        const std::string& query, 
        int top_k = 3) {
        
        auto query_embedding = co_await embedding_model_.encode(query);
        auto results = index_.search(query_embedding, top_k);
        
        // 过滤低相关度结果
        results.erase(
            std::remove_if(results.begin(), results.end(),
                [](const auto& r) { return r.score < 0.7; }),
            results.end()
        );
        
        co_return results;
    }
};
```

### 3. 工单系统工具

```cpp
class TicketSystem {
public:
    // 创建工单
    json create_ticket(const std::string& user_id,
                       const std::string& description,
                       const std::string& priority) {
        Ticket ticket{
            .id = generate_ticket_id(),
            .user_id = user_id,
            .description = description,
            .priority = priority,
            .status = "open",
            .created_at = now(),
        };
        
        tickets_[ticket.id] = ticket;
        save_to_database(ticket);
        
        // 通知客服团队
        notify_support_team(ticket);
        
        return {
            {"ticket_id", ticket.id},
            {"status", "created"},
            {"estimated_response", get_estimated_response_time(priority)}
        };
    }
    
    // 查询工单状态
    json query_ticket(const std::string& ticket_id) {
        auto it = tickets_.find(ticket_id);
        if (it == tickets_.end()) {
            return {{"error", "工单不存在"}};
        }
        
        const auto& t = it->second;
        return {
            {"ticket_id", t.id},
            {"status", t.status},
            {"created_at", t.created_at},
            {"description", t.description},
            {"agent_notes", t.agent_notes}
        };
    }

private:
    std::unordered_map<std::string, Ticket> tickets_;
};

// 注册为 Agent 工具
TOOL(ticket_create, "创建客服工单") {
    auto user_id = ctx.get_param<std::string>("user_id");
    auto description = ctx.get_param<std::string>("description");
    auto priority = ctx.get_param<std::string>("priority", "medium");
    
    co_return ticket_system_.create_ticket(user_id, description, priority);
}

TOOL(ticket_query, "查询工单状态") {
    auto ticket_id = ctx.get_param<std::string>("ticket_id");
    co_return ticket_system_.query_ticket(ticket_id);
}
```

### 4. Agent 对话流程

```cpp
class CustomerServiceAgent {
public:
    async::Task<Response> process(const Request& req) {
        // 1. 加载上下文
        auto context = session_manager_.get_context(req.session_id);
        
        // 2. 意图识别
        auto intent = co_await intent_classifier_.classify(req.message);
        
        // 3. 根据意图路由
        switch (intent.intent) {
            case Intent::FAQ_QUERY:
                co_return co_await handle_faq_query(req, context);
                
            case Intent::TICKET_CREATE:
                co_return co_await handle_ticket_creation(req, context, intent.entities);
                
            case Intent::TICKET_QUERY:
                co_return co_await handle_ticket_query(req, intent.entities);
                
            case Intent::HUMAN_HANDOFF:
                co_return co_await handle_human_handoff(req);
                
            default:
                co_return co_await handle_general_chat(req, context);
        }
    }

private:
    async::Task<Response> handle_faq_query(const Request& req, Context& ctx) {
        // 检索知识库
        auto docs = co_await kb_.retrieve(req.message);
        
        if (docs.empty()) {
            co_return Response{
                .content = "抱歉，这个问题我还不太确定。让我为您创建一张工单，会有专门的客服同事来帮您。",
                .suggested_action = "create_ticket"
            };
        }
        
        // 构建 RAG prompt
        std::string context_text;
        for (const auto& doc : docs) {
            context_text += doc.text + "\n\n";
        }
        
        auto prompt = fmt::format(R"(
基于以下产品文档，回答用户问题：

文档内容：
{}

用户问题：{}

要求：
1. 只使用文档中的信息回答
2. 如果文档中没有相关信息，请明确告知
3. 回答要简洁明了

请回答：)", context_text, req.message);
        
        auto answer = co_await llm_.complete(prompt);
        
        co_return Response{
            .content = answer,
            .sources = docs  // 返回引用来源
        };
    }
    
    async::Task<Response> handle_ticket_creation(
        const Request& req, 
        Context& ctx,
        const std::map<std::string, std::string>& entities) {
        
        // 检查是否已有进行中的工单
        auto pending = find_pending_ticket(req.user_id);
        if (pending) {
            co_return Response{
                .content = fmt::format("您已有一张待处理的工单 {}，请耐心等待回复。", pending->id)
            };
        }
        
        // 调用工具创建工单
        auto result = co_await tools_.execute("ticket_create", {
            {"user_id", req.user_id},
            {"description", entities.at("issue")},
            {"priority", entities.value("priority", "medium")}
        });
        
        co_return Response{
            .content = fmt::format(
                "已为您创建工单 **{}**，预计 {} 内会有客服同事联系您。",
                result["ticket_id"].get<std::string>(),
                result["estimated_response"].get<std::string>()
            )
        };
    }
};
```

### 5. 配置示例

```yaml
# config/config.yaml
customer_service:
  # LLM 配置
  llm:
    provider: openai
    model: gpt-4
    temperature: 0.7
    
  # 知识库配置
  knowledge_base:
    documents_path: "config/knowledge_base.json"
    embedding_model: "text-embedding-3-small"
    top_k: 3
    similarity_threshold: 0.7
    
  # IM 平台配置
  im:
    feishu:
      enabled: true
      app_id: "${FEISHU_APP_ID}"
      app_secret: "${FEISHU_SECRET}"
    dingtalk:
      enabled: true
      app_key: "${DINGTALK_APP_KEY}"
      app_secret: "${DINGTALK_SECRET}"
      
  # 工单系统配置
  ticket:
    database: "sqlite:data/tickets.db"
    auto_assign: true
    sla:
      urgent: "1h"
      high: "4h"
      medium: "24h"
      low: "72h"
```

## 知识库文档格式

```json
[
  {
    "id": "product_overview",
    "category": "产品介绍",
    "title": "产品概述",
    "content": "我们的产品是一款面向企业的智能客服解决方案..."
  },
  {
    "id": "pricing",
    "category": "价格方案",
    "title": "定价说明",
    "content": "我们提供免费版、专业版和企业版三种方案..."
  },
  {
    "id": "api_rate_limit",
    "category": "技术问题",
    "title": "API 限流说明",
    "content": "免费版：100次/分钟，专业版：1000次/分钟..."
  }
]
```

## 运行与测试

### 启动服务

```bash
cd src/step16
mkdir build && cd build
cmake ..
make -j4

# 设置环境变量
export FEISHU_APP_ID="cli_xxxxx"
export FEISHU_SECRET="xxxx"
export OPENAI_API_KEY="sk-xxxxx"

# 运行
./customer_service_bot
```

### 测试场景

1. **FAQ 测试**
   ```
   用户：你们产品怎么收费？
   机器人：我们的产品提供免费版、专业版和企业版三种方案...
   ```

2. **工单创建**
   ```
   用户：我发现一个严重的 bug，订单支付后没有扣款
   机器人：已为您创建工单 TICKET-2024-001，预计 4h 内会有客服同事联系您。
   ```

3. **工单查询**
   ```
   用户：我昨天的工单有进展了吗？
   机器人：您的工单 TICKET-2024-001 当前状态是：处理中，客服同事已备注...
   ```

4. **转人工**
   ```
   用户：我要投诉！转人工！
   机器人：理解您的感受，正在为您转接人工客服，请稍候...
   ```

## 项目亮点

1. **完整的工作流**：从用户输入 → 意图识别 → 知识检索/工具调用 → 回复生成
2. **多平台接入**：一套代码同时支持飞书、钉钉
3. **RAG 增强**：基于向量检索的精准回答，避免 LLM 幻觉
4. **工具调用**：工单系统与对话无缝集成
5. **上下文管理**：多轮对话保持连贯性

---

## 下一步

→ **Step 17: 期末实战 — 企业级多 Agent 协作平台**

我们将构建一个更复杂的系统：
- 多个专业 Agent（客服、销售、技术支持）
- Agent 间任务委派
- 工作流编排
- 完整的运维监控
