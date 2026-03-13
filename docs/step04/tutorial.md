# Step 4: Agent Loop - 执行引擎

> 目标：实现工具调用的循环检测和并行执行
> 
> 难度：⭐⭐⭐ (中等)
> 
> 代码量：约 320 行

## 本节收获

- 理解 Agent 的循环调用问题
- 掌握循环检测算法
- 学会使用 `std::async` 并行执行任务
- 实现工具调用框架

---

## 为什么需要执行引擎？

### Agent 的困境

想象一个 Agent 接到任务：

```
用户：查一下今天的天气，然后发邮件给我

Agent 的思考：
1. 需要调用 weather.get() 获取天气
2. 需要调用 email.send() 发送邮件
3. 这两个调用可以并行执行！
```

### 执行引擎的职责

```
┌─────────────────────────────────────────┐
│              执行引擎                    │
├─────────────────────────────────────────┤
│                                         │
│  ① 解析 LLM 输出，提取工具调用            │
│              │                          │
│              ▼                          │
│  ② 检测循环调用（安全检查）               │
│              │                          │
│              ▼                          │
│  ③ 并行执行独立工具                       │
│              │                          │
│              ▼                          │
│  ④ 收集结果，返回给 LLM                  │
│                                         │
└─────────────────────────────────────────┘
```

---

## 循环检测

### 什么是循环调用？

Agent 可能陷入死循环：

```
用户：搜索 "Python"

Agent: search("Python") 
       ↓
       发现结果不够详细
       ↓
       search("Python tutorial")
       ↓
       还是不够详细
       ↓
       search("Python tutorial beginner")
       ↓
       ...无限循环
```

### 循环检测算法

**简单策略：检测连续相同的调用**

```cpp
bool detect_loop(Session& session) {
    // 至少要有 3 次调用才能检测
    if (session.recent_tool_calls.size() < 3) return false;
    
    auto& calls = session.recent_tool_calls;
    size_t n = calls.size();
    
    // 检查最后 3 次是否完全相同
    if (calls[n-1] == calls[n-2] && calls[n-2] == calls[n-3]) {
        session.loop_warning_count++;
        return true;  // 检测到循环
    }
    return false;
}
```

**更复杂的策略：**
- 相似度检测（编辑距离）
- 时间窗口内的频率限制
- 语义重复检测

### 实现细节

```cpp
struct Session {
    std::vector<std::string> recent_tool_calls;  // 最近调用记录
    int loop_warning_count = 0;                   // 警告次数
    
    void record_call(const std::string& signature) {
        recent_tool_calls.push_back(signature);
        // 只保留最近 10 次
        if (recent_tool_calls.size() > 10) {
            recent_tool_calls.erase(recent_tool_calls.begin());
        }
    }
};
```

---

## 并行工具执行

### 串行 vs 并行

**串行执行：**
```
t0: 调用工具A ────────────▶ (100ms) ────────────▶
t1: 等待...                                      │
t2:                调用工具B ───────────▶ (80ms) ▶
总时间: 180ms
```

**并行执行：**
```
t0: 启动工具A ────────────▶ (100ms) ────────────▶
t1: 启动工具B ───────────▶ (80ms)  ────────────▶
总时间: 100ms (取最大值)
```

### std::async 并行执行

```cpp
#include <future>

std::vector<ToolResult> execute_tools_parallel(
    const std::vector<ToolCall>& calls) {
    
    std::vector<std::future<ToolResult>> futures;
    
    // 启动所有工具调用
    for (const auto& call : calls) {
        futures.push_back(
            std::async(std::launch::async, [&call]() {
                // 在新线程中执行工具
                return execute_single_tool(call);
            })
        );
    }
    
    // 收集所有结果
    std::vector<ToolResult> results;
    for (auto& f : futures) {
        results.push_back(f.get());  // 阻塞等待结果
    }
    
    return results;
}
```

**关键点：**
- `std::launch::async`：强制在新线程执行
- `f.get()`：获取结果，如果还没完成会阻塞等待
- 异常传播：工具抛出的异常会通过 `f.get()` 抛出

### async vs thread

| 特性 | std::thread | std::async |
|:---|:---|:---|
| 返回值 | 无 | 通过 future 获取 |
| 异常处理 | 需要自己传递 | 自动传播 |
| 线程池 | 每次都创建 | 可能复用线程 |
| 使用难度 | 较复杂 | 简单 |

---

## 工具调用框架

### 数据结构

```cpp
// 工具调用请求
struct ToolCall {
    std::string id;           // 唯一标识
    std::string name;         // 工具名
    json::value arguments;    // 参数
};

// 工具执行结果
struct ToolResult {
    std::string id;           // 对应请求的 id
    std::string output;       // 输出
    bool success;             // 是否成功
};
```

### 解析工具调用

在实际项目中，LLM 会输出类似：

```json
{
    "tool_calls": [
        {"name": "weather.get", "arguments": {"city": "Beijing"}},
        {"name": "email.send", "arguments": {"to": "user@example.com"}}
    ]
}
```

解析代码：

```cpp
std::vector<ToolCall> parse_tool_calls(const std::string& llm_output) {
    std::vector<ToolCall> calls;
    auto json = json::parse(llm_output);
    
    for (const auto& call : json["tool_calls"].as_array()) {
        calls.push_back({
            generate_id(),
            call["name"].as_string(),
            call["arguments"]
        });
    }
    return calls;
}
```

---

## 完整运行测试

### 1. 编译运行

```bash
cd src/step04
mkdir build && cd build
cmake .. && make
./nuclaw_step04
```

### 2. 测试工具调用

```bash
wscat -c ws://localhost:8081

# 测试计算器工具
> /calc 1+1
< Processed: /calc 1+1 [tools: 1, loop_checks: 0]

# 测试时间工具
> /time
< Processed: /time [tools: 1, loop_checks: 0]

# 测试多个工具并行
> /calc and /time
< Processed: /calc and /time [tools: 2, loop_checks: 0]
```

### 3. 测试循环检测

```bash
# 发送相同的请求 3 次
> /calc 1+1
> /calc 1+1
> /calc 1+1
< Warning: Detected potential loop. Please try a different approach.
```

---

## 代码亮点

### 1. Lambda 捕获技巧

```cpp
ws_.async_accept(
    [self = shared_from_this()](beast::error_code ec) {
        if (!ec) self->do_read();
    }
);
```

C++14 的广义 lambda 捕获，直接初始化捕获变量。

### 2. 链式异步调用

```cpp
ws_.async_write(asio::buffer(response),
    [self](beast::error_code, std::size_t) { 
        self->do_read();  // 写完后继续读
    }
);
```

通过回调实现"读 -> 处理 -> 写 -> 读"的循环。

---

## 下一步

→ **Step 5: Agent Loop - 高级特性**

我们将实现：
- 长期记忆系统
- 上下文压缩
- 响应质量评估
