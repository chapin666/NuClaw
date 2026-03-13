# Step 5: Agent Loop - 高级特性

> 目标：实现记忆系统、上下文压缩、响应质量门控
> 
> 难度：⭐⭐⭐⭐ (较难)
> 
> 代码量：约 400 行

## 本节收获

- 理解 LLM 上下文窗口限制
- 实现长期记忆系统
- 掌握上下文压缩策略
- 实现质量评估与重试机制

---

## 上下文窗口问题

### LLM 的"失忆症"

LLM 有**固定的上下文窗口**（如 GPT-4 是 8K/32K tokens）：

```
┌────────────────────────────────────────────────────┐
│                 LLM 上下文窗口                      │
├────────────────────────────────────────────────────┤
│ 系统提示 │ 历史消息 │ 当前输入 │ ← 新内容被截断     │
│  (500)  │  (7000) │  (500)  │                    │
│         │   ↑     │         │                    │
│         │ 超出限制 │         │                    │
└────────────────────────────────────────────────────┘
```

当对话变长时，旧消息会被**截断**，Agent "忘记" 之前的对话。

### 解决方案对比

| 方案 | 优点 | 缺点 |
|:---|:---|:---|
| 滑动窗口 | 简单 | 丢失重要信息 |
| 摘要压缩 | 保留概要 | 丢失细节 |
| 长期记忆 | 永久存储 | 检索复杂 |
| 混合策略 | 综合优势 | 实现复杂 |

**本节实现：混合策略**

---

## 长期记忆系统

### 记忆的数据结构

```cpp
struct MemoryEntry {
    std::string key;         // 检索键
    std::string value;       // 记忆内容
    float importance = 1.0;  // 重要性分数 (0-1)
};

struct Session {
    std::vector<MemoryEntry> long_term_memory;  // 长期记忆
    // ...
};
```

### 记忆存储

```cpp
void store_memory(Session& session, 
                  const std::string& key, 
                  const std::string& value) {
    // 内存限制：最多保留 50 条
    if (session.long_term_memory.size() > 50) {
        // 移除最不重要的一条
        auto min_it = std::min_element(
            session.long_term_memory.begin(),
            session.long_term_memory.end(),
            [](const MemoryEntry& a, const MemoryEntry& b) {
                return a.importance < b.importance;
            }
        );
        session.long_term_memory.erase(min_it);
    }
    
    session.long_term_memory.push_back({key, value, 1.0f});
}
```

**策略：**
- 基于重要性的 LRU（最近最少使用）
- 可以动态调整重要性分数

### 记忆检索

```cpp
std::string retrieve_memory(Session& session, 
                            const std::string& query) {
    std::string result;
    for (const auto& mem : session.long_term_memory) {
        if (mem.importance > 0.5) {  // 只返回重要记忆
            result += mem.key + ": " + mem.value + "; ";
        }
    }
    return result.empty() ? "no_memory" : result;
}
```

**高级检索策略：**
- 向量相似度搜索（OpenAI embeddings）
- 关键词匹配
- 时间衰减（越近的记忆越重要）

---

## 上下文压缩

### 压缩策略

当 token 数超过阈值（80%），触发压缩：

```cpp
void compress_context(Session& session) {
    if (session.history.size() < 10) return;  // 消息太少不压缩
    
    // 1. 生成摘要（简化版）
    std::string summary = "Summary of " + 
        std::to_string(session.history.size() - 5) + " messages";
    
    // 2. 保留最近 5 条详细消息
    // 3. 前面的消息替换为摘要
    session.history = {
        {"system", summary, estimate_tokens(summary)},
        // ... 保留最近的消息
    };
    
    // 4. 重新计算 token 数
    recalculate_tokens(session);
}
```

### 摘要生成算法

**简化版（本节）：** 直接标注消息数量

**生产级方案：**
- 使用 LLM 生成摘要（"总结上述对话"）
- 提取关键信息（实体、意图）
- 分层摘要（多级压缩）

### Token 估算

```cpp
size_t estimate_tokens(const std::string& text) {
    // 经验公式：英文约 4 字符/token，中文约 1.5 字符/token
    return text.length() / 4;
}
```

**注意：** 这是粗略估算，精确值需要用 tokenizer（如 tiktoken）。

---

## 质量门控

### 为什么需要？

LLM 可能生成：
- 无意义的回复
- 格式错误的输出
- 与上下文不相关的内容

质量门控自动检测并重试。

### 质量评估函数

```cpp
float evaluate_quality(const std::string& response) {
    // 简单规则
    if (response.length() < 10) return 0.3f;  // 太短
    if (response.find("error") != std::string::npos) return 0.5f;  // 含错误
    
    return 0.9f;  // 合格
}
```

**生产级方案：**
- 使用另一个 LLM 评估（如 GPT-4 评估 GPT-3.5 的输出）
- 规则引擎 + 机器学习
- 用户反馈学习

### 重试机制

```cpp
std::string generate_with_quality_gate(...) {
    std::string response = generate_response(input, context);
    
    float quality = evaluate_quality(response);
    
    // 质量不合格且重试次数未超限
    if (quality < 0.7 && session.regenerate_count < 3) {
        session.regenerate_count++;
        return generate_with_quality_gate(input, context, session);
    }
    
    session.regenerate_count = 0;
    return response;
}
```

**策略：**
- 最多重试 3 次
- 每次重试可以调整参数（温度、提示词）

---

## 完整运行测试

### 1. 编译运行

```bash
cd src/step05
mkdir build && cd build
cmake .. && make
./nuclaw_step05
```

### 2. 测试记忆系统

```bash
wscat -c ws://localhost:8081

# 存储一条长消息（会存入长期记忆）
> I prefer Python over JavaScript for backend development

# 继续对话...
> What language should I use?

# Agent 应该能从记忆中知道用户喜欢 Python
```

### 3. 测试上下文压缩

发送大量消息，观察 token 数变化：

```bash
# 连续发送多条消息
for i in {1..20}; do echo "Message $i"; done | wscat -c ws://localhost:8081

# 查看会话信息，current_tokens 应该保持在限制内
```

### 4. 测试质量门控

```bash
# 发送可能触发低质量响应的输入
> ?

# 观察是否触发重试
```

---

## 架构演进总结

```
Step 3: 基础 Agent Loop
   │
   ▼
Step 4: + 循环检测
   │    + 并行执行
   ▼
Step 5: + 长期记忆
        + 上下文压缩
        + 质量门控
```

---

## 下一步

Step 5 完成了 Agent Loop 的高级特性。接下来：

→ **Step 6: Tools 系统** - 实现真正的工具调用
→ **Step 7: 异步工具执行** - 集成 Asio
→ **Step 8: 工具生态** - HTTP 工具、代码执行等

Agent Core 部分已完成！
