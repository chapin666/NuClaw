# Step 11: 多 Agent 协作 - 团队作战

> 目标：理解多 Agent 架构，实现协作式问题解决
> 
> 难度：⭐⭐⭐⭐⭐ (困难)
> 
> 代码量：约 850 行
> 
> 预计学习时间：5-6 小时

---

## 📚 前置知识

### 单一 Agent 的局限

**场景：旅行规划**
```
用户：帮我规划一次北京到上海的 3 天旅行

单一 Agent 的问题：
- 需要同时考虑：交通、住宿、景点、餐饮
- 上下文过长，容易遗漏细节
- 无法并行处理多个信息源
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

**定义：** 多个自主 Agent 通过协作共同完成复杂任务的系统。

**核心特征：**
- **自主性**：每个 Agent 独立决策
- **协作性**：Agent 之间可以协作
- **分布性**：任务分布在不同 Agent

### 架构模式

**1. 中心化协调（Orchestrator）**
```
          协调器 Agent
               │
       ┌───────┼───────┐
       ▼       ▼       ▼
   交通A    酒店A    景点A
```

**2. 去中心化协作（Peer-to-Peer）**
```
   AgentA ←→ AgentB ←→ AgentC
      ↑              ↑
      └──────┬───────┘
            总线
```

---

## 第一步：Agent 基类设计

### 为什么需要基类？

**目的：** 定义统一的 Agent 接口和行为。

**类比：** 公司员工
```
所有员工都有：
- 姓名、工号
- 接收任务
- 完成任务
- 汇报结果
```

### Agent 基类

```cpp
class Agent {
public:
    Agent(const string& name, const string& role);
    
    void start();    // 启动 Agent
    void stop();     // 停止 Agent
    void send_message(const Message& msg);  // 接收消息
    
protected:
    virtual void process_message(const Message& msg) = 0;
    
    string name_;
    string role_;
    queue<Message> inbox_;
};
```

---

## 第二步：消息通信

### 消息总线

**作用：** 实现 Agent 间的松耦合通信。

**类比：** 公司前台
```
员工A ──信件──> 前台 ──转发──> 员工B

员工不需要知道对方的位置，只需要给前台
```

### 消息总线实现

```cpp
class MessageBus {
public:
    void register_agent(const string& name, shared_ptr<Agent> agent);
    void route_message(const Message& msg);  // 路由消息
    void broadcast(const Message& msg);      // 广播
};
```

---

## 第三步：协调器设计

### 协调器的职责

**作用：** 任务分解、分发、结果汇总。

**类比：** 项目经理
```
项目经理接收项目
    ↓
分解为子任务
    ↓
分配给各专业员工
    ↓
收集结果，汇总汇报
```

### 协调器实现

```cpp
class Coordinator : public Agent {
public:
    void dispatch_task(const string& task_id,
                       const string& task,
                       const vector<string>& targets);
    
protected:
    void process_message(const Message& msg) override;
    
private:
    map<string, string> agent_capabilities_;  // Agent 能力表
    map<string, string> agent_status_;        // Agent 状态
};
```

---

## 第四步：专业 Agent

### 交通 Agent 示例

```cpp
class TransportAgent : public Agent {
protected:
    void process_message(const Message& msg) override {
        if (msg.type == "TASK_ASSIGN") {
            // 查询交通信息
            auto result = query_transport(task);
            
            // 返回结果给协调器
            send_response(msg.from, result);
        }
    }
};
```

---

## 本节总结

### 核心概念

1. **多 Agent 架构**：复杂任务分解给多个专业 Agent
2. **消息总线**：Agent 间通信的基础设施
3. **协调器**：任务分发和结果汇总

### 适用场景

| 场景 | 是否适合 |
|:---|:---:|
| 简单问答 | ❌ 单 Agent 足够 |
| 跨领域任务 | ✅ 需要专业分工 |
| 复杂工作流 | ✅ 需要任务分解 |

---

## 📝 课后练习

### 练习：设计一个客服团队
设计包含以下角色的多 Agent 系统：
- 接待员（分流问题）
- 技术支持（解决技术问题）
- 销售顾问（处理订单）

### 思考题
1. 多 Agent 架构的优势和代价是什么？
2. 如何避免协调器成为瓶颈？

---

## 📖 扩展阅读

- **AutoGen**：微软的多 Agent 框架
- **Actor 模型**：Erlang/Elixir 的并发模型
- **微服务架构**：与多 Agent 的相似性
