# Step X: Background Agent - 定时任务与异步处理

## 位置建议

放在 **Step 9 之后**（安全沙箱之后）或 **Step 12 之后**（Channel 集成之后）。

推荐位置：**Step 10 和 Step 11 之间**

```
Step 9:  工具安全与沙箱
         ↓
Step 10: Background Agent（新增）← 工具系统延伸
         ↓
Step 11: 持久化与多租户（上）
```

## 为什么在这里？

1. **工具系统的延伸**：前面学会了即时工具调用，现在学习延迟/定时工具调用
2. **自然衔接**：用户可能说"明天提醒我"，这需要后台任务
3. **技术铺垫**：需要用到之前学的异步、定时器、安全沙箱

---

## 目标

实现一个后台任务系统，支持：
- 定时任务（Cron-like）
- 延迟任务（Delay queue）
- 任务持久化（重启不丢失）
- 任务执行隔离（安全）

---

## 演进关系

```
Step 6-9: 即时工具调用
          用户请求 → 立即执行 → 返回结果
          
Step 10:  Background Agent（延迟/定时执行）
          用户请求 → 安排任务 → 后台执行 → 异步通知
```

---

## 内容结构

### 第一部分：为什么需要后台任务？

**场景 1：定时任务**
```
用户：每天早上 9 点给我发天气简报
Agent：不是立即执行，而是安排一个定时任务
```

**场景 2：延迟任务**
```
用户：5 分钟后提醒我喝水
Agent：创建延迟任务，5分钟后触发
```

**场景 3：长时间任务**
```
用户：分析这个 100MB 的日志文件
Agent：不能在 WebSocket 连接期间阻塞，需要后台执行
完成后通过 WebSocket 推送结果
```

**场景 4：离线处理**
```
用户：监控这个网站，价格低于 100 时通知我
Agent：即使对话结束，后台持续监控
条件满足时主动推送消息
```

---

### 第二部分：Background Agent 架构

```
┌─────────────────────────────────────────────────────────────┐
│                    Background Agent                         │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐  │
│  │  任务队列     │    │   调度器      │    │   执行器      │  │
│  │  (Queue)     │───▶│  (Scheduler) │───▶│  (Executor)  │  │
│  └──────────────┘    └──────────────┘    └──────────────┘  │
│         │                                            │      │
│         │    ┌──────────────┐                       │      │
│         └───▶│  持久化存储   │◀──────────────────────┘      │
│              │ (SQLite/Redis)│                              │
│              └──────────────┘                              │
│                                                             │
│  ┌──────────────┐                                          │
│  │  结果通知     │ ── WebSocket/IM 推送 ──▶ 用户            │
│  └──────────────┘                                          │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

### 第三部分：核心组件实现

#### 1. Task 定义

```cpp
// task.hpp
enum class TaskType {
    ONCE,           // 执行一次
    DELAYED,        // 延迟执行
    RECURRING,      // 周期性（Cron）
    CONDITIONAL     // 条件触发
};

enum class TaskStatus {
    PENDING,        // 等待执行
    RUNNING,        // 执行中
    COMPLETED,      // 已完成
    FAILED,         // 失败
    CANCELLED       // 已取消
};

struct Task {
    std::string id;                 // 唯一标识
    std::string user_id;            // 所属用户
    TaskType type;                  // 任务类型
    TaskStatus status;              // 当前状态
    
    std::string description;        // 任务描述
    json::value tool_call;          // 要执行的工具调用
    
    // 调度相关
    std::chrono::system_clock::time_point scheduled_at;  // 计划执行时间
    std::optional<std::string> cron_expr;                // Cron 表达式（周期性任务）
    std::optional<json::value> condition;                // 触发条件（条件任务）
    
    // 执行相关
    std::optional<std::chrono::system_clock::time_point> started_at;
    std::optional<std::chrono::system_clock::time_point> completed_at;
    std::optional<json::value> result;   // 执行结果
    std::optional<std::string> error;    // 错误信息
    
    // 重试机制
    int retry_count = 0;
    int max_retries = 3;
};
```

#### 2. 任务调度器

```cpp
// scheduler.hpp
class TaskScheduler {
public:
    void schedule(Task task);
    void cancel(const std::string& task_id);
    std::vector<Task> get_pending_tasks();
    
    // Cron 解析
    std::chrono::system_clock::time_point next_run_time(
        const std::string& cron_expr,
        std::chrono::system_clock::time_point from = std::chrono::system_clock::now()
    );
    
private:
    std::priority_queue<Task, std::vector<Task>, TaskComparator> queue_;
    std::mutex mutex_;
    std::condition_variable cv_;
};
```

#### 3. 后台执行引擎

```cpp
// background_engine.hpp
class BackgroundEngine {
public:
    BackgroundEngine(ToolRegistry& tools, NotificationService& notifier);
    
    // 启动后台线程
    void start();
    void stop();
    
    // 任务管理
    std::string submit_task(const Task& task);
    Task get_task(const std::string& task_id);
    
private:
    void worker_loop();           // 工作线程主循环
    void execute_task(Task& task); // 执行单个任务
    
    ToolRegistry& tools_;
    NotificationService& notifier_;
    TaskScheduler scheduler_;
    std::vector<std::thread> workers_;
    bool running_ = false;
};
```

#### 4. 通知服务

```cpp
// notification.hpp
class NotificationService {
public:
    // WebSocket 推送
    void register_websocket(const std::string& user_id, WsConnectionPtr conn);
    void unregister_websocket(const std::string& user_id);
    
    // IM 推送（飞书/钉钉）
    void send_im_message(const std::string& user_id, const std::string& message);
    
    // 邮件/短信（扩展）
    void send_email(const std::string& email, const std::string& subject, const std::string& body);
    
    // 通用通知
    void notify(const std::string& user_id, const json::value& notification);
    
private:
    std::map<std::string, WsConnectionPtr> ws_connections_;
    std::map<std::string, IMChannelPtr> im_channels_;
};
```

---

### 第四部分：与主系统的集成

```cpp
// chat_engine.hpp（Step 6 的扩展）
class ChatEngine {
public:
    ChatEngine(LLMClient& llm, ToolRegistry& tools, BackgroundEngine& bg);
    
    std::string process(const std::string& input, Context& ctx);
    
private:
    LLMClient& llm_;
    ToolRegistry& tools_;
    BackgroundEngine& bg_;  // 新增
    
    // 检测是否需要后台任务
    bool is_background_task(const std::string& input);
    
    // 创建任务
    Task create_task_from_request(const std::string& input, const Context& ctx);
};

// 使用示例
std::string ChatEngine::process(const std::string& input, Context& ctx) {
    if (is_background_task(input)) {
        Task task = create_task_from_request(input, ctx);
        std::string task_id = bg_.submit_task(task);
        return "已创建任务 " + task_id + "，将在指定时间执行";
    }
    
    // 即时处理（原有逻辑）
    return process_immediately(input, ctx);
}
```

---

### 第五部分：典型任务示例

#### 示例 1：定时天气简报

```cpp
// 用户请求
"每天早上 9 点给我发北京天气"

// LLM 解析为
Task{
    .type = RECURRING,
    .cron_expr = "0 9 * * *",  // Cron: 每天9点
    .tool_call = {
        {"tool", "get_weather"},
        {"args", {{"city", "北京"}}}
    },
    .notification_template = "早安！北京天气：{result.weather}，{result.temp}°C"
}
```

#### 示例 2：延迟提醒

```cpp
// 用户请求
"10 分钟后提醒我开会"

// LLM 解析为
Task{
    .type = DELAYED,
    .scheduled_at = now() + 10min,
    .tool_call = {
        {"tool", "send_notification"},
        {"args", {{"message", "该开会了！"}}}
    }
}
```

#### 示例 3：条件监控

```cpp
// 用户请求
"监控比特币价格，低于 30000 美元时通知我"

// LLM 解析为
Task{
    .type = CONDITIONAL,
    .cron_expr = "*/5 * * * *",  // 每5分钟检查
    .condition = {
        {"tool", "get_crypto_price"},
        {"args", {{"symbol", "BTCUSD"}}},
        {"operator", "<"},
        {"value", 30000}
    },
    .notification_template = "比特币价格预警：当前 {result.price} USD"
}
```

---

### 第六部分：安全考虑

```cpp
// 任务执行沙箱
class TaskSandbox {
public:
    // 限制任务执行时间
    void set_timeout(std::chrono::seconds timeout);
    
    // 限制任务资源
    void set_memory_limit(size_t bytes);
    void set_cpu_limit(double percentage);
    
    // 任务隔离
    void execute_isolated(const Task& task);
};

// 防止滥用
class TaskQuotaManager {
public:
    bool can_submit(const std::string& user_id);
    int get_remaining_quota(const std::string& user_id);
    
private:
    // 每用户限制
    static constexpr int MAX_TASKS_PER_USER = 100;
    static constexpr int MAX_RECURRING_TASKS = 10;
};
```

---

## 代码演进

```
Step 9: 工具安全（即时执行）
         ↓
Step 10: Background Agent（延迟/定时执行）
         新增：Task 定义、Scheduler、BackgroundEngine
         修改：ChatEngine 集成任务提交
         代码量：~800 行（模块化）
```

## 与前后章的衔接

### Step 9 → Step 10
```
Step 9 问题：工具只能即时执行
Step 10 解决：支持延迟、定时、周期性执行
保留：工具注册表、安全沙箱
新增：任务调度、持久化、通知
```

### Step 10 → Step 11
```
Step 10 问题：任务存储在内存，重启丢失
Step 11 解决：SQLite 持久化
衔接：任务队列 → 数据库存储
```

## 教程大纲

1. **为什么需要后台任务**（场景驱动）
2. **任务类型详解**（一次性、延迟、周期、条件）
3. **核心架构设计**（队列、调度器、执行器）
4. **Cron 表达式**（定时任务语法）
5. **任务持久化**（内存 → 数据库铺垫）
6. **结果通知机制**（WebSocket 推送、IM 消息）
7. **安全与隔离**（超时、资源限制、配额）
8. **实战：天气简报机器人**（完整示例）

## 检查点

- [ ] 任务能定时执行
- [ ] 任务能延迟执行
- [ ] 任务持久化（为 Step 11 铺垫）
- [ ] 结果能通知到用户
- [ ] 有安全限制（超时、配额）
