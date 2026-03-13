# Step 11: 多 Agent 协作 - Agent 通信与任务分发

> 目标：理解多 Agent 架构，实现协作式问题解决
> 
㸮 
> 难度：⭐⭐⭐⭐⭐ (困难)
> 
> 代码量：约 850 行
> 
> 预计学习时间：5-6 小时

---

## 📚 前置知识

### 为什么需要多 Agent？

**单一 Agent 的局限：**

```
用户：帮我规划一次北京到上海的旅行，包括交通、住宿、景点

单一 Agent 的问题：
- 需要同时处理多个领域（交通、酒店、旅游）
- 上下文过长，容易迷失
- 难以并行处理子任务
```

**多 Agent 的优势：**

```
用户：帮我规划一次北京到上海的旅行

协调 Agent (Coordinator)
    ├── 交通 Agent → 查询航班/高铁
    ├── 酒店 Agent → 搜索酒店
    └── 景点 Agent → 推荐景点
         ↓
    汇总结果 → 完整行程
```

**多 Agent 适用场景：**
- 复杂任务分解
- 跨领域协作
- 并行处理提高效率
- 角色专业化

### 多 Agent 架构模式

#### 1. 中心化协调（Coordinator Pattern）

```
            ┌─────────────┐
            │ Coordinator │ ← 接收任务，分配子任务
            └──────┬──────┘
                   │
       ┌───────────┼───────────┐
       ↓           ↓           ↓
   ┌───────┐  ┌───────┐  ┌───────┐
   │Agent A│  │Agent B│  │Agent C│
   │ 交通  │  │ 酒店  │  │ 景点  │
   └───┬───┘  └───┬───┘  └───┬───┘
       └───────────┼───────────┘
                   ↓
            ┌─────────────┐
            │  汇总结果    │
            └─────────────┘
```

**优点：** 结构清晰，易于管理
**缺点：** 协调器是单点瓶颈

#### 2. 去中心化协作（Peer-to-Peer）

```
   ┌───────┐ ←→ ┌───────┐ ←→ ┌───────┐
   │Agent A│    │Agent B│    │Agent C│
   └───────┘ ←→ └───────┘ ←→ └───────┘
        ↑              ↑            ↑
        └──────────────┼────────────┘
                       ↓
                  共享消息总线
```

**优点：** 无单点故障，灵活
**缺点：** 复杂度高，难以调试

#### 3. 层级架构（Hierarchical）

```
         ┌──────────┐
         │ 主管 Agent │
         └─────┬────┘
       ┌───────┼───────┐
       ↓       ↓       ↓
   ┌───────┐ ┌───────┐ ┌───────┐
   │组长 A │ │组长 B │ │组长 C │
   └───┬───┘ └───┬───┘ └───┬───┘
       │         │         │
     子 Agent   子 Agent   子 Agent
```

**适用：** 大规模系统，如智能客服团队

### Agent 通信协议

**消息格式：**

```cpp
struct AgentMessage {
    std::string message_id;      // 唯一标识
    std::string from_agent;      // 发送者
    std::string to_agent;        // 接收者（空表示广播）
    std::string message_type;    // 消息类型
    std::string content;         // 消息内容（JSON）
    std::string parent_id;       // 父消息（用于上下文）
    std::chrono::timestamp time; // 时间戳
};

// 消息类型枚举
enum class MessageType {
    TASK_ASSIGN,    // 任务分配
    TASK_RESULT,    // 任务结果
    QUERY,          // 查询请求
    RESPONSE,       // 查询响应
    BROADCAST,      // 广播
    HEARTBEAT       // 心跳
};
```

---

## 第一步：Agent 基类设计

### 基础 Agent 类

```cpp
// agent.hpp
#pragma once
#include <string>
#include <vector>
#include <functional>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>

// 消息结构
struct Message {
    std::string id;
    std::string from;
    std::string to;
    std::string type;
    std::string content;
    std::string parent_id;
    std::chrono::system_clock::time_point timestamp;
};

// Agent 基类
class Agent {
public:
    Agent(const std::string& name, const std::string& role)
        : name_(name), role_(role), running_(false) {}
    
    virtual ~Agent() {
        stop();
    }
    
    // 启动 Agent
    void start() {
        running_ = true;
        worker_ = std::thread(&Agent::run, this);
    }
    
    // 停止 Agent
    void stop() {
        running_ = false;
        cv_.notify_all();
        if (worker_.joinable()) {
            worker_.join();
        }
    }
    
    // 发送消息给此 Agent
    void send_message(const Message& msg) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            inbox_.push(msg);
        }
        cv_.notify_one();
    }
    
    // 获取 Agent 名称
    std::string get_name() const { return name_; }
    std::string get_role() const { return role_; }
    
    // 设置消息发送回调
    void set_send_callback(std::function<void(const Message&)> callback) {
        send_callback_ = callback;
    }

protected:
    // 主循环（子类可覆盖）
    virtual void run() {
        while (running_) {
            Message msg;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock, [this] { return !inbox_.empty() || !running_; });
                
                if (!running_) break;
                
                msg = inbox_.front();
                inbox_.pop();
            }
            
            // 处理消息
            process_message(msg);
        }
    }
    
    // 处理消息（子类必须实现）
    virtual void process_message(const Message& msg) = 0;
    
    // 发送消息（通过回调）
    void send(const Message& msg) {
        if (send_callback_) {
            send_callback_(msg);
        }
    }
    
    std::string name_;
    std::string role_;
    bool running_;
    
    std::queue<Message> inbox_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::thread worker_;
    
    std::function<void(const Message&)> send_callback_;
};
```

---

## 第二步：协调 Agent 实现

### Coordinator 类

```cpp
// coordinator.hpp
#pragma once
#include "agent.hpp"
#include <map>
#include <set>

class Coordinator : public Agent {
public:
    Coordinator(const std::string& name = "coordinator")
        : Agent(name, "coordinator") {}
    
    // 注册子 Agent
    void register_agent(const std::string& name, const std::string& role) {
        std::lock_guard<std::mutex> lock(agents_mutex_);
        agent_roles_[name] = role;
        agent_status_[name] = "idle";
    }
    
    // 分派任务
    void dispatch_task(const std::string& task, 
                       const std::vector<std::string>& target_agents) {
        
        std::string task_id = generate_id();
        
        for (const auto& agent_name : target_agents) {
            Message msg;
            msg.id = generate_id();
            msg.from = name_;
            msg.to = agent_name;
            msg.type = "TASK_ASSIGN";
            msg.content = json::serialize(json::object{
                {"task_id", task_id},
                {"task", task},
                {"deadline", "5m"}
            });
            msg.parent_id = task_id;
            
            send(msg);
            
            {
                std::lock_guard<std::mutex> lock(agents_mutex_);
                agent_status_[agent_name] = "busy";
            }
        }
        
        // 记录任务状态
        {
            std::lock_guard<std::mutex> lock(tasks_mutex_);
            pending_tasks_[task_id] = {
                {"task", task},
                {"agents", target_agents},
                {"results", json::array()},
                {"completed", false}
            };
        }
    }
    
    // 广播消息
    void broadcast(const std::string& content, const std::string& type = "BROADCAST") {
        Message msg;
        msg.id = generate_id();
        msg.from = name_;
        msg.to = "";  // 空表示广播
        msg.type = type;
        msg.content = content;
        
        send(msg);
    }

protected:
    void process_message(const Message& msg) override {
        if (msg.type == "TASK_RESULT") {
            handle_task_result(msg);
        } else if (msg.type == "STATUS_UPDATE") {
            handle_status_update(msg);
        } else if (msg.type == "QUERY") {
            handle_query(msg);
        }
    }
    
    void handle_task_result(const Message& msg) {
        auto data = json::parse(msg.content);
        std::string task_id = data["task_id"].as_string();
        
        std::lock_guard<std::mutex> lock(tasks_mutex_);
        
        if (pending_tasks_.find(task_id) != pending_tasks_.end()) {
            auto& task = pending_tasks_[task_id];
            task["results"].as_array().push_back(data["result"]);
            
            // 检查是否所有 Agent 都完成了
            size_t expected = task["agents"].as_array().size();
            size_t received = task["results"].as_array().size();
            
            if (received >= expected) {
                task["completed"] = true;
                finalize_task(task_id, task);
            }
        }
        
        // 更新 Agent 状态
        {
            std::lock_guard<std::mutex> lock(agents_mutex_);
            agent_status_[msg.from] = "idle";
        }
    }
    
    void handle_status_update(const Message& msg) {
        auto data = json::parse(msg.content);
        std::string status = data["status"].as_string();
        
        std::lock_guard<std::mutex> lock(agents_mutex_);
        agent_status_[msg.from] = status;
    }
    
    void handle_query(const Message& msg) {
        // 回复当前状态
        json::object status;
        
        {
            std::lock_guard<std::mutex> lock(agents_mutex_);
            for (const auto& [name, role] : agent_roles_) {
                status[name] = agent_status_[name];
            }
        }
        
        Message response;
        response.id = generate_id();
        response.from = name_;
        response.to = msg.from;
        response.type = "RESPONSE";
        response.content = json::serialize(status);
        response.parent_id = msg.id;
        
        send(response);
    }
    
    void finalize_task(const std::string& task_id, json::value& task) {
        std::cout << "[✓] 任务完成: " << task_id << std::endl;
        
        // 可以在这里触发回调或通知用户
        // task["results"] 包含所有 Agent 的结果
    }
    
    std::string generate_id() {
        static std::atomic<int> counter{0};
        return "msg_" + std::to_string(++counter);
    }
    
    mutable std::mutex agents_mutex_;
    std::map<std::string, std::string> agent_roles_;
    std::map<std::string, std::string> agent_status_;
    
    mutable std::mutex tasks_mutex_;
    std::map<std::string, json::value> pending_tasks_;
};
```

---

## 第三步：专业 Agent 实现

### 交通 Agent

```cpp
// transport_agent.hpp
#pragma once
#include "agent.hpp"
#include "tool_registry.hpp"

class TransportAgent : public Agent {
public:
    TransportAgent(const std::string& name = "transport_agent")
        : Agent(name, "transport") {}

protected:
    void process_message(const Message& msg) override {
        if (msg.type == "TASK_ASSIGN") {
            auto data = json::parse(msg.content);
            std::string task = data["task"].as_string();
            std::string task_id = data["task_id"].as_string();
            
            std::cout << "[" << name_ << "] 处理交通任务: " << task << std::endl;
            
            // 执行交通查询
            auto result = query_transport(task);
            
            // 返回结果
            Message response;
            response.id = generate_id();
            response.from = name_;
            response.to = msg.from;  // 回复给协调器
            response.type = "TASK_RESULT";
            response.content = json::serialize(json::object{
                {"task_id", task_id},
                {"agent", name_},
                {"result", result}
            });
            response.parent_id = msg.id;
            
            send(response);
        }
    }
    
    json::value query_transport(const std::string& query) {
        // 调用交通查询工具
        // 简化示例
        json::object result;
        result["flights"] = json::array{
            json::object{{"flight", "CA1234"}, {"time", "08:00"}, {"price", 1200}},
            json::object{{"flight", "MU5678"}, {"time", "14:00"}, {"price", 980}}
        };
        result["trains"] = json::array{
            json::object{{"train", "G1"}, {"time", "09:00"}, {"price", 553}}
        };
        return result;
    }
    
    std::string generate_id() {
        static std::atomic<int> counter{0};
        return name_ + "_" + std::to_string(++counter);
    }
};
```

### 酒店 Agent

```cpp
// hotel_agent.hpp
#pragma once
#include "agent.hpp"

class HotelAgent : public Agent {
public:
    HotelAgent(const std::string& name = "hotel_agent")
        : Agent(name, "hotel") {}

protected:
    void process_message(const Message& msg) override {
        if (msg.type == "TASK_ASSIGN") {
            auto data = json::parse(msg.content);
            std::string task = data["task"].as_string();
            std::string task_id = data["task_id"].as_string();
            
            std::cout << "[" << name_ << "] 处理酒店任务: " << task << std::endl;
            
            auto result = query_hotels(task);
            
            Message response;
            response.id = generate_id();
            response.from = name_;
            response.to = msg.from;
            response.type = "TASK_RESULT";
            response.content = json::serialize(json::object{
                {"task_id", task_id},
                {"agent", name_},
                {"result", result}
            });
            response.parent_id = msg.id;
            
            send(response);
        }
    }
    
    json::value query_hotels(const std::string& query) {
        json::object result;
        result["hotels"] = json::array{
            json::object{{"name", "希尔顿"}, {"rating", 4.8}, {"price", 800}},
            json::object{{"name", "如家"}, {"rating", 4.2}, {"price", 300}}
        };
        return result;
    }
    
    std::string generate_id() {
        static std::atomic<int> counter{0};
        return name_ + "_" + std::to_string(++counter);
    }
};
```

### 景点 Agent

```cpp
// attraction_agent.hpp
#pragma once
#include "agent.hpp"

class AttractionAgent : public Agent {
public:
    AttractionAgent(const std::string& name = "attraction_agent")
        : Agent(name, "attraction") {}

protected:
    void process_message(const Message& msg) override {
        if (msg.type == "TASK_ASSIGN") {
            auto data = json::parse(msg.content);
            std::string task = data["task"].as_string();
            std::string task_id = data["task_id"].as_string();
            
            std::cout << "[" << name_ << "] 处理景点任务: " << task << std::endl;
            
            auto result = query_attractions(task);
            
            Message response;
            response.id = generate_id();
            response.from = name_;
            response.to = msg.from;
            response.type = "TASK_RESULT";
            response.content = json::serialize(json::object{
                {"task_id", task_id},
                {"agent", name_},
                {"result", result}
            });
            response.parent_id = msg.id;
            
            send(response);
        }
    }
    
    json::value query_attractions(const std::string& query) {
        json::object result;
        result["attractions"] = json::array{
            json::object{{"name", "外滩"}, {"rating", 4.9}, {"duration", "2h"}},
            json::object{{"name", "东方明珠"}, {"rating", 4.7}, {"duration", "3h"}},
            json::object{{"name", "豫园"}, {"rating", 4.5}, {"duration", "2h"}}
        };
        return result;
    }
    
    std::string generate_id() {
        static std::atomic<int> counter{0};
        return name_ + "_" + std::to_string(++counter);
    }
};
```

---

## 第四步：消息总线

### 总线实现

```cpp
// message_bus.hpp
#pragma once
#include "agent.hpp"
#include <map>

class MessageBus {
public:
    // 注册 Agent
    void register_agent(const std::string& name, std::shared_ptr<Agent> agent) {
        agents_[name] = agent;
        
        // 设置消息发送回调
        agent->set_send_callback([this](const Message& msg) {
            route_message(msg);
        });
        
        agent->start();
    }
    
    // 路由消息
    void route_message(const Message& msg) {
        if (msg.to.empty()) {
            // 广播
            broadcast(msg);
        } else {
            // 点对点
            auto it = agents_.find(msg.to);
            if (it != agents_.end()) {
                it->second->send_message(msg);
            } else {
                std::cerr << "[!] 未知接收者: " << msg.to << std::endl;
            }
        }
    }
    
    // 广播消息
    void broadcast(const Message& msg) {
        for (const auto& [name, agent] : agents_) {
            if (name != msg.from) {  // 不发送给自己
                agent->send_message(msg);
            }
        }
    }
    
    // 获取 Agent
    std::shared_ptr<Agent> get_agent(const std::string& name) {
        auto it = agents_.find(name);
        if (it != agents_.end()) {
            return it->second;
        }
        return nullptr;
    }
    
    // 停止所有 Agent
    void shutdown() {
        for (auto& [name, agent] : agents_) {
            agent->stop();
        }
        agents_.clear();
    }

private:
    std::map<std::string, std::shared_ptr<Agent>> agents_;
};
```

---

## 第五步：完整示例

### 旅行规划示例

```cpp
// main.cpp
#include "message_bus.hpp"
#include "coordinator.hpp"
#include "transport_agent.hpp"
#include "hotel_agent.hpp"
#include "attraction_agent.hpp"
#include <iostream>

int main() {
    // 1. 创建消息总线
    MessageBus bus;
    
    // 2. 创建协调器
    auto coordinator = std::make_shared<Coordinator>("trip_coordinator");
    bus.register_agent("trip_coordinator", coordinator);
    
    // 3. 创建专业 Agent
    auto transport = std::make_shared<TransportAgent>();
    auto hotel = std::make_shared<HotelAgent>();
    auto attraction = std::make_shared<AttractionAgent>();
    
    bus.register_agent("transport_agent", transport);
    bus.register_agent("hotel_agent", hotel);
    bus.register_agent("attraction_agent", attraction);
    
    // 4. 注册到协调器
    coordinator->register_agent("transport_agent", "transport");
    coordinator->register_agent("hotel_agent", "hotel");
    coordinator->register_agent("attraction_agent", "attraction");
    
    // 5. 分发任务
    std::cout << "=== 开始旅行规划 ===" << std::endl;
    coordinator->dispatch_task(
        "北京到上海，3天2夜",
        {"transport_agent", "hotel_agent", "attraction_agent"}
    );
    
    // 6. 等待结果
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    std::cout << "=== 规划完成 ===" << std::endl;
    
    // 7. 关闭
    bus.shutdown();
    
    return 0;
}
```

### 运行输出

```
=== 开始旅行规划 ===
[transport_agent] 处理交通任务: 北京到上海，3天2夜
[hotel_agent] 处理酒店任务: 北京到上海，3天2夜
[attraction_agent] 处理景点任务: 北京到上海，3天2夜
[✓] 任务完成: task_1
=== 规划完成 ===
```

---

## 本节总结

### 核心概念

1. **多 Agent 架构**：将复杂任务分解给多个专业 Agent 处理
2. **协调器模式**：中心化任务分配与结果汇总
3. **消息总线**：Agent 间的通信基础设施
4. **消息协议**：标准化的消息格式与类型

### 代码演进

```
Step 10: RAG 系统 (800行)
   ↓ + 多 Agent 架构
Step 11: 850行
   + agent.hpp: Agent 基类
   + coordinator.hpp: 协调器实现
   + *_agent.hpp: 专业 Agent 实现
   + message_bus.hpp: 消息总线
```

### 设计考虑

| 方面 | 选择 | 说明 |
|:---|:---|:---|
| **架构模式** | 中心化协调 | 简单清晰，适合入门 |
| **通信方式** | 内存消息队列 | 单机版，可扩展为网络 |
| **并发模型** | 每个 Agent 一个线程 | 简单有效 |
| **消息格式** | JSON | 通用、易调试 |

---

## 📝 课后练习

### 练习 1：实现错误重试
当 Agent 处理失败时，协调器自动重试或转派给其他 Agent：
```cpp
void handle_task_failure(const Message& msg) {
    // 重试逻辑
    // 或转派给其他 Agent
}
```

### 练习 2：添加超时机制
任务超时后自动标记为失败：
```cpp
void check_timeouts() {
    // 定期检查 pending_tasks
    // 超时的任务标记为失败
}
```

### 练习 3：负载均衡
根据 Agent 的负载情况分配任务：
```cpp
std::string select_best_agent(const std::vector<std::string>& candidates) {
    // 选择负载最低的 Agent
}
```

### 思考题
1. 中心化架构的瓶颈在哪里？如何解决？
2. Agent 间通信如何保证消息不丢失？
3. 多 Agent 系统的调试和监控有什么挑战？

---

## 📖 扩展阅读

### 多 Agent 框架

- **AutoGen** (Microsoft)：多 Agent 对话框架
- **LangGraph**：基于图的多 Agent 工作流
- **CrewAI**：角色扮演的多 Agent 系统

### 分布式系统概念

- **Actor 模型**：Erlang/Elixir 的并发模型
- **CQRS**：命令查询职责分离
- **Event Sourcing**：事件溯源

---

**恭喜！** 你现在掌握了多 Agent 协作系统的设计。下一章我们将进入生产就绪阶段，实现配置管理。
