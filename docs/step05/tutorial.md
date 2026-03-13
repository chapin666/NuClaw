# Step 5: Agent Loop - 高级特性

> 目标：实现记忆系统、上下文压缩、响应质量门控
> 
> 难度：⭐⭐⭐⭐ (较难)
003e 
003e 代码量：约 400 行

## 本节收获

- 理解 LLM 架构基础（Transformer、Tokenization）
- 掌握上下文窗口限制及解决方案
- 实现长期记忆系统（短期/长期/向量记忆）
- 理解上下文压缩策略（滑动窗口/摘要/RAG）
- 实现质量评估与重试机制

---

## 第一部分：LLM 架构基础

### 1.1 什么是 Token？

LLM 处理的不是"字符"或"单词"，而是 **Token**（词元）：

```
文本："Hello, world!"

Tokenization（分词）：
["Hello", ",", " world", "!"]
  ↓
转换为 ID：
[15496, 11, 995, 0]
  ↓
输入给模型
```

**不同语言的 token 数量：**
| 语言 | 示例 | 大约 Token 数 |
|:---|:---|:---|
| 英文 | "Hello world" | 2-3 |
| 中文 | "你好世界" | 4-8 |
| 代码 | `int main()` | 3-4 |

**重要：1 个汉字 ≠ 1 个 token！**
- GPT-4：约 1.5-2 个字符/token
- 中文通常比英文消耗更多 token

### 1.2 上下文窗口 (Context Window)

LLM 是"近视眼"，只能看到有限长度的输入：

```
┌────────────────────────────────────────────────────────────┐
│                    LLM 输入限制                             │
├────────────────────────────────────────────────────────────┤
│                                                            │
│  System Prompt │  History  │  Current Input                │
│  (系统提示)    │ (历史对话)│  (当前输入)                   │
│                │           │                               │
│  "你是一个     │  User: hi │  User: 今天天气如何           │
│   有帮助的     │  AI: 你好 │                               │
│   助手"        │  User:... │                               │
│                │           │                               │
│  ├─────────────┼───────────┼──────────────────────────────┤ │
│  │  500 tokens │ 3000 tok  │      500 tokens              │ │
│  └─────────────┴───────────┴──────────────────────────────┘ │
│                                                            │
│  总计：4000 tokens（以 GPT-3.5 4K 为例）                     │
│                                                            │
│  ⚠️ 超出部分会被截断，模型"看不到"！                        │
│                                                            │
└────────────────────────────────────────────────────────────┘
```

**常见模型的上下文窗口：**
| 模型 | 上下文窗口 | 大致字数 |
|:---|:---|:---|
| GPT-3.5 | 4K / 16K | 3000 / 12000 |
| GPT-4 | 8K / 32K | 6000 / 24000 |
| Claude 3 | 200K | 150000 |
| Llama 2 | 4K | 3000 |

### 1.3 为什么有上下文限制？

**技术原因：Transformer 的注意力机制**

```
注意力计算复杂度 = O(n²)

n = token 数量

n = 1000  →  100万次计算
n = 4000  →  1600万次计算  
n = 32000 →  10亿次计算！

上下文窗口翻倍 → 计算量变成 4 倍
```

**解决方案方向：**
1. **模型层面**：改进架构（如稀疏注意力）
2. **工程层面**：RAG、记忆系统、上下文压缩

---

## 第二部分：记忆系统架构

### 2.1 记忆的层次结构

人类记忆 vs Agent 记忆：

```
人类记忆：                              Agent 实现：
┌──────────────┐                      ┌──────────────┐
│ 工作记忆     │ ← 当前思考            │ 上下文窗口   │ 内存
│ (短时)       │   容量 4±1 项         │              │
└──────┬───────┘                      └──────────────┘
       │                                      │
       ▼                                      ▼
┌──────────────┐                      ┌──────────────┐
│ 长期记忆     │ ← 知识储备            │ 向量数据库   │ 持久化
│              │   容量几乎无限        │ / 数据库     │ 存储
└──────────────┘                      └──────────────┘
```

### 2.2 Agent 记忆的三种类型

**类型一：短期记忆（对话历史）**

```cpp
struct ShortTermMemory {
    std::vector<Message> history;
    size_t max_tokens = 4000;
    
    void add(const Message& msg) {
        history.push_back(msg);
        if (total_tokens() > max_tokens) {
            // 移除旧消息
            history.erase(history.begin());
        }
    }
};
```

特点：
- 存于内存，速度快
- 随会话结束消失
- 容量有限

**类型二：长期记忆（关键信息存储）**

```cpp
struct LongTermMemory {
    // 显式存储的关键事实
    std::map<std::string, std::string> facts;
    
    void remember(const std::string& key, 
                  const std::string& value) {
        facts[key] = value;
    }
    
    std::string recall(const std::string& key) {
        return facts.count(key) ? facts[key] : "";
    }
};

// 使用示例
remember("user_name", "张三");
remember("user_preference_language", "Python");
```

特点：
- 结构化存储
- 精确检索
- 需要显式写入

**类型三：向量记忆（语义检索）**

```cpp
struct VectorMemory {
    // 文本 → 向量（通过 Embedding API）
    std::vector<Embedding> embeddings;
    
    void store(const std::string& text) {
        auto vec = embedding_model.encode(text);
        embeddings.push_back({text, vec});
    }
    
    // 语义检索
    std::vector<std::string> query(const std::string& question,
                                      size_t top_k = 3) {
        auto query_vec = embedding_model.encode(question);
        // 计算余弦相似度，返回最相似的 top_k 条
        return find_similar(query_vec, embeddings, top_k);
    }
};
```

特点：
- 语义理解
- 模糊匹配
- 需要向量数据库

### 2.3 记忆检索策略

**策略一：精确匹配**

```cpp
// 直接查找
auto name = long_term_memory["user_name"];
```

适用：用户配置、固定事实

**策略二：关键词匹配**

```cpp
std::vector<std::string> keyword_search(
    const std::vector<Memory>& memories,
    const std::string& query) {
    
    std::vector<std::string> results;
    for (const auto& mem : memories) {
        if (mem.content.find(query) != std::string::npos) {
            results.push_back(mem.content);
        }
    }
    return results;
}
```

适用：简单场景，速度快

**策略三：向量相似度（RAG）**

```cpp
// 余弦相似度
double cosine_similarity(const Vector& a, const Vector& b) {
    return dot_product(a, b) / (magnitude(a) * magnitude(b));
}

// RAG：检索增强生成
std::string rag_query(const std::string& question) {
    // 1. 检索相关记忆
    auto relevant = vector_memory.query(question, 3);
    
    // 2. 构建增强提示
    std::string prompt = "基于以下信息回答问题:\n";
    for (const auto& mem : relevant) {
        prompt += "- " + mem + "\n";
    }
    prompt += "\n问题: " + question;
    
    // 3. 调用 LLM
    return llm.generate(prompt);
}
```

适用：复杂问答、知识库场景

---

## 第三部分：上下文压缩策略

### 3.1 滑动窗口（最简单）

```cpp
void sliding_window(std::vector<Message>& history,
                    size_t max_messages = 20) {
    // 只保留最近 N 条
    if (history.size() > max_messages) {
        history.erase(history.begin(), 
                      history.begin() + (history.size() - max_messages));
    }
}
```

**优点：** 简单、确定性强
**缺点：** 丢失早期重要信息

### 3.2 摘要压缩（本节实现）

```cpp
void compress_with_summary(std::vector<Message>& history) {
    if (history.size() < 10) return;
    
    // 保留最近 5 条详细记录
    std::vector<Message> recent(
        history.end() - 5, history.end()
    );
    
    // 前面的生成摘要
    std::string summary = generate_summary(
        std::vector<Message>(history.begin(), history.end() - 5)
    );
    
    // 替换历史
    history = {
        {"system", summary, estimate_tokens(summary)},
        ...recent
    };
}

std::string generate_summary(const std::vector<Message>& msgs) {
    // 简化版：统计信息
    return "[摘要] 之前讨论了 " + 
           std::to_string(msgs.size()) + " 条消息，" +
           "包括: " + extract_topics(msgs);
    
    // 生产版：调用 LLM 生成摘要
    // return llm.generate("总结以下对话: " + serialize(msgs));
}
```

**优点：** 保留概要信息
**缺点：** 丢失细节、摘要质量依赖 LLM

### 3.3 分层记忆（最复杂）

```
对话历史：
[
  "用户喜欢Python",
  "讨论了Web框架", 
  "比较了Django和Flask",
  "用户选择Flask",
  ...
]
         ↓
    分层摘要
         ↓
Level 0 (详细): 原始对话（最近 5 条）
Level 1 (摘要): "比较Web框架，选择Flask"（中间层摘要）
Level 2 (概要): "用户偏好Python生态"（最高层概要）
```

```cpp
struct HierarchicalMemory {
    std::vector<Message> level0;  // 原始记录（最近）
    std::vector<std::string> level1;  // 中层摘要
    std::vector<std::string> level2;  // 高层概要
    
    std::string retrieve(const std::string& query, int level) {
        switch(level) {
            case 0: return format(level0);
            case 1: return join(level1);
            case 2: return join(level2);
        }
    }
};
```

### 3.4 压缩策略对比

| 策略 | 复杂度 | 信息保留 | 适用场景 |
|:---|:---|:---|:---|
| 滑动窗口 | ⭐ | 差 | 简单对话 |
| 摘要压缩 | ⭐⭐ | 中 | 通用场景 |
| 分层记忆 | ⭐⭐⭐ | 好 | 长对话 |
| 向量 RAG | ⭐⭐⭐ | 动态 | 知识密集型 |

---

## 第四部分：质量门控系统

### 4.1 为什么需要质量评估？

LLM 可能产生的低质量输出：

| 问题类型 | 示例 | 影响 |
|:---|:---|:---|
| 幻觉 | "马云是腾讯创始人" | 错误信息 |
| 格式错误 | JSON 解析失败 | 系统崩溃 |
| 回答不完整 | 只回答了一半 | 用户体验差 |
| 重复 | 重复同样的句子 | 浪费 token |
| 不相关 | 答非所问 | 无效交互 |

### 4.2 质量评估维度

```cpp
struct QualityScore {
    float completeness;   // 完整性（是否回答所有部分）
    float relevance;      // 相关性（是否切题）
    float coherence;      // 连贯性（逻辑是否通顺）
    float accuracy;       // 准确性（事实是否正确）
    float safety;         // 安全性（有无有害内容）
    
    float overall() const {
        return (completeness + relevance + coherence + 
                accuracy + safety) / 5.0f;
    }
};
```

### 4.3 评估方法

**规则引擎（简单快速）：**

```cpp
QualityScore evaluate_by_rules(const std::string& response) {
    QualityScore score;
    
    // 长度检查
    if (response.length() < 10) {
        score.completeness = 0.3f;
    }
    
    // 关键词检查
    if (response.find("error") != std::string::npos ||
        response.find("对不起") != std::string::npos) {
        score.accuracy = 0.5f;
    }
    
    // 格式检查（JSON）
    if (!is_valid_json(response)) {
        score.coherence = 0.4f;
    }
    
    return score;
}
```

**LLM 评估（更准确）：**

```cpp
QualityScore evaluate_by_llm(const std::string& response,
                             const std::string& question) {
    std::string prompt = R"(
评估以下回答的质量：

问题：{question}
回答：{response}

请从以下维度评分（0-10）：
1. 完整性：
2. 相关性：
3. 准确性：
)";
    
    auto evaluation = llm.generate(format(prompt, question, response));
    return parse_scores(evaluation);
}
```

### 4.4 重试策略

```cpp
std::string generate_with_retry(const std::string& input,
                                Session& session) {
    const int MAX_RETRIES = 3;
    
    for (int i = 0; i < MAX_RETRIES; i++) {
        // 生成响应
        auto response = llm.generate(input);
        
        // 评估质量
        auto quality = evaluate(response);
        
        // 质量合格
        if (quality.overall() >= 0.7f) {
            return response;
        }
        
        // 调整参数重试
        adjust_parameters(i);
    }
    
    // 重试耗尽，返回最佳结果或错误
    return "抱歉，无法生成满意回答";
}

void adjust_parameters(int attempt) {
    // 第一次重试：提高温度，增加多样性
    if (attempt == 0) {
        llm.set_temperature(0.8f);
    }
    // 第二次重试：换用更强的模型
    else if (attempt == 1) {
        llm.switch_model("gpt-4");
    }
    // 第三次重试：添加更详细的提示
    else {
        llm.add_instruction("请详细回答，确保准确");
    }
}
```

---

## 第五部分：生产实践

### 5.1 Token 管理最佳实践

```cpp
class TokenManager {
public:
    // 预留 token 给系统提示
    static constexpr size_t SYSTEM_RESERVE = 500;
    
    // 预留 token 给响应
    static constexpr size_t RESPONSE_RESERVE = 1000;
    
    // 实际可用
    size_t available_tokens() const {
        return max_tokens_ - SYSTEM_RESERVE - RESPONSE_RESERVE - used_tokens_;
    }
    
    // 自动压缩
    void auto_compress_if_needed() {
        if (available_tokens() < 200) {
            compress_context();
        }
    }
};
```

### 5.2 记忆系统的工程权衡

| 方案 | 延迟 | 成本 | 准确度 | 适用场景 |
|:---|:---|:---|:---|:---|
| 纯上下文 | 低 | 低 | 中 | 短对话 |
| + Redis 缓存 | 中 | 中 | 高 | 中等规模 |
| + 向量数据库 | 中 | 高 | 高 | 知识库 |
| + 专用记忆模型 | 高 | 很高 | 很高 | 复杂 Agent |

---

## 第六部分：运行测试

### 6.1 编译运行

```bash
cd src/step05
mkdir build && cd build
cmake .. && make
./nuclaw_step05
```

### 6.2 测试记忆功能

```bash
wscat -c ws://localhost:8081

# 存储偏好
> I prefer Python for backend and React for frontend

# 后续对话中应能回忆
> What tech stack should I use for my new project?
# 期望：推荐 Python + React
```

### 6.3 测试上下文压缩

```bash
# 连续发送消息直到触发压缩
for i in {1..20}; do echo "Message $i content here"; done | wscat -c ws://localhost:8081

# 检查 token 数是否稳定
```

---

## Agent Core 总结

Step 3-5 完成了 Agent 的核心能力：

```
Step 3: 基础 Agent Loop
   ├── 状态机管理
   ├── WebSocket 通信
   └── 对话历史

Step 4: 执行引擎
   ├── 循环检测
   ├── 并行工具执行
   └── 安全控制

Step 5: 高级特性
   ├── 长期记忆
   ├── 上下文压缩
   └── 质量门控
```

---

## 下一步

→ **Step 6-8: Tools 系统**
- 真正的工具实现
- HTTP 工具、代码执行
- 工具生态建设
