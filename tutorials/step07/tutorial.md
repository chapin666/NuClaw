# Step 7: 短期记忆管理

> 目标：限制对话历史长度，实现滑动窗口和对话摘要
> 
> 难度：⭐⭐⭐ | 代码量：约 550 行 | 预计学习时间：2-3 小时

---

## 一、问题引入

### 1.1 Step 6 的问题

会话管理解决了用户隔离，但对话历史会无限增长：

```
用户聊天 1000 轮后：
- history 向量有 1000+ 条消息
- 每次调用 LLM 都要发送 1000 条历史
- Token 费用极高
- 可能超出模型上下文限制（4096/8192/128k tokens）
```

### 1.2 上下文限制的影响

不同 LLM 的上下文长度限制：

| 模型 | 上下文长度 | 大约能容纳 |
|:---|:---|:---|
| GPT-3.5-turbo | 4K / 16K tokens | 3000 / 12000 英文词 |
| GPT-4 | 8K / 32K / 128K tokens | 6000 / 24000 / 96000 英文词 |
| Claude 3 | 200K tokens | 150000 英文词 |

**中文占用更多 tokens**（约 1 汉字 ≈ 1.5 tokens）

### 1.3 本章目标

1. **滑动窗口**：只保留最近 N 轮对话
2. **Token 计数**：精确控制上下文长度
3. **对话摘要**：压缩早期对话为摘要
4. **智能截断**：保留重要信息，移除次要信息

---

## 二、核心概念

### 2.1 滑动窗口机制

```
原始对话（100轮）：
[1, 2, 3, ..., 95, 96, 97, 98, 99, 100]
 ↑                      ↑
最早                    最新

滑动窗口（保留最近10轮）：
[91, 92, 93, 94, 95, 96, 97, 98, 99, 100]
                    ↑
                   窗口
```

**问题：** 丢失了早期的重要信息（如用户自我介绍）

### 2.2 对话摘要机制

```
完整对话：
┌─────────────────────────────────────────────────────────┐
│  [系统提示]                                              │
│  [用户] 你好                                            │
│  [助手] 你好！                                          │
│  [用户] 我叫小明                                        │
│  [助手] 你好小明！                                      │
│  ...（中间 90 轮对话）...                               │
│  [用户] 今天天气如何                                    │
│  [助手] 今天晴天                                        │
│  [用户] 明天呢                                          │
└─────────────────────────────────────────────────────────┘

压缩后：
┌─────────────────────────────────────────────────────────┐
│  [系统提示]                                              │
│  [摘要] 用户叫小明，之前讨论了天气话题...               │
│  [用户] 今天天气如何                                    │
│  [助手] 今天晴天                                        │
│  [用户] 明天呢                                          │
└─────────────────────────────────────────────────────────┘
```

### 2.3 Token 计数策略

**策略对比：**

| 策略 | 优点 | 缺点 |
|:---|:---|:---|
| 轮数限制 | 简单 | 可能超出 token 限制 |
| Token 限制 | 精确 | 需要实时计数 |
| 混合策略 | 灵活 | 复杂 |

**推荐：混合策略**
- 先按轮数滑动窗口
- 再按 token 数截断
- 必要时生成摘要

---

## 三、代码结构详解

### 3.1 Token 计数器

```cpp
class TokenCounter {
public:
    // 简单估算：英文单词数 × 1.3
    // 精确计算需要 tiktoken 库
    static size_t estimate(const std::string& text) {
        size_t tokens = 0;
        
        // 中文按字计数
        for (char c : text) {
            if (static_cast<unsigned char>(c) >= 0x80) {
                tokens += 2;  // 中文字符约 2 tokens
            }
        }
        
        // 英文按空格分词
        size_t words = 0;
        std::istringstream iss(text);
        string word;
        while (iss >> word) words++;
        
        tokens += words;
        return tokens;
    }
    
    // 计算消息列表的 token 数
    static size_t count_messages(const std::vector<json>& messages) {
        size_t total = 0;
        for (const auto& msg : messages) {
            if (msg.contains("content") && msg["content"].is_string()) {
                total += estimate(msg["content"].get<std::string>());
            }
        }
        return total;
    }
};
```

### 3.2 滑动窗口实现

```cpp
class ConversationManager {
public:
    static constexpr size_t MAX_ROUNDS = 10;      // 最多保留 10 轮
    static constexpr size_t MAX_TOKENS = 3000;    // 最多 3000 tokens
    
    void add_message(std::vector<json>& history,
                     const std::string& role,
                     const std::string& content) {
        history.push_back({
            {"role", role},
            {"content", content}
        });
        
        // 应用滑动窗口
        apply_sliding_window(history);
        
        // 应用 token 限制
        apply_token_limit(history);
    }

private:
    void apply_sliding_window(std::vector<json>& history) {
        // 保留系统消息 + 最近 N 轮对话
        // 每轮 = user + assistant (+ tool)
        
        size_t rounds = 0;
        size_t keep_from = 0;
        
        // 从后往前数轮数
        for (int i = history.size() - 1; i >= 0; --i) {
            if (history[i]["role"] == "user") {
                rounds++;
                if (rounds >= MAX_ROUNDS) {
                    keep_from = i;
                    break;
                }
            }
        }
        
        // 保留系统消息
        if (!history.empty() && history[0]["role"] == "system") {
            keep_from = std::min(keep_from, (size_t)1);
        }
        
        // 截断
        if (keep_from > 0) {
            history.erase(history.begin() + keep_from, history.end() - MAX_ROUNDS * 2);
        }
    }
    
    void apply_token_limit(std::vector<json>& history) {
        while (TokenCounter::count_messages(history) > MAX_TOKENS && 
               history.size() > 2) {
            // 移除最早的用户-助手对话对
            size_t remove_idx = (history[0]["role"] == "system") ? 1 : 0;
            
            // 找到要移除的范围（一个完整的对话轮）
            size_t remove_end = remove_idx + 1;
            while (remove_end < history.size() && 
                   history[remove_end]["role"] != "user") {
                remove_end++;
            }
            
            history.erase(history.begin() + remove_idx,
                         history.begin() + remove_end);
        }
    }
};
```

### 3.3 对话摘要生成

```cpp
class ConversationSummarizer {
public:
    ConversationSummarizer(LLMClient& llm) : llm_(llm) {}
    
    // 生成对话摘要
    std::string summarize(const std::vector<json>& history) {
        if (history.size() < 4) return "";  // 太少不需要摘要
        
        // 提取需要摘要的部分（除去系统消息和最近一轮）
        std::vector<json> to_summarize;
        size_t start = (history[0]["role"] == "system") ? 1 : 0;
        size_t end = history.size() - 2;  // 保留最近一轮
        
        for (size_t i = start; i < end && i < history.size(); ++i) {
            to_summarize.push_back(history[i]);
        }
        
        // 构建摘要提示
        std::string prompt = build_summary_prompt(to_summarize);
        
        // 调用 LLM 生成摘要
        std::vector<json> summary_messages = {
            {{"role", "system"}, {"content", "你是一个对话摘要助手。"}},
            {{"role", "user"}, {"content", prompt}}
        };
        
        auto response = llm_.chat(summary_messages, {});
        return response.content;
    }
    
    // 将历史替换为摘要 + 最近对话
    std::vector<json> compress(std::vector<json>& history) {
        std::string summary = summarize(history);
        
        if (summary.empty()) return history;
        
        std::vector<json> compressed;
        
        // 保留系统消息
        if (!history.empty() && history[0]["role"] == "system") {
            compressed.push_back(history[0]);
        }
        
        // 添加摘要
        compressed.push_back({
            {"role", "system"},
            {"content", "[历史摘要] " + summary}
        });
        
        // 保留最近 2 轮对话
        if (history.size() >= 2) {
            compressed.push_back(history[history.size() - 2]);
            compressed.push_back(history[history.size() - 1]);
        }
        
        return compressed;
    }

private:
    std::string build_summary_prompt(const std::vector<json>& history) {
        std::string prompt = "请将以下对话总结为简短的摘要，保留关键信息：\n\n";
        
        for (const auto& msg : history) {
            std::string role = msg.value("role", "");
            std::string content = msg.value("content", "");
            
            if (role == "user") {
                prompt += "用户: " + content + "\n";
            } else if (role == "assistant") {
                prompt += "助手: " + content + "\n";
            }
        }
        
        prompt += "\n摘要：";
        return prompt;
    }

    LLMClient& llm_;
};
```

### 3.4 集成到 Session

```cpp
class AgentSession : public enable_shared_from_this<AgentSession> {
public:
    AgentSession(tcp::socket socket,
                 SessionManager& session_mgr,
                 LLMClient& llm)
        : ws_(move(socket)),
          session_mgr_(session_mgr),
          llm_(llm),
          summarizer_(llm) {}

private:
    void process() {
        // 检查是否需要摘要
        if (session_data_->history.size() > 20) {
            session_data_->history = summarizer_.compress(
                session_data_->history
            );
        }
        
        // 应用滑动窗口和 token 限制
        ConversationManager mgr;
        mgr.add_message(session_data_->history, "", "");
        
        // 调用 LLM
        vector<Tool> tools = {weather_tool};
        LLMResponse response = llm_.chat(session_data_->history, tools);
        
        // ... 处理响应
        
        // 更新会话存储
        session_mgr_.update_history(session_id_, session_data_->history);
    }

    websocket::stream<tcp::socket> ws_;
    SessionManager& session_mgr_;
    LLMClient& llm_;
    ConversationSummarizer summarizer_;
    // ...
};
```

---

## 四、策略对比

### 4.1 不同策略的效果

```
场景：100 轮对话后询问"我叫什么"

策略 1 - 无限制：
结果：记得名字 ✓
Token：8000+ ✗

策略 2 - 滑动窗口（保留10轮）：
结果：忘记名字 ✗
Token：800 ✓

策略 3 - 滑动窗口 + 摘要：
结果：记得名字 ✓
Token：1000 ✓
```

### 4.2 最佳实践

```cpp
// 推荐配置
struct MemoryConfig {
    size_t max_rounds = 10;           // 最多保留轮数
    size_t max_tokens = 3000;         // 最大 token 数
    size_t summary_threshold = 20;    // 超过此轮数生成摘要
    size_t keep_recent = 4;           // 摘要后保留最近轮数
};
```

---

## 五、本章总结

- ✅ 滑动窗口限制对话轮数
- ✅ Token 计数精确控制上下文
- ✅ 对话摘要压缩历史信息
- ✅ 混合策略平衡成本和效果
- ✅ 代码从 500 行扩展到 550 行

---

## 六、课后思考

短期记忆解决了上下文长度问题，但还有局限：

```
用户周一问："我的密码是什么？"
Agent："您的密码是 xxx"

用户周三再问："我的密码是什么？"
Agent："抱歉，我不知道"  ← 摘要把密码细节丢失了！

用户周五说："记住我喜欢 Python"
用户下个月说："我应该学什么编程语言？"
Agent："这取决于您的兴趣"  ← 完全忘记了！
```

需要一种能长期保存、精确检索的记忆机制。

<details>
<summary>点击查看下一章 💡</summary>

**Step 8: 长期记忆与 RAG**

我们将学习：
- 向量数据库（Vector Database）
- 文本嵌入（Text Embedding）
- RAG（检索增强生成）
- 语义搜索

</details>
