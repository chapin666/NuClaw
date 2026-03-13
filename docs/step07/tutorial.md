# Step 7: Tools 系统（中）- 异步工具执行

> 目标：实现异步工具执行，解决同步工具的阻塞问题
> 
> 难度：⭐⭐⭐ (中等)
003e 
003e 代码量：约 560 行

## 本节收获

- 理解同步 vs 异步执行的本质区别
- 掌握 C++ 回调机制（std::function + lambda）
- 实现工具级别的并发控制
- 理解线程安全和任务队列
- 掌握 std::async 和 std::future 的使用

---

## 第一部分：为什么需要异步？

### 1.1 同步工具的问题

**Step 6 的问题：**

```cpp
// 同步执行：阻塞当前线程直到完成
json::value result = tool->execute(args);
// ↑ 如果工具执行需要 5 秒，这里就阻塞 5 秒
```

**实际场景：**

```
用户请求 ──> Agent ──> HTTP 工具（调用外部 API，2秒）
                ↑
                └── 这2秒内，Agent 无法处理其他请求！

如果有 100 个并发用户：
- 同步模式：需要 100 个线程，大部分时间在等待
- 异步模式：1 个线程可以处理所有请求
```

### 1.2 同步 vs 异步 对比

| 特性 | 同步 | 异步 |
|:---|:---|:---|
| 执行方式 | 阻塞等待 | 立即返回 |
| 资源利用 | 低（等待时占用线程） | 高（线程不等待） |
| 并发能力 | 差 | 好 |
| 代码复杂度 | 简单 | 较复杂 |
| 适用场景 | 本地计算 | I/O 操作（网络、文件） |

### 1.3 异步解决方案

```cpp
// 异步执行：立即返回，结果通过回调通知
void execute_async(const json::object& args, 
                   function<void(json::value)> callback);

// 使用方式
tool->execute_async(args, [](json::value result) {
    // 结果准备好时自动调用
    cout << "Result: " << result << endl;
});
// ↑ 立即返回，不阻塞！
```

**流程对比：**

```
同步模式：
调用 ──> 执行（阻塞等待）──> 返回结果 ──> 继续处理
         ↑ 线程卡住

异步模式：
调用 ──> 启动执行（立即返回）
         ↓
      后台线程执行
         ↓
      完成 ──> 回调通知 ──> 处理结果
```

---

## 第二部分：C++ 回调机制

### 2.1 std::function - 可调用对象的包装

```cpp
// 可以存储任何可调用对象
function<void(int)> callback;

// 1. Lambda
callback = [](int x) { cout << x; };

// 2. 普通函数
void print(int x) { cout << x; }
callback = print;

// 3. 成员函数（通过 bind）
class Handler {
    void on_result(int x) { ... }
};
Handler h;
callback = bind(&Handler::on_result, &h, placeholders::_1);
```

### 2.2 Lambda 表达式详解

```cpp
// 基本语法：[捕获列表](参数列表) -> 返回类型 { 函数体 }

// 示例 1：简单 lambda
auto add = [](int a, int b) { return a + b; };

// 示例 2：捕获外部变量
int factor = 2;
auto multiply = [factor](int x) { return x * factor; };

// 示例 3：引用捕获（可以修改外部变量）
int count = 0;
auto increment = [&count]() { count++; };

// 示例 4：异步回调的典型用法
void fetch_data(function<void(string)> callback) {
    thread([callback]() {
        string data = "result";
        callback(data);  // 异步返回结果
    }).detach();
}
```

**捕获列表：**
| 语法 | 含义 |
|:---|:---|
| `[]` | 不捕获任何变量 |
| `[=]` | 按值捕获所有变量 |
|`[&]`| 按引用捕获所有变量 |
|`[x]`| 只捕获 x（按值）|
|`[&x]`| 只捕获 x（按引用）|
|`[=, &x]`| 默认按值，x 按引用 |
|`[this]`| 捕获当前对象指针 |

### 2.3 异步回调模式

```cpp
class Tool {
public:
    // 纯虚函数：子类实现异步逻辑
    virtual void execute_async(
        const json::object& args,
        function<void(json::value)> callback
    ) = 0;
    
    // 同步包装：异步转同步
    json::value execute_sync(const json::object& args) {
        promise<json::value> promise;
        auto future = promise.get_future();
        
        execute_async(args, [&promise](json::value result) {
            promise.set_value(move(result));
        });
        
        return future.get();  // 阻塞等待
    }
};
```

---

## 第三部分：并发控制

### 3.1 为什么要限制并发？

**问题场景：**

```
用户同时发送 100 个 HTTP 请求
- 如果不限制：创建 100 个线程
- 后果：系统资源耗尽，崩溃

解决方案：限制同时执行的工具数量
```

### 3.2 任务队列 + 并发限制

```cpp
class ToolExecutor {
public:
    ToolExecutor(size_t max_concurrent = 4);
    
    void submit(shared_ptr<Tool> tool, 
                const json::object& args,
                function<void(json::value)> callback);

private:
    size_t max_concurrent_;         // 最大并发数
    size_t running_;                // 当前运行数
    queue<Task> queue_;             // 等待队列
    mutex mutex_;                   // 保护共享数据
    condition_variable cv_;         // 用于同步
};
```

**工作流程：**

```
提交任务 ──> 检查当前运行数
                  │
    运行数 < 最大 ─┼──> 立即执行
                  │
    运行数 >= 最大 ─┼──> 加入队列等待
                  │
              任务完成 ──> 检查队列
                  │
    队列不为空 ───> 取出下一个执行
```

### 3.3 线程安全

```cpp
void ToolExecutor::submit(...) {
    lock_guard<mutex> lock(mutex_);  // 加锁保护
    
    if (running_ >= max_concurrent_) {
        queue_.push({tool, args, callback});  // 安全访问队列
    } else {
        running_++;  // 安全修改计数
        execute_internal(tool, args, callback);
    }
}  // 自动解锁
```

**为什么需要 mutex？**

```
场景：两个线程同时 submit

线程 A: running_ = 3
线程 B: running_ = 3（同时读取）

结果：
线程 A: running_++ ──> 4
线程 B: running_++ ──> 4（预期应该是 5）

数据竞争导致错误！
```

**解决方案：**
- `std::mutex`：互斥锁，同一时间只有一个线程可以访问
- `std::lock_guard`：RAII 封装，自动加锁/解锁
- `std::condition_variable`：条件变量，用于线程间通知

---

## 第四部分：异步工具实现

### 4.1 HttpTool - 模拟 HTTP 请求

```cpp
class HttpTool : public Tool {
public:
    void execute_async(const json::object& args,
                       function<void(json::value)> callback) override {
        string url = args.at("url").as_string();
        
        // 在新线程中执行（模拟异步 HTTP）
        thread([url, callback]() {
            this_thread::sleep_for(100ms);  // 模拟网络延迟
            
            json::object result;
            result["success"] = true;
            result["url"] = url;
            result["body"] = "Response from " + url;
            
            callback(result);  // 回调返回结果
        }).detach();
    }
};
```

### 4.2 带超时的实现

```cpp
class DatabaseTool : public Tool {
public:
    void execute_async(const json::object& args,
                       function<void(json::value)> callback) override {
        string query = args.at("sql").as_string();
        int timeout_ms = args.contains("timeout") 
            ? args.at("timeout").as_int64() 
            : 5000;
        
        thread([query, timeout_ms, callback]() {
            auto start = steady_clock::now();
            
            // 模拟查询（50ms）
            this_thread::sleep_for(50ms);
            
            auto elapsed = duration_cast<milliseconds>(
                steady_clock::now() - start).count();
            
            if (elapsed > timeout_ms) {
                callback(error("Query timeout"));
            } else {
                callback(success(query, elapsed));
            }
        }).detach();
    }
};
```

---

## 第五部分：完整运行测试

### 5.1 编译运行

```bash
cd src/step07
mkdir build && cd build
cmake .. && make
./nuclaw_step07
```

### 5.2 测试异步执行

```bash
wscat -c ws://localhost:8081

# 发送多个请求（不会阻塞）
> {"tool": "http_request", "args": {"url": "http://example.com"}}
> {"tool": "http_request", "args": {"url": "http://api.test.com"}}
> {"tool": "http_request", "args": {"url": "http://service.io"}}

# 观察输出：请求是并发处理的
```

### 5.3 测试并发限制

```bash
# 快速发送 10 个请求
for i in {1..10}; do 
    echo "{\"tool\": \"http_request\", \"args\": {\"url\": \"http://test$i.com\"}}"
done | wscat -c ws://localhost:8081

# 观察输出：
# [▶️ Executing] 4 个同时运行
# [⏳ Queued] 6 个在队列等待
# 当一个完成，队列中的下一个开始执行
```

---

## 第六部分：与 Step 6 的对比

| 特性 | Step 6 (同步) | Step 7 (异步) |
|:---|:---|:---|
| 执行方式 | 阻塞调用 | 非阻塞 + 回调 |
| 并发能力 | 顺序执行 | 并行执行 |
| 资源占用 | 高（线程阻塞） | 低（线程复用） |
| 实现复杂度 | 简单 | 中等 |
| 适用场景 | 本地计算 | I/O 操作 |
| 超时控制 | 难 | 容易 |

---

## 第七部分：常见问题

### Q: 为什么要用 detach() 而不是 join()？

A:
- `join()`：阻塞等待线程结束
- `detach()`：线程独立运行，不阻塞调用者

异步工具需要 detach，否则会失去异步的意义。

### Q: 回调函数什么时候执行？

A:
回调在工具执行完成后，由工具创建的线程调用。

```cpp
thread([callback]() {
    // 执行工具逻辑...
    callback(result);  // ← 在这里调用
}).detach();
```

### Q: 如何确保线程安全？

A:
1. **共享数据加锁**：使用 `mutex` 保护
2. **避免数据竞争**：多个线程不修改同一数据
3. **使用原子操作**：`atomic<int>` 代替普通 int

### Q: future 和 promise 是什么？

A:
它们是异步编程的同步原语：

```cpp
promise<int> p;      // 设置值
future<int> f = p.get_future();  // 获取值

// 线程 A
p.set_value(42);  // 设置值

// 线程 B
int x = f.get();  // 获取值（如果还没设置，阻塞等待）
```

用于异步转同步的场景。

---

## 下一步

→ **Step 8: Tools 系统（下）- 工具生态**

实现真正的工具：
- HTTP 客户端（使用 Boost.Beast）
- 文件操作（带沙箱安全）
- 代码执行（安全检查）

学习如何构建完整的工具生态系统。
