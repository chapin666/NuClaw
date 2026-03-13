# Step 5: LLM 接入 - 从规则到语义理解

> 目标：解决 Step 4 规则匹配的死板问题，接入真实 AI 能力
> 
> 难度：⭐⭐⭐ (中等)
> 
> 代码量：约 450 行

## 本节收获

- 理解规则系统的局限性
- 学习 LLM (大语言模型) 的基本概念
- 实现 HTTP API 调用（OpenAI）
- **Agent Loop 升级**：大脑换成 LLM 了！

---

## 第一步：回顾 Step 4 的核心问题

Step 4 实现了 WebSocket + 规则 AI，但规则太死板：

```
用户：今天适合出门吗？
AI：我不理解。

用户：外面天气怎么样？
AI：我不理解。

用户：告诉我天气
AI：我不理解。
```

**都涉及天气，但因为不匹配"天气"关键词，全部失败！**

### 规则系统的问题

```cpp
// 只能匹配预设模式
if (regex_match(input, "天气")) return get_weather();
if (regex_match(input, "时间")) return get_time();
// 无法处理："适合出门吗"、"外面怎么样"
```

**每增加一个功能，都要：**
1. 想所有可能的表达方式
2. 写正则匹配
3. 测试各种变体

维护成本爆炸！

---

## 第二步：LLM 解决方案

### 什么是 LLM？

**Large Language Model（大语言模型）**
- 在海量文本上训练
- 理解自然语言的语义
- 不需要预设规则

### LLM vs 规则系统对比

| 用户输入 | 规则系统 | LLM |
|:---|:---|:---|
| 今天天气如何？ | ✅ 匹配"天气" | ✅ 理解为天气查询 |
| 外面天气怎么样？ | ❌ 不匹配 | ✅ 理解为天气查询 |
| 今天适合出门吗？ | ❌ 不匹配 | ✅ 理解为天气相关 |
| 现在几点了？ | ✅ 匹配"时间" | ✅ 理解为时间查询 |
| 现在什么时辰？ | ❌ 不匹配 | ✅ 理解为时间查询 |

**LLM 理解语义，不只是匹配关键词。**

### LLM 工作原理（简化）

```
用户输入：今天适合出门吗？

LLM 处理：
1. 分词：[今天, 适合, 出门, 吗, ？]
2. 语义理解：
   - "出门"通常与天气相关
   - "适合"是询问建议
3. 推断意图：用户想了解天气情况
4. 生成回复：建议查询天气
```

---

## 第三步：代码演进

从 Step 4 演进（新增约 50 行）：

```diff
// Step 4: 规则匹配
class ChatEngine {
    string process(string input) {
        for (auto& rule : rules_) {
            if (regex_match(input, rule.pattern))  // ← 死板
                return rule.response();
        }
        return "不理解";
    }
};

// Step 5: LLM 理解
class ChatEngine {
    LLMClient llm_;  // ← 新增
    
    string process(string input) {
        // 构建 prompt
        vector<pair<string, string>> messages = {
            {"system", "你是 AI 助手"},
            {"user", input}
        };
        
        // 调用 LLM API
        return llm_.complete(messages);  // ← 理解语义
    }
};

+ // 新增：LLM HTTP 客户端
+ class LLMClient {
+     string complete(vector<Message> msgs) {
+         // HTTP POST 到 OpenAI API
+         // 返回 LLM 生成的回复
+     }
+ };
```

### 关键改进

```cpp
// Step 4: 规则匹配（只能匹配已知模式）
string reply = rule_based_match(input);

// Step 5: LLM 理解（处理任意输入）
string reply = llm.complete(build_prompt(input));
```

---

## 第四步：LLM API 调用

### OpenAI API 格式

```http
POST https://api.openai.com/v1/chat/completions
Authorization: Bearer sk-xxx
Content-Type: application/json

{
    "model": "gpt-3.5-turbo",
    "messages": [
        {"role": "system", "content": "你是 AI 助手"},
        {"role": "user", "content": "今天适合出门吗？"}
    ]
}
```

### C++ 实现（简化版）

```cpp
class LLMClient {
public:
    string complete(const vector<Message>& messages) {
        // 1. 构建请求 JSON
        json::object request;
        request["model"] = "gpt-3.5-turbo";
        request["temperature"] = 0.7;
        
        json::array msgs;
        for (auto& [role, content] : messages) {
            json::object msg;
            msg["role"] = role;
            msg["content"] = content;
            msgs.push_back(msg);
        }
        request["messages"] = msgs;
        
        // 2. HTTP POST 到 OpenAI
        // （实际实现需要 Boost.Beast HTTPS 客户端）
        
        // 3. 解析响应
        return parse_llm_response(http_response);
    }
};
```

### 注意：API Key 管理

```cpp
// 从环境变量读取，不要硬编码
string get_api_key() {
    const char* key = getenv("OPENAI_API_KEY");
    if (!key) {
        cerr << "请设置 OPENAI_API_KEY 环境变量" << endl;
        return "";
    }
    return key;
}
```

运行前设置：
```bash
export OPENAI_API_KEY="sk-your-key-here"
./server
```

---

## 第五步：对话历史管理

### 为什么需要历史？

多轮对话需要记住之前说了什么：

```
用户：我叫张三。
AI：你好张三！

用户：我的名字是什么？
AI：你的名字是张三。  ← 需要记住上轮对话！
```

### 实现

```cpp
struct ChatContext {
    vector<pair<string, string>> history;  // 对话历史
};

class ChatEngine {
    string process(string input, ChatContext& ctx) {
        // 添加用户消息到历史
        ctx.history.push_back({"user", input});
        
        // 调用 LLM（包含完整历史）
        string reply = llm_.complete(ctx.history);
        
        // 添加 AI 回复到历史
        ctx.history.push_back({"assistant", reply});
        
        return reply;
    }
};
```

### 历史长度限制

LLM 有上下文窗口限制（如 GPT-3.5 是 4K tokens）：

```cpp
// 限制历史长度
if (ctx.history.size() > 20) {
    // 删除最老的对话
    ctx.history.erase(ctx.history.begin(), ctx.history.begin() + 2);
}
```

---

## 第六步：运行测试

### 编译运行

```bash
# 设置 API Key
export OPENAI_API_KEY="sk-your-key"

# 编译
g++ -std=c++17 main.cpp -o server -lboost_system -lpthread

# 运行
./server
```

### WebSocket 测试

```bash
wscat -c ws://localhost:8080/ws

> 你好
< 你好！我是基于 LLM 的 AI 助手。

> 今天适合出门吗？
< 这取决于天气。建议查询当地天气预报。
   （如果是晴天且温度适宜，就很适合出门！）

> 北京天气如何？
< 抱歉，我目前没有实时天气数据接入。
   建议查看天气 App 或网站获取最新信息。
```

### 发现问题！

**LLM 理解了"天气"，但无法获取实时数据！**

知识是静态的（训练时的数据），不知道当前天气。

**下一章解决方案：工具调用**
- LLM 理解需要天气 → 调用天气 API → 返回结果

---

## 本节总结

### 我们解决了什么？

**Step 4 的问题：规则死板 → 无法理解自然语言**

**Step 5 的解决：LLM 接入 → 语义理解**

### Agent Loop 演进

```
Step 4: 输入 → 规则匹配 → 输出
          （死板，只能匹配关键词）

Step 5: 输入 → LLM 理解 → 输出
          （灵活，理解语义）
```

### 代码演进

```
Step 4: WebSocket + 规则 AI (400行)
   ↓ +50 行
Step 5: WebSocket + LLM (450行)
   - LLMClient 类
   - HTTP API 调用
   - 对话历史管理
```

### 仍存在的问题

**LLM 无实时数据：**
```
用户：北京现在多少度？
AI：抱歉，我没有实时天气数据。
```

LLM 的知识是静态的，无法获取：
- 实时天气
- 当前股价
- 数据库查询

**下一章：工具调用（Function Calling）**

让 LLM 能调用外部工具获取实时数据！

---

## 附：LLM vs 规则对比

```cpp
// 规则系统（Step 4）
if (input == "天气") return get_weather();
if (input == "时间") return get_time();
return "不理解";

// LLM（Step 5）
prompt = R"(
用户问：{input}
请理解用户意图并回复。
)";
return llm.complete(prompt);  // 理解任意输入
```

**这就是 Agent 的大脑升级。**
