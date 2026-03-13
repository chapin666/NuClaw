# Step 4: Agent Loop - 执行引擎

> 目标：实现工具调用的循环检测和并行执行
> 
> 难度：⭐⭐⭐ (中等)
> 
003e 代码量：约 320 行

## 本节收获

- 理解 Agent 执行策略（串行 vs 并行 vs 图执行）
- 掌握循环检测算法和防护策略
- 学会使用 `std::async` 并行执行任务
- 理解工具调用的安全考虑
- 实现完整的工具调用框架

---

## 第一部分：Agent 执行策略

### 1.1 工具调用的复杂度

想象 Agent 接到一个复杂任务：

```
用户："帮我规划一次北京到上海的旅行，
      查看天气预报，比较高铁和飞机价格，
      然后预订最便宜的选项"

需要调用的工具：
1. weather.get("北京") - 北京天气
2. weather.get("上海") - 上海天气  
3. train.search("北京", "上海") - 查高铁
4. flight.search("北京", "上海") - 查机票
5. booking.reserve(...) - 预订

依赖关系：
• 工具 1 和 2 互相独立（可并行）
• 工具 3 和 4 互相独立（可并行）
• 工具 5 依赖 1-4 的结果（必须最后执行）
```

### 1.2 三种执行策略

**策略一：串行执行**

```
时间 ─────────────────────────────────────────▶

工具1 ─────────────────────────▶
       工具2 ──────────────────▶
              工具3 ───────────▶
                     工具4 ─────▶
                            工具5 ▶

总时间 = 所有工具执行时间之和
```

优点：
- 实现简单
- 无竞态条件
- 资源占用低

缺点：
- 速度慢
- 无法利用并行性

**策略二：全并行**

```
时间 ─────────────────────────▶

工具1 ─────────────────────────▶
工具2 ─────────────────▶
工具3 ───────────▶
工具4 ──────────────────────▶
工具5 ────▶

总时间 = 最慢的工具执行时间
```

优点：
- 速度最快
- 充分利用资源

缺点：
- 需要处理依赖关系
- 可能违反调用顺序

**策略三：依赖图执行（推荐）**

```
          ┌────────┐     ┌────────┐
          │ 工具1  │     │ 工具2  │
          └───┬────┘     └───┬────┘
              │              │
              └──────┬───────┘
                     │
              ┌──────┴───────┐
              │ 工具3        │ 工具4 │
              └───┬────┘     └───┬────┘
                  │              │
                  └──────┬───────┘
                         │
                    ┌────┴────┐
                    │ 工具5   │
                    └─────────┘

Phase 1: 并行执行工具1、工具2
Phase 2: 并行执行工具3、工具4
Phase 3: 执行工具5
```

优点：
- 既快速又正确
- 符合业务逻辑

缺点：
- 需要构建依赖图
- 实现复杂

**本节实现策略二（全并行）**，因为简单且适用于大多数场景。

### 1.3 执行引擎架构

```
┌─────────────────────────────────────────────────────────┐
│                     执行引擎                             │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  输入：LLM 输出的工具调用列表                             │
│    [{"name": "weather.get", "args": {...}}, ...]        │
│              │                                          │
│              ▼                                          │
│  ┌─────────────────────┐                               │
│  │     安全检查层       │                               │
│  │  • 循环检测          │                               │
│  │  • 权限校验          │                               │
│  │  • 参数验证          │                               │
│  └──────────┬──────────┘                               │
│             │                                           │
│             ▼                                           │
│  ┌─────────────────────┐                               │
│  │     调度层           │                               │
│  │  • 依赖分析          │                               │
│  │  • 任务调度          │                               │
│  │  • 超时控制          │                               │
│  └──────────┬──────────┘                               │
│             │                                           │
│             ▼                                           │
│  ┌─────────────────────┐                               │
│  │     执行层           │                               │
│  │  • 线程池            │                               │
│  │  • 工具调用          │                               │
│  │  • 结果收集          │                               │
│  └──────────┬──────────┘                               │
│             │                                           │
│             ▼                                           │
│  输出：工具执行结果列表                                   │
│    [{"id": "1", "output": "...", "success": true}, ...] │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

---

## 第二部分：循环检测详解

### 2.1 为什么 Agent 会陷入循环？

**场景一：过度优化**
```
用户："优化这段代码"

Agent: 调用 linter 检查 → 发现可以优化的地方
       ↓
       修改代码
       ↓
       再次调用 linter → 又发现可以优化的地方
       ↓
       无限循环...
```

**场景二：信息不足**
```
用户："查找最新的 Go 版本"

Agent: search("Go latest version")
       ↓
       结果不够详细
       ↓
       search("Go latest version official")
       ↓
       结果还是不够详细
       ↓
       search("Go latest version official website")
       ↓
       无限细化...
```

**场景三：工具失败重试**
```
Agent: 调用 API → 超时
       ↓
       重试 → 还是超时
       ↓
       重试 → 还是超时
       ↓
       无限重试...
```

### 2.2 循环检测策略

**策略一：完全重复检测（本节实现）**

```cpp
bool detect_loop(const std::vector<std::string>& history) {
    if (history.size() < 3) return false;
    
    size_t n = history.size();
    // 检查最后 3 次是否完全相同
    return history[n-1] == history[n-2] && 
           history[n-2] == history[n-3];
}
```

适用：完全相同的工具调用

**策略二：相似度检测**

```cpp
bool detect_similarity(const std::string& a, const std::string& b) {
    // 计算编辑距离
    int distance = levenshtein_distance(a, b);
    // 相似度 90% 以上认为是重复
    return distance < a.length() * 0.1;
}
```

适用：参数略有不同，但本质相同的调用

**策略三：频率限制**

```cpp
bool detect_frequency(const std::vector<Timestamp>& timestamps) {
    // 1 分钟内调用超过 10 次
    auto recent = count_recent(timestamps, 60);
    return recent > 10;
}
```

适用：高频调用同一工具

**策略四：语义检测（高级）**

```cpp
bool detect_semantic_loop(const std::vector<Call>& calls) {
    // 使用 LLM 判断是否陷入循环
    std::string prompt = "判断以下工具调用是否在无限循环: " + 
                         serialize(calls);
    std::string result = llm_judge(prompt);
    return result == "LOOP";
}
```

适用：复杂场景，但成本较高

### 2.3 循环处理策略

检测到循环后，Agent 应该：

```cpp
enum class LoopAction {
    ABORT,        // 终止执行，返回错误
    WARN,         // 警告用户，继续执行
    SKIP,         // 跳过本次工具调用
    ESCALATE,     // 升级给人类处理
    ADAPT         // 调整策略（如更换工具）
};

LoopAction handle_loop(Session& session) {
    if (session.loop_count < 2) {
        return LoopAction::WARN;  // 前两次警告
    } else if (session.loop_count < 5) {
        return LoopAction::ADAPT; // 尝试调整
    } else {
        return LoopAction::ABORT; // 强制终止
    }
}
```

---

## 第三部分：并行执行详解

### 3.1 C++ 并发基础

**线程 (std::thread)**：
```cpp
#include <thread>

void task() {
    std::cout << "Running in thread\n";
}

std::thread t(task);  // 创建线程
// ...
t.join();  // 等待线程结束
```

**缺点：**
- 无法直接获取返回值
- 异常不会传播到主线程
- 需要手动管理生命周期

**异步任务 (std::async)**：
```cpp
#include <future>

int task() {
    return 42;
}

std::future<int> result = std::async(std::launch::async, task);
int value = result.get();  // 获取返回值
```

**优点：**
- 通过 future 获取返回值
- 异常自动传播
- 可能使用线程池

### 3.2 并行执行工具

```cpp
std::vector<ToolResult> execute_parallel(
    const std::vector<ToolCall>& calls) {
    
    std::vector<std::future<ToolResult>> futures;
    
    // 启动所有任务
    for (const auto& call : calls) {
        futures.push_back(
            std::async(std::launch::async, [&call]() -> ToolResult {
                try {
                    // 执行工具
                    auto output = execute_tool(call);
                    return {call.id, output, true};
                } catch (const std::exception& e) {
                    return {call.id, e.what(), false};
                }
            })
        );
    }
    
    // 收集结果
    std::vector<ToolResult> results;
    for (auto& future : futures) {
        results.push_back(future.get());
    }
    
    return results;
}
```

**关键细节：**
- `std::launch::async` 强制创建新线程
- `future.get()` 阻塞直到结果可用
- 捕获异常并封装到结果中

### 3.3 超时控制

工具调用可能无限阻塞：

```cpp
ToolResult execute_with_timeout(const ToolCall& call, 
                                std::chrono::seconds timeout) {
    auto future = std::async(std::launch::async, [&call]() {
        return execute_tool(call);
    });
    
    // 等待指定时间
    if (future.wait_for(timeout) == std::future_status::timeout) {
        return {call.id, "Timeout", false};
    }
    
    return {call.id, future.get(), true};
}
```

---

## 第四部分：工具安全

### 4.1 工具调用的风险

Agent 调用工具 = 执行代码，存在安全风险：

| 风险 | 示例 | 防护 |
|:---|:---|:---|
| 数据泄露 | 工具把数据发到外部 | 白名单机制 |
| 资源耗尽 | 无限循环占用 CPU | 超时+限额 |
| 权限提升 | 删除系统文件 | 沙箱执行 |
| 副作用 | 重复扣款 | 幂等设计 |

### 4.2 权限控制

```cpp
class ToolRegistry {
public:
    void register_tool(const std::string& name, 
                       ToolFunc func,
                       PermissionLevel level);
    
    bool can_execute(const std::string& user_role,
                     const std::string& tool_name) {
        auto tool_level = get_tool_level(tool_name);
        auto user_level = get_user_level(user_role);
        return user_level >= tool_level;
    }
};

// 权限等级
enum class PermissionLevel {
    READ_ONLY,    // 只读工具（查询）
    STANDARD,     // 标准工具（发送邮件）
    SENSITIVE,    // 敏感工具（支付、删除）
    ADMIN         // 管理工具（配置系统）
};
```

### 4.3 沙箱执行

对于危险工具，在隔离环境执行：

```cpp
// 使用容器隔离
docker run --rm --network none --read-only \
    sandbox-image \
    execute-tool "${args}"

// 或使用 seccomp 限制系统调用
sandbox::Sandbox sb;
sb.add_policy(sandbox::Policy::NoNetwork);
sb.add_policy(sandbox::Policy::ReadOnlyFS);
sb.execute(tool);
```

---

## 第五部分：代码实现

### 5.1 完整执行流程

```cpp
std::string AgentLoop::process(const std::string& input,
                               const std::string& session_id) {
    auto& session = sessions_[session_id];
    session.state = State::THINKING;
    
    // 1. 解析工具调用
    auto calls = parse_tool_calls(input);
    
    // 2. 安全检查
    if (detect_loop(session)) {
        return handle_loop_action(session);
    }
    
    // 3. 并行执行
    if (!calls.empty()) {
        session.state = State::TOOL_CALLING;
        auto results = execute_tools_parallel(calls);
        
        // 4. 记录调用历史
        for (const auto& call : calls) {
            session.recent_tool_calls.push_back(
                call.name + "_" + json::serialize(call.arguments)
            );
        }
    }
    
    // 5. 生成响应
    session.state = State::IDLE;
    return generate_response(input, calls);
}
```

---

## 第六部分：运行测试

### 6.1 编译运行

```bash
cd src/step04
mkdir build && cd build
cmake .. && make
./nuclaw_step04
```

### 6.2 测试循环检测

```bash
wscat -c ws://localhost:8081

# 发送相同请求 3 次触发循环检测
> /calc 1+1
> /calc 1+1
> /calc 1+1
< Warning: Detected potential loop...
```

---

## 下一步

→ **Step 5: Agent Loop - 高级特性**

学习：
- LLM 上下文窗口管理
- 长期记忆系统
- 响应质量门控
