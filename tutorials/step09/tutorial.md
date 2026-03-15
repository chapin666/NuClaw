# Step 9: 多 Agent 协作

> 目标：实现多 Agent 架构，支持复杂任务分解和协作
> 
003e 难度：⭐⭐⭐⭐ | 代码量：约 750 行 | 预计学习时间：4-5 小时

---

## 一、问题引入

### 1.1 Step 8 的局限

单个 Agent 的能力有限。复杂任务需要多个专业 Agent 协作：

```
用户: 帮我规划一个去日本的旅行

单个 Agent 的问题：
- 只能给出通用建议
- 无法同时处理路线、酒店、交通、预算
- 没有专业领域的深度知识

期望的多 Agent 协作：
┌─────────────────────────────────────────┐
│           Coordinator Agent             │
│           (协调者，理解需求)             │
└─────────────┬───────────────────────────┘
              │
    ┌─────────┼─────────┐
    │         │         │
    ▼         ▼         ▼
┌───────┐ ┌───────┐ ┌───────┐
│行程   │ │酒店   │ │交通   │
│规划   │ │预订   │ │查询   │
│Agent  │ │Agent  │ │Agent  │
└───┬───┘ └───┬───┘ └───┬───┘
    │         │         │
    └─────────┼─────────┘
              │
              ▼
┌─────────────────────────────────────────┐
│         Integrator Agent                │
│         (整合者，汇总结果)               │
└─────────────────────────────────────────┘
```

### 1.2 本章目标

1. **Agent 角色定义**：不同专业领域的 Agent
2. **任务分解**：将复杂任务分解为子任务
3. **任务分配**：将子任务分配给合适的 Agent
4. **结果整合**：汇总各 Agent 的结果

---

## 二、核心概念

### 2.1 多 Agent 架构模式

**模式 1：层级架构（Hierarchical）**
```
        Coordinator
             │
    ┌────────┼────────┐
    │        │        │
   Agent 1  Agent 2  Agent 3
```

**模式 2：网状架构（Mesh）**
```
    Agent A ←────→ Agent B
       ↑      ↕      ↑
       └────→ Agent C ←──┘
```

**模式 3：流水线架构（Pipeline）**
```
Input → Agent 1 → Agent 2 → Agent 3 → Output
```

**本章采用：层级架构**（最常用、最易理解）

### 2.2 任务分解策略

```
用户请求："帮我规划一个去日本的旅行"

Step 1: Coordinator 理解需求
- 目的地：日本
- 任务类型：旅行规划

Step 2: 分解子任务
┌─────────────────────────────────────────┐
│ 子任务 1：规划行程路线                   │
│   - 需要知道：天数、兴趣点               │
│   - 分配给：行程规划 Agent               │
├─────────────────────────────────────────┤
│ 子任务 2：查找酒店                       │
│   - 需要知道：预算、位置偏好             │
│   - 分配给：酒店预订 Agent               │
├─────────────────────────────────────────┤
│ 子任务 3：查询交通                       │
│   - 需要知道：出发地、日期               │
│   - 分配给：交通查询 Agent               │
└─────────────────────────────────────────┘

Step 3: 并行执行子任务
Step 4: 整合结果
```

### 2.3 Agent 通信协议

```cpp
struct AgentMessage {
    std::string from;           // 发送者 Agent ID
    std::string to;             // 接收者 Agent ID（空表示广播）
    std::string type;           // 消息类型：task/request/response
    json payload;               // 消息内容
    std::string correlation_id; // 关联 ID（用于追踪任务链）
};
```

---

## 三、代码结构详解

### 3.1 Agent 基类

```cpp
class Agent : public enable_shared_from_this<Agent> {
public:
    Agent(const std::string& id, 
          const std::string& role,
          LLMClient& llm)
        : id_(id), role_(role), llm_(llm) {}
    
    virtual ~Agent() = default;
    
    // 接收消息
    virtual void receive(const AgentMessage& msg) = 0;
    
    // 发送消息
    void send(const AgentMessage& msg) {
        if (message_bus_) {
            message_bus_->route(msg);
        }
    }
    
    // 设置消息总线
    void set_message_bus(std::shared_ptr<MessageBus> bus) {
        message_bus_ = bus;
    }
    
    const std::string& get_id() const { return id_; }
    const std::string& get_role() const { return role_; }

protected:
    std::string id_;
    std::string role_;
    LLMClient& llm_;
    std::shared_ptr<MessageBus> message_bus_;
};
```

### 3.2 消息总线

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
            // 广播
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

private:
    std::unordered_map<std::string, std::shared_ptr<Agent>> agents_;
    std::mutex mutex_;
};
```

### 3.3 Coordinator Agent（协调者）

```cpp
class CoordinatorAgent : public Agent {
public:
    CoordinatorAgent(LLMClient& llm) 
        : Agent("coordinator", "任务协调者", llm) {}
    
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
        
        // 2. 记录待完成任务
        std::string correlation_id = generate_id();
        pending_tasks_[correlation_id] = {
            .total = tasks.size(),
            .completed = 0,
            .results = {}
        };
        
        // 3. 分发任务
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
        
        auto& task_info = pending_tasks_[corr_id];
        task_info.completed++;
        task_info.results.push_back(msg.payload["result"]);
        
        // 所有任务完成，整合结果
        if (task_info.completed >= task_info.total) {
            json final_result = integrate_results(task_info.results);
            
            // 发送给用户
            AgentMessage response;
            response.from = id_;
            response.to = "user";
            response.type = "response";
            response.correlation_id = corr_id;
            response.payload = final_result;
            
            send(response);
            
            // 清理
            pending_tasks_.erase(corr_id);
        }
    }

private:
    struct Task {
        std::string type;
        std::string description;
        std::string assigned_agent;
        json parameters;
    };
    
    struct TaskInfo {
        size_t total;
        size_t completed;
        std::vector<json> results;
    };
    
    // 任务分解（使用 LLM）
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
        
        std::vector<json> messages = {
            {{"role", "user"}, {"content", prompt}}
        };
        
        auto response = llm_.chat(messages, {});
        
        // 解析响应
        json result = json::parse(response.content);
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
    
    // 结果整合（使用 LLM）
    json integrate_results(const std::vector<json>& results) {
        std::string prompt = "整合以下子任务结果，生成统一的回复：\n\n";
        
        for (size_t i = 0; i < results.size(); ++i) {
            prompt += "子任务 " + std::to_string(i + 1) + ":\n";
            prompt += results[i].dump(2) + "\n\n";
        }
        
        std::vector<json> messages = {
            {{"role", "user"}, {"content", prompt}}
        };
        
        auto response = llm_.chat(messages, {});
        
        return {
            {"content", response.content},
            {"sub_results", results}
        };
    }
    
    std::string generate_id() {
        static std::atomic<int> counter{0};
        return "task_" + std::to_string(++counter);
    }
    
    std::unordered_map<std::string, TaskInfo> pending_tasks_;
};
```

### 3.4 专业 Agent 示例（行程规划）

```cpp
class ItineraryAgent : public Agent {
public:
    ItineraryAgent(LLMClient& llm)
        : Agent("itinerary_planner", "行程规划专家", llm) {}
    
    void receive(const AgentMessage& msg) override {
        if (msg.type == "task") {
            handle_task(msg);
        }
    }
    
    void handle_task(const AgentMessage& msg) {
        std::string task_type = msg.payload["task_type"];
        json parameters = msg.payload["parameters"];
        
        // 执行任务
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
        
        std::vector<json> messages = {
            {{"role", "system"}, {"content", "你是专业的行程规划师。"}},
            {{"role", "user"}, {"content", prompt}}
        };
        
        auto response = llm_.chat(messages, {});
        
        return {
            {"itinerary", response.content},
            {"destination", destination},
            {"days", days}
        };
    }
};
```

### 3.5 系统初始化

```cpp
class MultiAgentSystem {
public:
    MultiAgentSystem(LLMClient& llm) : llm_(llm) {
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
    void handle_user_request(const std::string& user_input,
                            std::function<void(const json&)> callback) {
        // 设置回调
        user_callback_ = callback;
        
        // 发送给 Coordinator
        AgentMessage msg;
        msg.from = "user";
        msg.to = "coordinator";
        msg.type = "user_request";
        msg.payload = {{"content", user_input}};
        
        bus_->route(msg);
    }

private:
    LLMClient& llm_;
    std::shared_ptr<MessageBus> bus_;
    std::shared_ptr<CoordinatorAgent> coordinator_;
    std::function<void(const json&)> user_callback_;
};
```

---

## 四、完整流程示例

```
用户: 帮我规划一个去日本的5天旅行，预算1万元

Step 1: Coordinator 接收请求
        ↓
        调用 LLM 分解任务
        ↓
        子任务：
        1. 行程规划 → itinerary_planner
           {destination: "日本", days: 5, interests: [...]}
        
        2. 酒店查找 → hotel_finder
           {location: "日本", budget: 5000, nights: 4}
        
        3. 交通查询 → transport_query
           {destination: "日本", origin: "北京"}
        
        4. 预算计算 → budget_calculator
           {total_budget: 10000, items: [...]}

Step 2: 并行执行子任务
        ↓
        [4 个 Agent 同时工作]
        ↓
        各 Agent 返回结果

Step 3: Coordinator 整合结果
        ↓
        调用 LLM 生成统一回复
        ↓

回复：
"为您规划了日本5天行程：

📍 行程安排：
   Day 1: 东京 - 浅草寺、晴空塔
   Day 2: 东京 - 秋叶原、明治神宫
   ...

🏨 酒店推荐：
   新宿区商务酒店，4晚约4000元

✈️ 交通：
   北京-东京往返机票约5000元

💰 预算分配：
   机票：5000元
   酒店：4000元
   餐饮：800元
   门票：200元
   总计：10000元（刚好符合预算）"
```

---

## 五、本章总结

- ✅ Agent 基类和消息总线设计
- ✅ Coordinator 负责任务分解和结果整合
- ✅ 专业 Agent 执行特定任务
- ✅ 并行处理提高响应速度
- ✅ 代码从 650 行扩展到 750 行

---

## 六、课后思考

多 Agent 系统增强了处理能力，但还有安全问题：

```
1. API Key 泄露：代码中硬编码了 OpenAI API Key
2. 输入验证：用户可能发送恶意输入
3. 速率限制：无限制调用会导致费用飙升
4. 权限控制：没有用户权限管理
```

<details>
<summary>点击查看下一章 💡</summary>

**Step 10: 安全性与 API 管理**

我们将学习：
- API Key 安全管理
- 输入验证和过滤
- 速率限制实现
- 基础权限控制

</details>
