# Step 11: 多 Agent 协作 - Agent 通信与任务分发

> 目标：理解多 Agent 架构，实现协作式问题解决
> 
> 难度：⭐⭐⭐⭐⭐ (困难)
> 
> 代码量：约 850 行
> 
> 预计学习时间：5-6 小时

---

## 🏗️ 工程化目录结构说明

### 从 Step 11 开始采用标准 C++ 项目结构

**为什么改变目录结构？**

之前的 Step 0-10 采用平铺文件结构，目的是让初学者容易理解。但从 Step 11 开始，代码复杂度增加，需要采用**工程化的目录结构**：

```
src/step11/                    # 项目根目录
├── CMakeLists.txt             # 构建系统配置
├── configs/                   # 配置文件目录
│   └── config.yaml           # YAML 配置文件
├── include/nuclaw/            # 头文件目录（规范命名空间）
│   ├── agent.hpp             # Agent 基类
│   ├── coordinator.hpp       # 协调器
│   ├── message_bus.hpp       # 消息总线
│   └── ...                   # 其他头文件
├── src/                       # 源文件目录
│   └── main.cpp              # 程序入口
└── tests/                     # 测试目录（预留）
```

**这种结构的优势：**

| 方面 | 平铺结构 (Step 0-10) | 工程化结构 (Step 11+) |
|:---|:---|:---|
| **代码组织** | 所有文件混在一起 | 头文件、源文件、配置分离 |
| **编译安装** | 需要手动指定文件 | CMake 自动处理 |
| **命名空间** | 容易命名冲突 | `include/nuclaw/` 规范命名 |
| **可维护性** | 文件多了难找 | 目录清晰，易于维护 |
| **团队协作** | 容易产生冲突 | 结构清晰，便于协作 |

**关于 `#include` 路径的变化：**

```cpp
// Step 10 及之前：平铺结构
#include "message_bus.hpp"
#include "coordinator.hpp"

// Step 11 及之后：工程化结构
#include "nuclaw/message_bus.hpp"
#include "nuclaw/coordinator.hpp"
```

使用 `nuclaw/` 前缀是为了：
1. **避免命名冲突** - 明确标识属于 nuclaw 项目的头文件
2. **规范命名空间** - 符合 C++ 项目惯例（如 `boost/asio.hpp`）
3. **便于安装** - 安装到系统 `/usr/include/nuclaw/` 时不会冲突

---

## 📚 前置知识

### 单一 Agent 的局限性

**场景：旅行规划**

```
用户：帮我规划一次北京到上海的 3 天旅行

单一 Agent 的问题：
- 需要同时考虑：交通、住宿、景点、餐饮
- 上下文过长，容易遗漏细节
- 无法并行处理多个信息源
- 各领域专业性不足
```

**类比：**
```
单一 Agent = 全科医生
  - 什么都能看
  - 但复杂问题需要专科医生

多 Agent = 专家团队
  - 心内科、脑外科、骨科...
  - 各领域的专家协作
```

### 什么是多 Agent 系统？

**定义：** 多个自主 Agent 通过协作、通信、协调来共同完成复杂任务的系统。

**核心特征：**
1. **自主性**：每个 Agent 独立决策
2. **协作性**：Agent 之间可以协作
3. **分布性**：任务分布在不同 Agent
4. **动态性**：Agent 可以动态加入/离开

### 多 Agent 架构模式

#### 1. 中心化协调（Coordinator Pattern）

```
            ┌─────────────┐
            │ 协调器 Agent │ ← 接收任务，分配子任务
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

**工作流程：**
1. 协调器接收用户任务
2. 分解任务，分发给专业 Agent
3. 各 Agent 并行处理
4. 协调器汇总结果，生成最终回答

**优点：** 结构清晰，易于管理
**缺点：** 协调器是单点瓶颈

#### 2. 去中心化协作（Peer-to-Peer）

```
   ┌───────┐ ←→ ┌───────┐ ←→ ┌───────┐
   │Agent A│    │Agent B│    │Agent C│
   └───────┘ ←→ └───────┘ ←→ └───────┘
        ↑              ↑            ↑
        └──────────────┼────────────┘
                  共享消息总线
```

**特点：**
- Agent 之间直接通信
- 没有中心协调器
- 更灵活，但更难调试

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
#include <atomic>
#include <boost/json.hpp>

namespace json = boost::json;

// 消息结构
struct Message {
    std::string id;
    std::string from;
    std::string to;
    std::string type;
    std::string content;
    std::string parent_id;
    std::chrono::system_clock::time_point timestamp;
    
    static std::string generate_id() {
        static std::atomic<int> counter{0};
        return "msg_" + std::to_string(++counter);
    }
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

## 第二步：消息总线实现

### 总线设计

```cpp
// message_bus.hpp
#pragma once
#include "agent.hpp"
#include <map>
#include <memory>
#include <iostream>

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
        std::cout << "[+] 注册 Agent: " << name << std::endl;
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

## 第三步：协调器 Agent 实现

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
    void register_agent_info(const std::string& name, const std::string& role) {
        std::lock_guard<std::mutex> lock(agents_mutex_);
        agent_roles_[name] = role;
        agent_status_[name] = "idle";
    }
    
    // 分派任务
    void dispatch_task(const std::string& task_id,
                       const std::string& task,
                       const std::vector<std::string>& target_agents) {
        
        std::cout << "[协调器] 分发任务: " << task_id << "\n";
        
        // 记录任务
        {
            std::lock_guard<std::mutex> lock(tasks_mutex_);
            pending_tasks_[task_id] = {
                {"task", task},
                {"agents", target_agents},
                {"completed", json::array()},
                {"completed_count", 0}
            };
        }
        
        // 发送给目标 Agent
        for (const auto& agent_name : target_agents) {
            Message msg;
            msg.id = Message::generate_id();
            msg.from = name_;
            msg.to = agent_name;
            msg.type = "TASK_ASSIGN";
            msg.content = json::serialize(json::object{
                {"task_id", task_id},
                {"task", task}
            });
            msg.parent_id = task_id;
            
            send(msg);
            
            {
                std::lock_guard<std::mutex> lock(agents_mutex_);
                agent_status_[agent_name] = "busy";
            }
        }
    }

protected:
    void process_message(const Message& msg) override {
        if (msg.type == "TASK_RESULT") {
            handle_task_result(msg);
        } else if (msg.type == "STATUS_UPDATE") {
            handle_status_update(msg);
        }
    }
    
    void handle_task_result(const Message& msg) {
        auto data = json::parse(msg.content);
        std::string task_id = data["task_id"].as_string();
        
        std::cout << "[协调器] 收到结果: " << msg.from << " 完成任务 " << task_id << std::endl;
        
        {
            std::lock_guard<std::mutex> lock(tasks_mutex_);
            
            if (pending_tasks_.find(task_id) != pending_tasks_.end()) {
                auto& task = pending_tasks_[task_id];
                task["completed"].as_array().push_back(data["result"]);
                
                int completed = ++task["completed_count"].as_int64();
                int expected = task["agents"].as_array().size();
                
                if (completed >= expected) {
                    task["status"] = "completed";
                    finalize_task(task_id, task);
                }
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
    
    void finalize_task(const std::string& task_id, json::value& task) {
        std::cout << "[✓] 任务完成: " << task_id << std::endl;
        
        // 汇总所有 Agent 的结果
        // 实际项目中这里会调用 LLM 进行智能汇总
        std::cout << "[协调器] 汇总结果:\n";
        for (const auto& result : task["completed"].as_array()) {
            std::cout << "  - " << json::serialize(result) << "\n";
        }
    }
    
    std::string generate_id() {
        static std::atomic<int> counter{0};
        return "task_" + std::to_string(++counter);
    }
    
    mutable std::mutex agents_mutex_;
    std::map<std::string, std::string> agent_roles_;
    std::map<std::string, std::string> agent_status_;
    
    mutable std::mutex tasks_mutex_;
    std::map<std::string, json::value> pending_tasks_;
};
```

---

## 第四步：专业 Agent 实现

### 交通 Agent

```cpp
// transport_agent.hpp
#pragma once
#include "agent.hpp"
#include <iostream>

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
            response.id = Message::generate_id();
            response.from = name_;
            response.to = "coordinator";
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
        // 模拟查询交通信息
        json::object result;
        result["flights"] = json::array{
            json::object{{"flight", "CA1234"}, {"departure", "08:00"}, {"price", 1200}},
            json::object{{"flight", "MU5678"}, {"departure", "14:00"}, {"price", 980}}
        };
        result["trains"] = json::array{
            json::object{{"train", "G1"}, {"departure", "09:00"}, {"price", 553}}
        };
        return result;
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
            response.id = Message::generate_id();
            response.from = name_;
            response.to = "coordinator";
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
};
```

---

## 第五步：完整示例

### 主程序

```cpp
// main.cpp
#include "message_bus.hpp"
#include "coordinator.hpp"
#include "transport_agent.hpp"
#include "hotel_agent.hpp"
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    std::cout << "========================================\n";
    std::cout << "   NuClaw Step 11: 多 Agent 协作\n";
    std::cout << "========================================\n\n";
    
    // 1. 创建消息总线
    MessageBus bus;
    
    // 2. 创建协调器
    auto coordinator = std::make_shared<Coordinator>("coordinator");
    bus.register_agent("coordinator", coordinator);
    
    // 3. 创建专业 Agent
    auto transport = std::make_shared<TransportAgent>();
    auto hotel = std::make_shared<HotelAgent>();
    
    bus.register_agent("transport_agent", transport);
    bus.register_agent("hotel_agent", hotel);
    
    // 4. 注册 Agent 信息到协调器
    coordinator->register_agent_info("transport_agent", "交通查询");
    coordinator->register_agent_info("hotel_agent", "酒店搜索");
    
    // 5. 模拟任务分发
    std::cout << "=== 开始旅行规划 ===\n\n";
    std::cout << "[场景] 用户：帮我规划北京到上海的行程\n\n";
    
    coordinator->dispatch_task(
        "trip_001",
        "北京到上海，3天2夜",
        {"transport_agent", "hotel_agent"}
    );
    
    // 6. 等待任务完成
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    std::cout << "\n=== 规划完成 ===\n";
    
    // 7. 关闭
    bus.shutdown();
    
    return 0;
}
```

---

## 本节总结

### 核心概念

1. **多 Agent 架构**：将复杂任务分解给多个专业 Agent 处理
2. **协调器模式**：中心化任务分配与结果汇总
3. **消息总线**：Agent 间的通信基础设施
4. **消息协议**：标准化的消息格式与类型

### 架构演进

```
单 Agent：
  用户 → 一个 Agent 处理所有事情

多 Agent：
  用户 → 协调器 → 多个专业 Agent 并行处理
              ↓
         结果汇总 → 返回用户
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
当 Agent 处理失败时，协调器自动重试或转派给其他 Agent。

### 练习 2：添加超时机制
任务超时后自动标记为失败。

### 练习 3：负载均衡
根据 Agent 的负载情况分配任务。

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
