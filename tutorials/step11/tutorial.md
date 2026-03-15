# Step 11: 多 Agent 协作 —— 团队作战

> 目标：实现多 Agent 架构，支持复杂任务分解和协作
> 
003e 难度：⭐⭐⭐⭐ | 代码量：约 850 行 | 预计学习时间：4-5 小时

---

## 一、为什么需要多 Agent？

### 1.1 单 Agent 的局限

一个 Agent 的能力有限，复杂任务需要多个专家协作：

```
单 Agent 处理复杂任务：

用户: "帮我规划一个去日本的旅行"

Agent 1（全能型）:
- 只能给出通用建议
- 无法同时处理路线、酒店、交通、预算
- 没有专业领域的深度知识

缺点：
- 什么都懂一点，什么都不精
- 复杂任务容易遗漏细节
- 处理时间太长
```

### 1.2 多 Agent 协作的价值

```
多 Agent 处理相同任务：

用户: "帮我规划一个去日本的旅行"

        ┌─────────────────────────┐
        │    Coordinator Agent    │
        │      (协调者/入口)       │
        └───────────┬─────────────┘
                    │ 分解任务
        ┌───────────┼───────────┐
        ▼           ▼           ▼
┌───────────┐ ┌───────────┐ ┌───────────┐
│ 行程规划   │ │ 酒店查询   │ │ 交通查询   │
│  Agent    │ │  Agent    │ │  Agent    │
└─────┬─────┘ └─────┬─────┘ └─────┬─────┘
      │             │             │
      └─────────────┼─────────────┘
                    │ 汇总结果
                    ▼
        ┌─────────────────────────┐
        │    Integrator Agent     │
        │      (整合者/出口)       │
        └─────────────────────────┘
                    │
                    ▼
            "为您规划了日本5天行程：
             📍 东京-大阪-京都
             🏨 新宿酒店，4晚
             ✈️ 北京-东京往返"
```

**多 Agent 优势：**
- **专业化**：每个 Agent 专注一个领域
- **并行化**：多个 Agent 同时工作
- **可扩展**：新增领域只需添加新 Agent
- **容错性**：单个 Agent 失败不影响整体

---

## 二、多 Agent 架构设计

### 2.1 架构模式

**模式 1：层级架构（Hierarchical）**

```
        Coordinator
             │
    ┌────────┼────────┐
    │        │        │
   Agent 1  Agent 2  Agent 3
```

- Coordinator 负责任务分解和结果整合
- 最常用、最容易理解

**模式 2：流水线（Pipeline）**

```
Input → Agent A → Agent B → Agent C → Output
```

- 每个 Agent 完成一个处理步骤
- 适合有明确流程的任务

**模式 3：网状架构（Mesh）**

```
    Agent A ←────→ Agent B
       ↑      ↕      ↑
       └────→ Agent C ←──┘
```

- Agent 之间直接通信
- 适合复杂的协作场景

**本章采用：层级架构**（最实用）

### 2.2 核心组件

```cpp
// Agent 间通信消息
struct AgentMessage {
    std::string from;           // 发送者 ID
    std::string to;             // 接收者 ID（空表示广播）
    std::string type;           // 消息类型：task/request/response/event
    json payload;               // 消息内容
    std::string correlation_id; // 关联 ID（追踪任务链）
    std::chrono::timestamp timestamp;
};

// Agent 基类
class Agent : public std::enable_shared_from_this<Agent> {
public:
    Agent(const std::string& id, const std::string& role)
        : id_(id), role_(role) {}
    
    virtual ~Agent() = default;
    
    // 接收消息（子类实现）
    virtual void receive(const AgentMessage& msg) = 0;
    
    // 发送消息
    void send(const AgentMessage& msg);
    
    // 设置消息总线
    void set_message_bus(std::shared_ptr<MessageBus> bus) {
        message_bus_ = bus;
    }
    
    const std::string& get_id() const { return id_; }
    const std::string& get_role() const { return role_; }

protected:
    std::string id_;
    std::string role_;
    std::shared_ptr<MessageBus> message_bus_;
};
```

---

## 三、消息总线实现

```cpp
class MessageBus {
public:
    void register_agent(std::shared_ptr<Agent> agent) {
        std::lock_guard<std::mutex> lock(mutex_);
        agents_[agent->get_id()] = agent;
        agent->set_message_bus(shared_from_this());
    }
    
    void route(const AgentMessage& msg) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (msg.to.empty()) {
            // 广播给所有 Agent（除了发送者）
            for (const auto& [id, agent] : agents_) {
                if (id != msg.from) {
                    agent->receive(msg);
                }
            }
        } else {
            // 单播
            auto it = agents_.find(msg.to);
            if (it != agents_.end()) {
                it->second->receive(msg);
            }
        }
    }
    
    // 异步发送（带超时）
    void request(const AgentMessage& req,
                 std::chrono::milliseconds timeout,
                 std::function<void(std::optional<AgentMessage>)> callback);

private:
    std::unordered_map<std::string, std::shared_ptr<Agent>> agents_;
    std::mutex mutex_;
};
```

---

## 四、Coordinator Agent

### 4.1 任务分解

```cpp
class CoordinatorAgent : public Agent {
public:
    CoordinatorAgent(LLMClient& llm) 
        : Agent("coordinator", "任务协调者"), llm_(llm) {}
    
    void receive(const AgentMessage& msg) override {
        if (msg.type == "user_request") {
            handle_user_request(msg);
        } else if (msg.type == "task_complete") {
            handle_task_complete(msg);
        }
    }
    
    // 用户请求入口
    void handle_user_request(const AgentMessage& msg) {
        std::string user_input = msg.payload["content"];
        
        // 1. 分析需求，分解任务
        std::vector<Task> tasks = decompose_task(user_input);
        
        // 2. 生成任务 ID
        std::string correlation_id = generate_uuid();
        
        // 3. 记录任务状态
        TaskGroup& group = task_groups_[correlation_id];
        group.total = tasks.size();
        group.user_callback = msg.payload["callback"];  // 保存回调
        
        // 4. 分发任务
        for (const auto& task : tasks) {
            AgentMessage task_msg;
            task_msg.from = id_;
            task_msg.to = task.assigned_agent;
            task_msg.type = "task";
            task_msg.correlation_id = correlation_id;
            task_msg.payload = {
                {"task_type", task.type},
                {"task_description", task.description},
                {"parameters", task.parameters}
            };
            
            send(task_msg);
        }
    }
    
    // 处理子任务完成
    void handle_task_complete(const AgentMessage& msg) {
        std::string corr_id = msg.correlation_id;
        
        auto it = task_groups_.find(corr_id);
        if (it == task_groups_.end()) return;
        
        TaskGroup& group = it->second;
        group.completed++;
        group.results.push_back(msg.payload["result"]);
        
        // 所有任务完成，整合结果
        if (group.completed >= group.total) {
            std::string final_result = integrate_results(group.results);
            
            // 回调给用户
            group.user_callback(final_result);
            
            // 清理
            task_groups_.erase(it);
        }
    }

private:
    struct Task {
        std::string type;
        std::string description;
        std::string assigned_agent;
        json parameters;
    };
    
    struct TaskGroup {
        size_t total = 0;
        size_t completed = 0;
        std::vector<json> results;
        std::function<void(std::string)> user_callback;
    };
    
    // 使用 LLM 分解任务
    std::vector<Task> decompose_task(const std::string& user_input) {
        std::string prompt = R"(
分析用户需求，分解为子任务。

可用 Agent：
- itinerary_planner: 行程规划
- hotel_finder: 酒店查找
- transport_query: 交通查询
- budget_calculator: 预算计算

用户需求：)" + user_input + R"(

请输出 JSON 格式的子任务列表：
{
  "tasks": [
    {
      "type": "任务类型",
      "description": "任务描述",
      "assigned_agent": "分配的 Agent ID",
      "parameters": {...}
    }
  ]
}
)";
        
        // 调用 LLM 解析任务
        auto response = llm_.chat({{"user", prompt}});
        
        // 解析响应
        json result = json::parse(response);
        std::vector<Task> tasks;
        
        for (const auto& t : result["tasks"]) {
            tasks.push_back({
                t.value("type", ""),
                t.value("description", ""),
                t.value("assigned_agent", ""),
                t.value("parameters", json::object())
            });
        }
        
        return tasks;
    }
    
    // 使用 LLM 整合结果
    std::string integrate_results(const std::vector<json>& results) {
        std::string prompt = "整合以下子任务结果，生成统一的回复：\n\n";
        
        for (size_t i = 0; i < results.size(); ++i) {
            prompt += "子任务 " + std::to_string(i + 1) + ":\n";
            prompt += results[i].dump(2) + "\n\n";
        }
        
        return llm_.chat({{"user", prompt}});
    }
    
    LLMClient& llm_;
    std::unordered_map<std::string, TaskGroup> task_groups_;
};
```

---

## 五、专业 Agent 实现

### 5.1 行程规划 Agent

```cpp
class ItineraryAgent : public Agent {
public:
    ItineraryAgent(LLMClient& llm) 
        : Agent("itinerary_planner", "行程规划专家"), llm_(llm) {}
    
    void receive(const AgentMessage& msg) override {
        if (msg.type == "task") {
            handle_task(msg);
        }
    }
    
    void handle_task(const AgentMessage& msg) {
        std::string task_type = msg.payload["task_type"];
        json parameters = msg.payload["parameters"];
        
        json result;
        
        if (task_type == "plan_route") {
            result = plan_route(
                parameters["destination"],
                parameters["days"],
                parameters.value("interests", json::array())
            );
        }
        
        // 返回结果
        AgentMessage response;
        response.from = id_;
        response.to = msg.from;  // 返回给 Coordinator
        response.type = "task_complete";
        response.correlation_id = msg.correlation_id;
        response.payload = {
            {"agent_id", id_},
            {"agent_role", role_},
            {"result", result}
        };
        
        send(response);
    }

private:
    json plan_route(const std::string& destination,
                    int days,
                    const json& interests) {
        // 使用 LLM 生成行程规划
        std::string prompt = "规划 " + std::to_string(days) + " 天的 " + 
                            destination + " 行程。";
        
        if (!interests.empty()) {
            prompt += "用户兴趣：" + interests.dump();
        }
        
        std::string plan = llm_.chat({
            {{"role", "system"}, {"content", "你是专业的行程规划师。"}},
            {{"role", "user"}, {"content", prompt}}
        });
        
        return {
            {"itinerary", plan},
            {"destination", destination},
            {"days", days}
        };
    }

    LLMClient& llm_;
};
```

---

## 六、系统初始化

```cpp
class MultiAgentSystem {
public:
    MultiAgentSystem(asio::io_context& io, LLMClient& llm) 
        : io_(io), llm_(llm) {
        
        // 创建消息总线
        bus_ = std::make_shared<MessageBus>();
        
        // 创建 Coordinator
        coordinator_ = std::make_shared<CoordinatorAgent>(llm_);
        bus_->register_agent(coordinator_);
        
        // 创建专业 Agent
        auto itinerary = std::make_shared<ItineraryAgent>(llm_);
        auto hotel = std::make_shared<HotelAgent>(llm_);
        auto transport = std::make_shared<TransportAgent>(llm_);
        
        bus_->register_agent(itinerary);
        bus_->register_agent(hotel);
        bus_->register_agent(transport);
    }
    
    // 用户请求入口
    void handle_request(const std::string& user_input,
                        std::function<void(const std::string&)> callback) {
        AgentMessage msg;
        msg.from = "user";
        msg.to = "coordinator";
        msg.type = "user_request";
        msg.payload = {
            {"content", user_input},
            {"callback", callback}
        };
        
        bus_->route(msg);
    }

private:
    asio::io_context& io_;
    LLMClient& llm_;
    std::shared_ptr<MessageBus> bus_;
    std::shared_ptr<CoordinatorAgent> coordinator_;
};
```

---

## 七、本章小结

**核心收获：**

1. **多 Agent 架构**：
   - Coordinator 负责任务分解和整合
   - 专业 Agent 执行特定任务
   - 消息总线实现通信

2. **任务协作流程**：
   - 分解 → 分发 → 执行 → 整合

3. **并行处理**：
   - 多个 Agent 同时工作
   - 提高整体响应速度

---

## 八、引出的问题

### 8.1 配置管理问题

目前配置硬编码：

```cpp
llm_ = LLMClient("sk-xxx");  // API Key 硬编码！
```

**需要：** 配置文件管理、热加载。

### 8.2 监控问题

不知道系统运行状态：

- 多少 Agent 在运行？
- 任务成功率多少？
- 响应时间多少？

**需要：** 监控、日志、指标收集。

---

**后续章节预告：**

- **Step 12**: 配置管理（YAML/JSON 配置、热加载）
- **Step 13**: 监控告警（Metrics、Logging、Tracing）
- **Step 14**: 部署运维（Docker、K8s、CI/CD）

多 Agent 系统已经搭建，接下来要让系统可配置、可监控、可部署。
