# Step 7: 异步工具执行 - 基于 Step 6 演进

> 目标：理解异步编程，解决同步阻塞问题，实现并发控制
> 
> 难度：⭐⭐⭐⭐ (较难)
> 
> 代码量：约 600 行（分散在 10 个文件中）
> 
> 预计学习时间：4-5 小时

---

## 📚 前置知识

### 同步 vs 异步

**同步（Synchronous）：**
```
程序：调用函数 → 等待结果 → 继续执行
生活：排队买咖啡 → 等待制作 → 拿到咖啡 → 去上班
```

**异步（Asynchronous）：**
```
程序：调用函数 → 立即返回 → 结果通过回调通知 → 继续执行
生活：手机下单咖啡 → 去上班 → 咖啡做好了推送通知 → 顺路取咖啡
```

### 为什么需要异步？

**Step 6 的问题：**

```cpp
// 同步执行
std::string process(std::string input) {
    if (needs_tool(input)) {
        auto result = tool_executor.execute(call);  // ← 阻塞！
        // 如果工具执行需要 3 秒，这里就卡住 3 秒
        return generate_reply(result);
    }
}
```

**场景分析：**

```
用户 A：查询北京天气（需要调用天气 API，耗时 2 秒）
用户 B：查询上海天气（需要调用天气 API，耗时 2 秒）
用户 C：查询时间（本地计算，耗时 0.01 秒）

同步模式：
时间 0s:  处理 A 的请求，开始调用天气 API
时间 2s:  A 完成，开始处理 B 的请求
时间 4s:  B 完成，开始处理 C 的请求
时间 4s:  C 完成
总计：4 秒

异步模式：
时间 0s:  启动 A 的天气 API 调用（不等待）
时间 0s:  启动 B 的天气 API 调用（不等待）
时间 0s:  处理 C 的请求（立即完成）
时间 2s:  A 和 B 同时完成
总计：2 秒
```

**异步的优势：**
- **不阻塞**：发起调用后立即返回，可以继续处理其他事情
- **并发性**：多个任务可以同时进行
- **资源利用率高**：等待 IO 时可以做其他计算

### 异步编程的核心概念

#### 1. 回调函数（Callback）

**定义**：异步操作完成后调用的函数。

```cpp
// 同步
int result = add(1, 2);  // 立即得到结果
std::cout << result;      // 输出 3

// 异步
add_async(1, 2, [](int result) {  // 传入回调函数
    std::cout << result;           // 异步完成后输出
});
// 这里立即继续执行，不等待结果
```

#### 2. Lambda 表达式

**定义**：匿名函数，可以捕获外部变量。

```cpp
// 基本语法
[捕获列表](参数列表) { 函数体 }

// 示例
int x = 10;

// [=] 按值捕获所有变量
auto lambda1 = [=]() { return x; };  // x 是 10 的副本

// [&] 按引用捕获所有变量
auto lambda2 = [&]() { x = 20; };     // 修改外部的 x

// [x] 只捕获 x（按值）
auto lambda3 = [x]() { return x; };

// [x, &y] 捕获 x 按值，y 按引用
auto lambda4 = [x, &y]() { y = x; };
```

**在异步编程中的作用：**

```cpp
void process_async(string input, function<void(string)> callback) {
    // 启动异步任务
    std::thread([input, callback]() {
        // 在新线程中执行耗时操作
        string result = do_work(input);
        
        // 完成后调用回调
        callback(result);
    }).detach();
}

// 使用
process_async("hello", [](string result) {
    cout << "结果: " << result << endl;
});
```

#### 3. std::function

**定义**：可以存储任何可调用对象的通用类型。

```cpp
// 定义回调类型
using Callback = std::function<void(const std::string&)>;

// 可以存储：
Callback cb1 = [](const std::string& s) { cout << s; };
Callback cb2 = &process_result;  // 普通函数
Callback cb3 = std::bind(&Class::method, obj, placeholders::_1);  // 成员函数
```

#### 4. std::thread 和 std::async

**创建线程：**

```cpp
// 方法 1：直接创建线程
std::thread t([]() {
    // 在新线程中执行
    do_work();
});
t.detach();  // 分离线程，主线程不等待

// 方法 2：使用 async（推荐）
auto future = std::async(std::launch::async, []() {
    return do_work();  // 在新线程中执行，返回结果
});
int result = future.get();  // 获取结果（会阻塞直到完成）
```

#### 5. 并发控制 - Semaphore 思想

**问题：** 如果同时有 1000 个请求，要启动 1000 个线程吗？

**解决方案：** 限制并发数

```cpp
class ConcurrentController {
    size_t max_concurrent_ = 10;  // 最多同时执行 10 个
    size_t running_count_ = 0;     // 当前运行数
    std::queue<Task> waiting_queue_;  // 等待队列
    
public:
    void submit(Task task) {
        if (running_count_ < max_concurrent_) {
            // 立即执行
            running_count_++;
            execute_async(task, [this]() {
                running_count_--;
                process_queue();  // 检查等待队列
            });
        } else {
            // 加入等待队列
            waiting_queue_.push(task);
        }
    }
};
```

---

## 第一步：深刻理解同步的问题

### Step 6 的性能瓶颈

```cpp
// Step 6 的同步执行
class ToolExecutor {
public:
    static ToolResult execute(const ToolCall& call) {
        // 模拟耗时操作（如调用外部 API）
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        if (call.name == "get_weather") {
            return WeatherTool::execute(call.arguments);
        }
        // ...
    }
};
```

**问题场景：**

```
用户 1: 查询北京天气
        ↓
        调用天气 API（阻塞 2 秒）
        ↓
        返回结果
        
用户 2: 查询上海天气
        ↓
        等待用户 1 完成（2 秒后）
        ↓
        调用天气 API（阻塞 2 秒）
        ↓
        返回结果（总共等待 4 秒）
        
用户 3: 查询时间
        ↓
        等待用户 2 完成（4 秒后）
        ↓
        本地计算（0.01 秒）
        ↓
        返回结果（总共等待 4 秒）
```

**即使本地计算很快，也要等前面的慢操作完成！**

### 生活中的类比

**同步 = 只有一个窗口的银行：**
```
客户 A：办复杂业务（30 分钟）
客户 B：存 100 元（1 分钟）
客户 C：查询余额（1 分钟）

客户 B 和 C 必须等 A 完成，即使他们很快。
```

**异步 = 取号排队系统：**
```
客户 A：取号 → 等待 → 办业务（30 分钟）
客户 B：取号 → 等待 → 办业务（1 分钟）
客户 C：取号 → 等待 → 办业务（1 分钟）

三个人同时等待，先完成的先办理。
```

---

## 第二步：异步执行设计

### 核心思想

```
旧流程（同步）：
用户请求 → 处理 → 调用工具（阻塞等待）→ 返回结果

新流程（异步）：
用户请求 → 处理 → 启动工具（立即返回）→ 继续处理其他请求
                    ↓
              工具完成后回调 → 发送结果给用户
```

### 代码演进

**Step 6（同步）：**

```cpp
class ToolExecutor {
public:
    // 同步执行 - 阻塞直到完成
    static ToolResult execute(const ToolCall& call) {
        std::this_thread::sleep_for(2s);  // 模拟耗时
        return execute_tool(call);
    }
};
```

**Step 7（异步）：**

```cpp
class ToolExecutor {
public:
    // 保留同步接口（向后兼容）
    static ToolResult execute_sync(const ToolCall& call);
    
    // 新增异步接口
    void execute_async(const ToolCall& call,
                       AsyncCallback<ToolResult> callback,
                       std::chrono::milliseconds timeout);
    
    // 新增并发控制
    size_t get_running_count() const;  // 当前运行数
    size_t get_queue_length() const;   // 队列长度
};
```

### 完整实现

```cpp
// tool_executor.hpp - 异步版本
class ToolExecutor {
public:
    ToolExecutor(size_t max_concurrent = 3) 
        : max_concurrent_(max_concurrent), running_count_(0) {}
    
    // 异步执行
    void execute_async(const ToolCall& call, 
                       AsyncCallback<ToolResult> callback,
                       std::chrono::milliseconds timeout_ms = std::chrono::seconds(5)) {
        
        // 1. 检查并发限制
        {
            std::unique_lock<std::mutex> lock(mutex_);
            if (running_count_ >= max_concurrent_) {
                // 队列已满，加入等待
                std::cout << "[⏳ 排队] " << call.name << std::endl;
                pending_queue_.push({call, callback, timeout_ms});
                return;
            }
            running_count_++;
        }
        
        // 2. 立即执行（不阻塞）
        std::cout << "[⚙️ 执行] " << call.name << " (" << running_count_ << "/" << max_concurrent_ << ")" << std::endl;
        
        // 3. 在新线程中执行
        std::thread([this, call, callback, timeout_ms]() {
            // 执行工具（带超时）
            auto result = execute_with_timeout(call, timeout_ms);
            
            // 回调通知结果
            callback(result);
            
            // 4. 完成，检查队列
            {
                std::unique_lock<std::mutex> lock(mutex_);
                running_count_--;
                
                if (!pending_queue_.empty()) {
                    auto next = pending_queue_.front();
                    pending_queue_.pop();
                    lock.unlock();
                    
                    // 递归处理队列中的下一个
                    execute_async(next.call, next.callback, next.timeout);
                }
            }
        }).detach();  // 分离线程，独立运行
    }
    
    // 带超时的执行
    static ToolResult execute_with_timeout(const ToolCall& call,
                                            std::chrono::milliseconds timeout_ms) {
        // 使用 std::async 和 future 实现超时
        auto future = std::async(std::launch::async, [call]() {
            return execute_tool(call);  // 实际执行
        });
        
        // 等待结果，带超时
        if (future.wait_for(timeout_ms) == std::future_status::timeout) {
            return ToolResult::fail("执行超时");
        }
        
        return future.get();
    }

private:
    struct PendingTask {
        ToolCall call;
        AsyncCallback<ToolResult> callback;
        std::chrono::milliseconds timeout;
    };
    
    size_t max_concurrent_;
    std::atomic<size_t> running_count_{0};
    mutable std::mutex mutex_;
    std::queue<PendingTask> pending_queue_;
};
```

---

## 第三步：ChatEngine 演进

### 同步接口（保留兼容）

```cpp
class ChatEngine {
public:
    // Step 6 的同步接口
    std::string process(const std::string& user_input, ChatContext& ctx) {
        ctx.message_count++;
        
        if (llm_.needs_tool(user_input)) {
            ToolCall call = llm_.parse_tool_call(user_input);
            ToolResult result = ToolExecutor::execute_sync(call);  // 同步
            return llm_.generate_response(user_input, result, call);
        }
        
        return llm_.direct_reply(user_input);
    }
};
```

### 新增异步接口

```cpp
class ChatEngine {
public:
    // 同步接口（保留）
    std::string process(const std::string& user_input, ChatContext& ctx);
    
    // 新增异步接口
    void process_async(const std::string& user_input, 
                       ChatContext& ctx,
                       std::function<void(std::string)> callback) {
        ctx.message_count++;
        
        if (llm_.needs_tool(user_input)) {
            ToolCall call = llm_.parse_tool_call(user_input);
            
            // 异步执行工具
            executor_.execute_async(call, 
                [this, user_input, call, callback, &ctx](ToolResult result) {
                    // 工具完成后，生成回复
                    std::string reply = llm_.generate_response(user_input, result, call);
                    
                    // 保存历史
                    ctx.history.push_back({"user", user_input});
                    ctx.history.push_back({"assistant", reply});
                    
                    // 回调通知
                    callback(reply);
                },
                std::chrono::seconds(3)  // 3秒超时
            );
        }
        else {
            // 不需要工具，直接回复
            std::string reply = llm_.direct_reply(user_input);
            ctx.history.push_back({"user", user_input});
            ctx.history.push_back({"assistant", reply});
            callback(reply);
        }
    }
    
    // 查询执行器状态
    void print_status() const {
        std::cout << "[📊 状态] 运行中: " << executor_.get_running_count()
                  << ", 排队: " << executor_.get_queue_length() << std::endl;
    }

private:
    LLMClient llm_;
    ToolExecutor executor_{3};  // 最多3个并发
};
```

---

## 第四步：运行测试

### 编译

```bash
cd src/step07
mkdir build && cd build
cmake .. && make
./nuclaw_step07
```

### 并发测试

```bash
wscat -c ws://localhost:8080/ws

# 快速发送多个请求
> 北京天气
> 上海天气
> 广州天气
> 现在几点

# 观察输出：
[🧠 处理 async] "北京天气"
  → 需要工具
  → 将执行: get_weather
[🧠 处理 async] "上海天气"
  → 需要工具
  → 将执行: get_weather
[🧠 处理 async] "广州天气"
  → 需要工具
  → 将执行: get_weather
[🧠 处理 async] "现在几点"
  → 需要工具
  → 将执行: get_time

[📊 状态] 运行中: 3, 排队: 1
  [⚙️ 执行] get_weather (1/3)
  [⚙️ 执行] get_weather (2/3)
  [⚙️ 执行] get_weather (3/3)
  [⏳ 排队] get_time

[📊 状态] 运行中: 3, 排队: 0
  [✓] 北京天气 完成
  [✓] 上海天气 完成
  [✓] 广州天气 完成
  [⚙️ 执行] get_time (3/3)

[✓] 现在几点 完成
```

### 性能对比

| 模式 | 4 个工具请求 | 说明 |
|:---|:---|:---|
| Step 6 同步 | 8 秒 | 串行执行 |
| Step 7 异步 | 2 秒 | 3 个并行 + 1 个排队 |

**性能提升 4 倍！**

---

## 本节总结

### 核心概念

1. **异步编程**：不阻塞，立即返回，回调通知结果
2. **Lambda 捕获**：在异步回调中访问外部变量
3. **并发控制**：限制同时执行的任务数，防止资源耗尽
4. **超时机制**：防止任务无限执行

### 代码演进

```
Step 6: 同步工具执行 (550行)
   ↓ 演进
Step 7: 异步工具执行 (600行)
   - tool_executor.hpp: +execute_async() +并发控制 +超时
   - chat_engine.hpp: +process_async()
```

### 演进统计

| 文件 | 状态 | 变化 |
|:---|:---:|:---|
| tool.hpp | 相同 | 无 |
| *_tool.hpp | 相同 | 无 |
| llm_client.hpp | 相同 | 无 |
| server.hpp | 相同 | 无 |
| chat_engine.hpp | 演进 | +process_async() |
| tool_executor.hpp | 演进 | +异步执行 +并发控制 |
| main.cpp | 重写 | 演示异步 |

### 仍存在的问题

**没有安全控制：**
- 工具可能被滥用
- 需要权限控制、沙箱隔离

**下一章：工具安全与沙箱**
- URL 白名单
- 路径沙箱
- 代码黑名单

---

## 📝 课后练习

### 练习 1：调整并发数
测试不同并发数的效果：
- max_concurrent = 1（串行）
- max_concurrent = 3（默认）
- max_concurrent = 10（高并发）

### 练习 2：实现优先级队列
让某些工具优先执行：
- 快速工具（如时间查询）优先
- 慢速工具（如天气查询）后排

### 练习 3：取消任务
实现任务取消机制：
- 用户可以取消正在执行的工具
- 超时自动取消

### 思考题
1. 异步编程有什么缺点？
2. 什么时候应该用同步，什么时候用异步？
3. 如何调试异步代码？

---

## 📖 扩展阅读

### 异步编程模式

| 模式 | 说明 | 适用场景 |
|:---|:---|:---|
| **Callback** | 回调函数 | 简单场景 |
| **Promise/Future** | 期约模式 | 需要传递结果 |
| **async/await** | 协程（C++20）| 代码可读性要求高 |
| **Reactive** | 响应式编程 | 流式数据处理 |

### C++ 异步库

- **std::thread**：标准线程库
- **std::async**：简化异步任务
- **Boost.Asio**：高性能异步 IO
- **libuv**：跨平台异步 IO（Node.js 底层）

---

**恭喜！** 你的 Agent 现在支持异步执行了。下一章我们将添加安全控制，防止工具被滥用。
