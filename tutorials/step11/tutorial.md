# Step 11: 多 Agent 协作 - Agent 通信与任务分发

> 目标：实现多 Agent 协作系统，处理复杂任务
> 
> 难度：⭐⭐⭐⭐⭐ (困难)
> 
> 代码量：约 850 行
> 
> 预计学习时间：5-6 小时

---

## 🎯 Agent 开发知识点

**本节核心问题：** 复杂任务如何分解给多个专业 Agent 处理？

**Agent 架构演进：**
```
单 Agent（Step 1-10）    多 Agent（Step 11）
     │                        │
     ▼                        ▼
┌─────────┐           ┌─────────────┐
│通用Agent│           │ 协调器Agent  │
└────┬────┘           └──────┬──────┘
     │              ┌────────┼────────┐
     │              ▼        ▼        ▼
  所有任务      专业A   专业B   专业C
```

**关键能力：**
- 任务分解与分发
- Agent 间通信
- 结果汇总
- 容错处理

---

## 📚 理论基础 + 代码实现

### 1. Agent 基类设计

**理论：** 定义统一的 Agent 接口和行为

```cpp
// agent.hpp
struct Message {
    std::string id;
    std::string from;
    std::string to;           // 空表示广播
    std::string type;         // TASK_ASSIGN, TASK_RESULT, etc.
    std::string content;      // JSON 格式
    std::string parent_id;    // 父消息 ID
};

class Agent {
public:
    Agent(const std::string& name, const std::string& role)
        : name_(name), role_(role), running_(false) {}
    
    virtual ~Agent() { stop(); }
    
    void start() {
        running_ = true;
        worker_ = std::thread(&Agent::run, this);
    }
    
    void stop() {
        running_ = false;
        cv_.notify_all();
        if (worker_.joinable()) worker_.join();
    }
    
    // 接收消息
    void send_message(const Message& msg) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            inbox_.push(msg);
        }
        cv_.notify_one();
    }
    
    void set_send_callback(std::function<void(const Message&)> cb) {
        send_callback_ = cb;
    }
    
    std::string get_name() const { return name_; }
    std::string get_role() const { return role_; }

protected:
    virtual void process_message(const Message& msg) = 0;
    
    void send(const Message& msg) {
        if (send_callback_) send_callback_(msg);
    }
    
    void run() {
        while (running_) {
            Message msg;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock, [this] { 
                    return !inbox_.empty() || !running_; 
                });
                if (!running_) break;
                msg = inbox_.front();
                inbox_.pop();
            }
            process_message(msg);
        }
    }
    
    std::string name_, role_;
    bool running_;
    std::queue<Message> inbox_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::thread worker_;
    std::function<void(const Message&)> send_callback_;
};
```

### 2. 消息总线实现

**理论：** 实现 Agent 间的松耦合通信

```cpp
// message_bus.hpp
class MessageBus {
public:
    void register_agent(const std::string& name, 
                        std::shared_ptr<Agent> agent) {
        agents_[name] = agent;
        
        // 设置消息发送回调
        agent->set_send_callback([this](const Message& msg) {
            route_message(msg);
        });
        
        agent->start();
        std::cout << "[+] 注册 Agent: " << name << std::endl;
    }
    
    void route_message(const Message& msg) {
        if (msg.to.empty()) {
            broadcast(msg);
        } else {
            auto it = agents_.find(msg.to);
            if (it != agents_.end()) {
                it->second->send_message(msg);
            }
        }
    }
    
    void broadcast(const Message& msg) {
        for (const auto& [name, agent] : agents_) {
            if (name != msg.from) {
                agent->send_message(msg);
            }
        }
    }

private:
    std::map<std::string, std::shared_ptr<Agent>> agents_;
};
```

### 3. 协调器 Agent

**理论：** 负责任务分解和结果汇总

```cpp
// coordinator.hpp
class Coordinator : public Agent {
public:
    Coordinator() : Agent("coordinator", "coordinator") {}
    
    void register_agent_info(const std::string& name, 
                             const std::string& capability) {
        agent_capabilities_[name] = capability;
        agent_status_[name] = "idle";
    }
    
    // 分发任务
    void dispatch_task(const std::string& task_id,
                       const std::string& task,
                       const std::vector<std::string>& targets) {
        
        std::cout << "[协调器] 分发任务: " << task_id << "\n";
        
        // 记录任务
        pending_tasks_[task_id] = {
            {"task", task},
            {"targets", targets},
            {"completed", json::array()}
        };
        
        // 发送给目标 Agent
        for (const auto& agent_name : targets) {
            Message msg;
            msg.id = generate_id();
            msg.from = name_;
            msg.to = agent_name;
            msg.type = "TASK_ASSIGN";
            msg.content = json::serialize(json::object{
                {"task_id", task_id},
                {"task", task}
            });
            
            send(msg);
            agent_status_[agent_name] = "busy";
        }
    }

protected:
    void process_message(const Message& msg) override {
        if (msg.type == "TASK_RESULT") {
            handle_task_result(msg);
        }
    }
    
    void handle_task_result(const Message& msg) {
        auto data = json::parse(msg.content);
        std::string task_id = data["task_id"].as_string();
        
        // 收集结果
        pending_tasks_[task_id]["completed"].as_array().push_back(
            data["result"]
        );
        
        agent_status_[msg.from] = "idle";
        
        // 检查是否完成
        check_completion(task_id);
    }
    
    void check_completion(const std::string& task_id) {
        auto& task = pending_tasks_[task_id];
        size_t total = task["targets"].as_array().size();
        size_t completed = task["completed"].as_array().size();
        
        if (completed >= total) {
            std::cout << "[协调器] 任务完成: " << task_id << "\n";
            // 触发结果汇总
            finalize_task(task_id);
        }
    }
    
    void finalize_task(const std::string& task_id) {
        // 汇总所有 Agent 结果，生成最终回答
        // 实际项目中这里会调用 LLM 进行汇总
    }
    
    std::map<std::string, std::string> agent_capabilities_;
    std::map<std::string, std::string> agent_status_;
    std::map<std::string, json::value> pending_tasks_;
};
```

### 4. 专业 Agent 示例

**交通 Agent：**
```cpp
// transport_agent.hpp
class TransportAgent : public Agent {
public:
    TransportAgent() : Agent("transport", "transport") {}

protected:
    void process_message(const Message& msg) override {
        if (msg.type == "TASK_ASSIGN") {
            auto data = json::parse(msg.content);
            std::string task = data["task"].as_string();
            std::string task_id = data["task_id"].as_string();
            
            // 处理交通查询
            auto result = query_transport(task);
            
            // 返回结果
            Message response;
            response.id = generate_id();
            response.from = name_;
            response.to = "coordinator";
            response.type = "TASK_RESULT";
            response.content = json::serialize(json::object{
                {"task_id", task_id},
                {"agent", name_},
                {"result", result}
            });
            
            send(response);
        }
    }
    
    json::value query_transport(const std::string& query) {
        // 实际：调用航班/火车 API
        json::object result;
        result["flights"] = json::array{
            json::object{{"no", "CA1234"}, {"price", 1200}},
            json::object{{"no", "MU5678"}, {"price", 980}}
        };
        return result;
    }
};
```

### 5. 主程序集成

```cpp
// main.cpp
int main() {
    MessageBus bus;
    
    // 创建 Agent
    auto coordinator = std::make_shared<Coordinator>();
    auto transport = std::make_shared<TransportAgent>();
    auto hotel = std::make_shared<HotelAgent>();
    
    // 注册到总线
    bus.register_agent("coordinator", coordinator);
    bus.register_agent("transport", transport);
    bus.register_agent("hotel", hotel);
    
    // 注册能力
    coordinator->register_agent_info("transport", "交通查询");
    coordinator->register_agent_info("hotel", "酒店搜索");
    
    // 分发任务
    coordinator->dispatch_task(
        "trip_001",
        "北京到上海行程",
        {"transport", "hotel"}
    );
    
    return 0;
}
```

---

## 📋 Agent 开发检查清单

- [ ] Agent 间通信是否异步？
- [ ] 消息是否可序列化？
- [ ] 是否有超时机制？
- [ ] 是否处理 Agent 故障？
- [ ] 任务分解是否合理？

---

**下一步：** Step 12 配置管理
