# Step 17: 期末实战 — 企业级多 Agent 协作平台

> 目标：构建生产级的多 Agent 协作系统，支持工作流编排和复杂任务处理
> 
> 难度：⭐⭐⭐⭐⭐ (毕业项目)
> 
> 代码量：约 1500 行

## 项目简介

这是 NuClaw 教程的**终极项目**。你将构建一个**企业级多 Agent 协作平台**，灵感来源于 OpenClaw 本身的架构设计。

### 核心能力

- 🎭 **多角色 Agent**：客服、销售、技术支持、主管 Agent
- 🔄 **Agent 间协作**：任务委派、结果汇总、冲突解决
- 📋 **工作流引擎**：可视化编排复杂业务流程
- 📊 **统一管控**：集中监控、权限管理、审计日志
- 🚀 **高可用设计**：故障转移、负载均衡、 graceful shutdown

## 业务场景

假设你在为一家 SaaS 公司构建智能客服中心：

```
用户问题: "我们的企业版订单想升级，但是 API 调用量还是不够"

处理流程:
┌──────────┐    ┌────────────┐    ┌────────────┐    ┌──────────┐
│ 客服 Agent │ → │ 销售 Agent │ → │ 技术 Agent │ → │ 主管 Agent │
│ (接待)    │    │ (方案报价) │    │ (技术评估) │    │ (最终确认) │
└──────────┘    └────────────┘    └────────────┘    └──────────┘
```

## 系统架构

```
┌──────────────────────────────────────────────────────────────────────────┐
│                        企业级多 Agent 协作平台                             │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  ┌─────────────────────────────────────────────────────────────────────┐ │
│  │                        API Gateway (Nginx)                          │ │
│  └─────────────────────────────────────────────────────────────────────┘ │
│                                    │                                     │
│  ┌─────────────────────────────────────────────────────────────────────┐ │
│  │                     Master Controller (主控)                         │ │
│  │  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐ ┌─────────────┐   │ │
│  │  │  任务调度器  │ │  负载均衡器  │ │  状态管理器  │ │  监控告警   │   │ │
│  │  └─────────────┘ └─────────────┘ └─────────────┘ └─────────────┘   │ │
│  └─────────────────────────────────────────────────────────────────────┘ │
│                                    │                                     │
│         ┌─────────────────────────┼─────────────────────────┐           │
│         ▼                         ▼                         ▼           │
│  ┌──────────────┐          ┌──────────────┐          ┌──────────────┐  │
│  │ 客服 Agent   │          │ 销售 Agent   │          │ 技术 Agent   │  │
│  │ Pool (3)     │◄────────►│ Pool (2)     │◄────────►│ Pool (2)     │  │
│  └──────────────┘          └──────────────┘          └──────────────┘  │
│         │                         │                         │           │
│         └─────────────────────────┼─────────────────────────┘           │
│                                   ▼                                     │
│                          ┌──────────────┐                              │
│                          │  主管 Agent  │                              │
│                          │  (协调决策)   │                              │
│                          └──────────────┘                              │
│                                                                          │
│  ┌─────────────────────────────────────────────────────────────────────┐ │
│  │                    共享服务层                                        │ │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐              │ │
│  │  │  向量DB  │ │  消息队列 │ │  分布式锁 │ │  审计日志 │              │ │
│  │  │(Milvus) │ │ (Rabbit) │ │ (Redis)  │ │ (Click)  │              │ │
│  │  └──────────┘ └──────────┘ └──────────┘ └──────────┘              │ │
│  └─────────────────────────────────────────────────────────────────────┘ │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

## 核心组件实现

### 1. Agent 注册与发现

```cpp
// Agent 描述信息
struct AgentDescriptor {
    std::string id;
    std::string type;           // "customer_service", "sales", "tech_support"
    std::string name;
    std::vector<std::string> capabilities;  // 能力列表
    int max_concurrency = 10;   // 最大并发
    std::chrono::seconds ttl;   // 心跳超时
    std::string endpoint;       // 服务地址
};

// Agent 注册中心
class AgentRegistry {
public:
    // Agent 注册
    async::Task<void> register_agent(const AgentDescriptor& desc) {
        auto key = fmt::format("agent:{}", desc.id);
        
        // 存储到 Redis
        co_await redis_.hset(key, "info", json(desc).dump());
        co_await redis_.expire(key, desc.ttl);
        
        // 加入类型索引
        co_await redis_.sadd(fmt::format("agents:type:{}", desc.type), desc.id);
        
        // 启动心跳检测
        start_heartbeat_monitor(desc.id);
    }
    
    // 发现特定类型的 Agent
    async::Task<std::vector<AgentDescriptor>> 
    discover_agents(const std::string& type) {
        auto agent_ids = co_await redis_.smembers(
            fmt::format("agents:type:{}", type)
        );
        
        std::vector<AgentDescriptor> agents;
        for (const auto& id : agent_ids) {
            auto info = co_await redis_.hget(fmt::format("agent:{}", id), "info");
            if (info) {
                agents.push_back(json::parse(*info));
            }
        }
        co_return agents;
    }
    
    // 选择最优 Agent（负载均衡）
    async::Task<std::optional<AgentDescriptor>> 
    select_agent(const std::string& type) {
        auto agents = co_await discover_agents(type);
        if (agents.empty()) co_return std::nullopt;
        
        // 获取每个 Agent 的当前负载
        std::vector<std::pair<AgentDescriptor, int>> loads;
        for (const auto& agent : agents) {
            auto load = co_await redis_.get(
                fmt::format("agent:{}:load", agent.id)
            );
            loads.push_back({agent, load ? std::stoi(*load) : 0});
        }
        
        // 选择负载最低的
        auto min_it = std::min_element(loads.begin(), loads.end(),
            [](const auto& a, const auto& b) { return a.second < b.second; });
        
        co_return min_it->first;
    }

private:
    RedisClient redis_;
};
```

### 2. 工作流引擎

```cpp
// 工作流定义
struct Workflow {
    std::string id;
    std::string name;
    std::vector<WorkflowNode> nodes;
    std::vector<WorkflowEdge> edges;
};

struct WorkflowNode {
    std::string id;
    std::string type;        // "agent", "condition", "parallel", "merge"
    std::string agent_type;  // Agent 类型（如果是 agent 节点）
    json config;             // 节点配置
};

struct WorkflowEdge {
    std::string from;
    std::string to;
    std::string condition;   // 条件表达式（可选）
};

// 工作流执行引擎
class WorkflowEngine {
public:
    async::Task<WorkflowResult> execute(
        const Workflow& workflow,
        const json& input) {
        
        WorkflowContext ctx;
        ctx.input = input;
        ctx.variables = input;
        
        // 拓扑排序确定执行顺序
        auto execution_order = topological_sort(workflow);
        
        for (const auto& node_id : execution_order) {
            auto result = co_await execute_node(
                workflow, 
                node_id, 
                ctx
            );
            ctx.results[node_id] = result;
        }
        
        co_return WorkflowResult{
            .success = true,
            .output = ctx.results[execution_order.back()],
            .trace = ctx.trace
        };
    }

private:
    async::Task<json> execute_node(
        const Workflow& workflow,
        const std::string& node_id,
        WorkflowContext& ctx) {
        
        auto node = find_node(workflow, node_id);
        
        if (node.type == "agent") {
            // 调用 Agent 处理
            co_return co_await execute_agent_node(node, ctx);
        }
        else if (node.type == "condition") {
            // 条件分支
            co_return co_await execute_condition_node(node, ctx);
        }
        else if (node.type == "parallel") {
            // 并行执行
            co_return co_await execute_parallel_node(workflow, node, ctx);
        }
        else if (node.type == "merge") {
            // 合并结果
            co_return execute_merge_node(node, ctx);
        }
        
        co_return json{{"error", "unknown node type"}};
    }
    
    async::Task<json> execute_agent_node(
        const WorkflowNode& node,
        WorkflowContext& ctx) {
        
        // 选择合适的 Agent
        auto agent = co_await registry_.select_agent(node.agent_type);
        if (!agent) {
            throw std::runtime_error(
                fmt::format("No available agent for type: {}", node.agent_type)
            );
        }
        
        // 构建 Agent 输入
        AgentRequest req{
            .session_id = ctx.session_id,
            .input = render_template(node.config["prompt"], ctx.variables),
            .context = collect_context(ctx)
        };
        
        // 调用 Agent
        auto start = std::chrono::steady_clock::now();
        auto response = co_await agent_client_.call(*agent, req);
        auto latency = std::chrono::steady_clock::now() - start;
        
        // 记录追踪
        ctx.trace.push_back({
            {"node", node.id},
            {"agent", agent->id},
            {"latency_ms", latency.count()},
            {"output", response.output}
        });
        
        co_return response.output;
    }
    
    async::Task<json> execute_parallel_node(
        const Workflow& workflow,
        const WorkflowNode& node,
        WorkflowContext& ctx) {
        
        // 获取所有并行分支
        auto branches = get_outgoing_edges(workflow, node.id);
        
        // 并行执行
        std::vector<async::Task<json>> tasks;
        for (const auto& edge : branches) {
            tasks.push_back(execute_subgraph(workflow, edge.to, ctx));
        }
        
        auto results = co_await async::gather(tasks);
        
        // 汇总结果
        json merged;
        for (size_t i = 0; i < results.size(); ++i) {
            merged[fmt::format("branch_{}", i)] = results[i];
        }
        
        co_return merged;
    }
};
```

### 3. Agent 间通信协议

```cpp
// Agent 间消息
struct AgentMessage {
    std::string message_id;
    std::string from_agent;
    std::string to_agent;
    MessageType type;
    json payload;
    std::chrono::timestamp timestamp;
    int ttl = 3;  // 转发次数限制，防止循环
};

enum class MessageType {
    TASK_ASSIGNMENT,      // 任务委派
    TASK_RESULT,          // 任务结果
    CLARIFICATION,        // 请求澄清
    ESCALATION,           // 升级/转交
    BROADCAST,            // 广播消息
    HEARTBEAT             // 心跳
};

// Agent 消息总线
class AgentMessageBus {
public:
    using MessageHandler = std::function<async::Task<void>(const AgentMessage&)>;
    
    void subscribe(const std::string& agent_id, MessageHandler handler) {
        handlers_[agent_id] = std::move(handler);
    }
    
    async::Task<void> send(const AgentMessage& msg) {
        // 发送到消息队列（支持跨进程）
        co_await message_queue_.publish("agent.messages", json(msg).dump());
    }
    
    async::Task<void> broadcast(const std::string& from_agent, 
                               const json& payload) {
        AgentMessage msg{
            .from_agent = from_agent,
            .type = MessageType::BROADCAST,
            .payload = payload
        };
        co_await send(msg);
    }
    
    // 请求-响应模式
    async::Task<json> request(
        const std::string& from_agent,
        const std::string& to_agent,
        const json& request_payload,
        std::chrono::seconds timeout = 30s) {
        
        auto correlation_id = generate_uuid();
        
        // 创建临时响应队列
        auto response_queue = co_await create_temp_queue(correlation_id);
        
        AgentMessage request{
            .message_id = correlation_id,
            .from_agent = from_agent,
            .to_agent = to_agent,
            .type = MessageType::TASK_ASSIGNMENT,
            .payload = request_payload
        };
        
        co_await send(request);
        
        // 等待响应
        auto response = co_await response_queue.wait_for(timeout);
        
        if (!response) {
            throw std::runtime_error("Request timeout");
        }
        
        co_return response->payload;
    }

private:
    RabbitMQClient message_queue_;
    std::unordered_map<std::string, MessageHandler> handlers_;
};
```

### 4. 主管 Agent（Supervisor）

```cpp
class SupervisorAgent {
public:
    async::Task<json> handle_request(const json& request) {
        // 1. 分析请求复杂度
        auto complexity = assess_complexity(request);
        
        // 2. 选择执行策略
        if (complexity == Complexity::SIMPLE) {
            // 简单问题，直接分配给单一 Agent
            co_return co_await handle_simple(request);
        }
        else if (complexity == Complexity::MODERATE) {
            // 中等复杂度，顺序执行
            co_return co_await handle_sequential(request);
        }
        else {
            // 高复杂度，并行分析后汇总
            co_return co_await handle_parallel(request);
        }
    }

private:
    async::Task<json> handle_parallel(const json& request) {
        // 同时启动多个 Agent 分析
        std::vector<async::Task<json>> tasks;
        
        tasks.push_back(
            message_bus_.request(id_, "sales_agent", {
                {"task", "analyze_business_impact"},
                {"request", request}
            })
        );
        
        tasks.push_back(
            message_bus_.request(id_, "tech_agent", {
                {"task", "analyze_technical_feasibility"},
                {"request", request}
            })
        );
        
        tasks.push_back(
            message_bus_.request(id_, "cs_agent", {
                {"task", "analyze_customer_sentiment"},
                {"request", request}
            })
        );
        
        auto results = co_await async::gather(tasks);
        
        // 汇总决策
        auto decision_prompt = fmt::format(R"(
基于以下各 Agent 的分析结果，做出最终决策：

业务影响分析：
{}

技术可行性分析：
{}

客户情绪分析：
{}

请给出：
1. 建议的处理方案
2. 理由说明
3. 下一步行动
)", results[0].dump(), results[1].dump(), results[2].dump());
        
        auto final_decision = co_await llm_.complete(decision_prompt);
        
        co_return json{
            {"decision", final_decision},
            {"details", results},
            {"approved_by", "supervisor"}
        };
    }
};
```

### 5. 完整配置示例

```yaml
# config/production.yaml
platform:
  name: "Enterprise Agent Platform"
  environment: production

# Master Controller 配置
master:
  listen_address: "0.0.0.0:8080"
  worker_threads: 16
  graceful_shutdown_timeout: 30s
  
# Agent 定义
agents:
  - type: customer_service
    name: "客服 Agent"
    replicas: 3
    capabilities:
      - greeting
      - faq
      - ticket_creation
      - escalation
    llm:
      model: gpt-4
      temperature: 0.7
      
  - type: sales
    name: "销售 Agent"
    replicas: 2
    capabilities:
      - product_introduction
      - pricing
      - contract_generation
      - upselling
    llm:
      model: gpt-4
      temperature: 0.8
      
  - type: tech_support
    name: "技术支持 Agent"
    replicas: 2
    capabilities:
      - troubleshooting
      - api_debugging
      - integration_guide
    tools:
      - log_query
      - status_check
      - runbook_search
      
  - type: supervisor
    name: "主管 Agent"
    replicas: 1
    capabilities:
      - decision_making
      - conflict_resolution
      - workflow_orchestration

# 工作流定义
workflows:
  - id: enterprise_upgrade
    name: "企业版升级流程"
    description: "处理企业版升级请求"
    nodes:
      - id: intake
        type: agent
        agent_type: customer_service
        config:
          prompt: "收集客户需求信息"
          
      - id: business_analysis
        type: agent
        agent_type: sales
        config:
          prompt: "分析业务需求和报价"
          
      - id: technical_review
        type: agent
        agent_type: tech_support
        config:
          prompt: "评估技术可行性和资源需求"
          
      - id: parallel_gate
        type: parallel
        
      - id: decision
        type: agent
        agent_type: supervisor
        config:
          prompt: "汇总分析并做出最终决策"
          
      - id: execution
        type: agent
        agent_type: sales
        config:
          prompt: "执行升级流程"
    edges:
      - from: intake
        to: parallel_gate
      - from: parallel_gate
        to: business_analysis
      - from: parallel_gate
        to: technical_review
      - from: business_analysis
        to: decision
      - from: technical_review
        to: decision
      - from: decision
        to: execution

# 监控配置
monitoring:
  prometheus:
    enabled: true
    port: 9090
  tracing:
    enabled: true
    jaeger_endpoint: "http://jaeger:14268"
  alerts:
    - name: agent_high_latency
      condition: "histogram_quantile(0.95, agent_response_duration_seconds) > 5"
      severity: warning
    - name: agent_unavailable
      condition: "up{job='agents'} < 0.5"
      severity: critical
```

## 部署架构

```yaml
# docker-compose.yml
version: '3.8'

services:
  master:
    build: ./master
    ports:
      - "8080:8080"
    environment:
      - REDIS_URL=redis://redis:6379
      - RABBITMQ_URL=amqp://rabbitmq:5672
      
  agent_cs:
    build: ./agents/customer_service
    replicas: 3
    depends_on:
      - redis
      - rabbitmq
      
  agent_sales:
    build: ./agents/sales
    replicas: 2
    
  agent_tech:
    build: ./agents/tech_support
    replicas: 2
    
  agent_supervisor:
    build: ./agents/supervisor
    replicas: 1
    
  redis:
    image: redis:7-alpine
    
  rabbitmq:
    image: rabbitmq:3-management
    
  prometheus:
    image: prom/prometheus
    volumes:
      - ./prometheus.yml:/etc/prometheus/prometheus.yml
      
  grafana:
    image: grafana/grafana
    ports:
      - "3000:3000"
```

## 项目交付物

完成本项目后，你将拥有：

1. **完整的系统代码** (1500+ 行)
2. **Docker 化部署** 方案
3. **Kubernetes Helm Chart**
4. **监控大盘** (Grafana)
5. **API 文档** (OpenAPI/Swagger)

---

## 恭喜你！

完成 Step 17 后，你已经掌握了：

- ✅ 企业级 Agent 系统设计
- ✅ 分布式系统架构
- ✅ 工作流引擎实现
- ✅ 微服务通信模式
- ✅ 生产环境运维

你现在有能力设计和实现复杂的 AI Agent 平台了！

---

**NuClaw 教程至此全部完成！**

从 89 行的 Echo 服务器到 1500+ 行的企业级多 Agent 平台，你已经走过了完整的 AI Agent 工程师之路。

🎉 毕业快乐！
