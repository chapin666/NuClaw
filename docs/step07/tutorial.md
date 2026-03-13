# Step 7: 异步工具执行 - 基于 Step 6 演进

> 目标：解决 Step 6 同步工具阻塞问题
> 
> 难度：⭐⭐⭐ (中等)
> 
> 代码量：约 600 行（分散在 10 个文件中）

## 📁 代码结构说明（演进式）

Step 7 完全基于 Step 6 的模块化结构：**文件列表相同，关键文件演进**

```
src/step07/                    src/step06/
├── main.cpp              (修改)     main.cpp
├── tool.hpp              (相同)     tool.hpp
├── weather_tool.hpp      (相同)     weather_tool.hpp
├── time_tool.hpp         (相同)     time_tool.hpp
├── calc_tool.hpp         (相同)     calc_tool.hpp
├── tool_executor.hpp     (演进)     tool_executor.hpp
├── llm_client.hpp        (相同)     llm_client.hpp
├── chat_engine.hpp       (演进)     chat_engine.hpp
└── server.hpp            (相同)     server.hpp
```

### 与 Step 6 的关系

**演进统计：**
- 8 个文件**完全相同**（从 Step 6 复制）
- 2 个文件**演进修改**（`tool_executor.hpp`, `chat_engine.hpp`）
- 1 个文件**重写**（`main.cpp` 演示代码）

**演进对比：**

```cpp
// Step 6 tool_executor.hpp - 同步执行
class ToolExecutor {
public:
    static ToolResult execute(const ToolCall& call);  // 同步阻塞
};

// Step 7 tool_executor.hpp - 异步执行  
class ToolExecutor {
public:
    // 保留同步接口（兼容）
    static ToolResult execute_sync(const ToolCall& call);
    
    // 新增异步接口（演进）
    void execute_async(const ToolCall& call, 
                       AsyncCallback callback,
                       std::chrono::milliseconds timeout);
    
    // 新增并发控制
    size_t get_running_count() const;
    size_t get_queue_length() const;
};
```

```cpp
// Step 6 chat_engine.hpp - 同步处理
class ChatEngine {
    std::string process(const std::string& input, ChatContext& ctx);
};

// Step 7 chat_engine.hpp - 异步处理
class ChatEngine {
    // 保留同步接口（兼容）
    std::string process(const std::string& input, ChatContext& ctx);
    
    // 新增异步接口（演进）
    void process_async(const std::string& input, 
                       ChatContext& ctx,
                       std::function<void(std::string)> callback);
};
```

### 为什么保持相同结构？

**演进式学习的优势：**
1. **熟悉感**：读者已经认识这些文件，不需要重新学习结构
2. **聚焦**：只关注变化的部分（异步执行），不被新文件分散注意力
3. **可追溯**：`git diff step06 step07` 清晰展示演进过程

---

## 本节收获

- 理解同步 vs 异步执行的本质区别
- 掌握 C++ 回调机制（std::function + lambda）
- 实现工具级别的并发控制
- 理解线程安全和任务队列

---

## 第一步：回顾 Step 6 的问题

Step 6 实现了工具调用，但是**同步阻塞**的：

```cpp
// 同步执行
std::string process(std::string input) {
    if (needs_tool(input)) {
        auto result = tool_executor.execute(call);  // ← 阻塞！
        // 等待工具执行完成才能继续
        return generate_reply(result);
    }
}
```

**问题场景：**
```
用户问：北京天气如何？
→ 调用天气 API（需要 2 秒）
→ 这 2 秒内，服务器无法处理其他请求！

如果有 10 个用户同时提问：
- 同步模式：需要 20 秒才能全部响应
- 异步模式：只需要 2 秒（并行执行）
```

---

## 第二步：异步解决方案

### 核心思想

不等待工具完成，**立即返回**，完成后通过**回调**通知：

```cpp
// 异步执行
void process_async(string input, function<void(string)> callback) {
    if (needs_tool(input)) {
        // 启动异步执行，不等待
        tool_executor.execute_async(call, 
            // Lambda 回调：工具完成后调用
            [callback](ToolResult result) {
                string reply = generate_reply(result);
                callback(reply);  // 通知调用者
            }
        );
        // 立即返回，不阻塞！
    }
}
```

### 关键技术

**1. std::function - 可调用对象包装**
```cpp
// 可以存储任何可调用对象
std::function<void(int)> callback;

// 存储 Lambda
callback = [](int x) { cout << x; };

// 存储普通函数
callback = &print_number;

// 存储成员函数（用 bind）
callback = std::bind(&Class::method, obj, placeholders::_1);
```

**2. Lambda 捕获列表**
```cpp
// [=] 捕获所有变量（按值）
// [&] 捕获所有变量（按引用）
// [this] 捕获当前对象
// [a, &b] 捕获 a 按值，b 按引用

tool_executor.execute_async(call, 
    [&chat_engine, &ctx, callback](ToolResult result) {
        // 捕获 chat_engine 和 ctx 的引用
        // 捕获 callback 的值
        string reply = chat_engine.generate(result);
        callback(reply);
    }
);
```

**3. 并发控制 - Semaphore 模式**
```cpp
class ToolExecutor {
    size_t max_concurrent_ = 3;  // 最多同时执行 3 个
    size_t running_count_ = 0;    // 当前运行数
    queue<PendingTask> queue_;     // 等待队列
    
    void execute_async(call, callback) {
        if (running_count_ >= max_concurrent_) {
            // 队列已满，加入等待
            queue_.push({call, callback});
            return;
        }
        
        // 立即执行
        running_count_++;
        std::thread([this, call, callback]() {
            auto result = execute(call);
            callback(result);
            running_count_--;
            
            // 检查等待队列
            if (!queue_.empty()) {
                auto next = queue_.front();
                queue_.pop();
                execute_async(next.call, next.callback);
            }
        }).detach();
    }
};
```

---

## 第三步：代码演进详解

### tool_executor.hpp 演进

```cpp
// Step 6: 只有同步执行
class ToolExecutor {
public:
    static ToolResult execute(const ToolCall& call) {
        // 直接执行，阻塞等待
        return execute_tool(call);
    }
};

// Step 7: 同步 + 异步
class ToolExecutor {
public:
    // 保留同步接口（向后兼容）
    static ToolResult execute_sync(const ToolCall& call);
    
    // 新增异步接口
    void execute_async(const ToolCall& call,
                       AsyncCallback<ToolResult> callback,
                       std::chrono::milliseconds timeout);
    
    // 新增状态查询
    size_t get_running_count() const;  // 当前运行数
    size_t get_queue_length() const;   // 队列长度
    
private:
    size_t max_concurrent_ = 3;
    std::atomic<size_t> running_count_{0};
    std::queue<PendingTask> pending_queue_;
    std::mutex mutex_;
};
```

### chat_engine.hpp 演进

```cpp
// Step 6: 只有同步处理
class ChatEngine {
public:
    std::string process(const std::string& input, ChatContext& ctx) {
        if (llm_.needs_tool(input)) {
            auto result = executor_.execute(call);  // 阻塞
            return generate_reply(result);
        }
        return llm_.direct_reply(input);
    }
};

// Step 7: 同步 + 异步
class ChatEngine {
public:
    // 保留同步接口
    std::string process(const std::string& input, ChatContext& ctx);
    
    // 新增异步接口
    void process_async(const std::string& input,
                       ChatContext& ctx,
                       std::function<void(std::string)> callback) {
        if (llm_.needs_tool(input)) {
            // 异步执行工具
            executor_.execute_async(call,
                [this, callback](ToolResult result) {
                    // 工具完成后回调
                    callback(generate_reply(result));
                },
                std::chrono::seconds(3)  // 3秒超时
            );
        } else {
            // 不需要工具，立即回调
            callback(llm_.direct_reply(input));
        }
    }
};
```

---

## 第四步：运行测试

### 编译运行

```bash
# Step 7 目录
cd src/step07
mkdir build && cd build
cmake .. && make
./nuclaw_step07
```

### 异步执行演示

```bash
wscat -c ws://localhost:8080/ws

# 快速连续发送多个请求
> 北京天气
> 上海天气  
> 现在几点
> 25 * 4

# 观察输出：
[🧠 Processing async] "北京天气"
  → Will call tool: get_weather
[🧠 Processing async] "上海天气"
  → Will call tool: get_weather
[🧠 Processing async] "现在几点"
  → Will call tool: get_time
[🧠 Processing async] "25 * 4"
  → Will call tool: calculate

[📊 Executor] Running: 3, Queued: 1  ← 并发控制！
  [✓] 北京 completed
  [✓] 上海 completed
[📊 Executor] Running: 2, Queued: 0
  [✓] 现在几点 completed
  [✓] 25 * 4 completed
```

### 并发效果对比

| 模式 | 4 个工具执行时间 | 说明 |
|:---|:---|:---|
| Step 6 同步 | 400ms | 串行执行 |
| Step 7 异步 | 100ms | 4 个并行 |

---

## 本节总结

### 我们解决了什么？

**Step 6 的问题：同步阻塞**
- 工具执行时无法处理其他请求
- 并发能力差

**Step 7 的解决：异步执行**
- 立即返回，不阻塞
- 并发控制（限制同时执行数）
- 超时保护

### 代码演进

```
Step 6: 工具调用 (550行)
   ↓ 演进
Step 7: + 异步执行 (600行)
   - tool_executor.hpp: +execute_async(), +并发控制
   - chat_engine.hpp: +process_async()
```

### 文件演进总结

| 文件 | 状态 | 说明 |
|:---|:---|:---|
| main.cpp | 重写 | 演示异步执行 |
| tool_executor.hpp | 演进 | +异步 +并发控制 +超时 |
| chat_engine.hpp | 演进 | +process_async() |
| 其他 8 个文件 | 相同 | 从 Step 6 复用 |

### 仍存在的问题

**并发但没有安全控制：**
- HTTP 工具可能访问内网（SSRF）
- 文件工具可能读取敏感文件
- 代码工具可能执行危险代码

**下一章：工具安全与沙箱**
- URL 白名单
- 路径沙箱
- 代码黑名单

---

## 附：演进式代码的价值

通过 Step 6 → 7 的学习，你应该体会到：

1. **结构熟悉**：相同的文件列表，降低认知负担
2. **变更聚焦**：只看修改的文件，知道学什么
3. **可追溯**：git diff 清晰展示演进过程

这就是**渐进式学习**的设计理念。
