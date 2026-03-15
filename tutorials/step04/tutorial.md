# Step 4: 多轮对话 —— 让 Agent 记得住（会话级上下文）

> 目标：解决单次对话的局限，实现**会话内的多轮对话上下文**
> 
> 难度：⭐⭐⭐ | 代码量：约 400 行 | 预计学习时间：2-3 小时

---

## 一、Step 3 的局限：金鱼记忆

Step 3 的规则 AI 能识别意图，但有个致命问题——**没有对话上下文**：

```
对话 1：
用户: 北京天气怎么样？
Agent: 🌤️ 北京今天晴天，25°C

对话 2（同一用户，5秒后）：
用户: 那上海呢？
Agent: ❓ 抱歉，我不太理解"那上海呢"是什么意思

问题：每次 HTTP 请求都是全新的，Agent 不记得刚才说了什么
```

**人类的对话是连续的：**

```
用户: 北京天气怎么样？
Agent: 🌤️ 北京今天晴天，25°C

用户: 那上海呢？
Agent: ☁️ 上海今天多云，22°C
       💡（我理解"那"指的是天气，想比较两个城市）

用户: 深圳呢？
Agent: 🌧️ 深圳今天小雨，28°C
       💡（我理解你在继续询问天气）
```

---

## 二、解决方案：对话上下文

### 2.1 核心问题

```
无状态的 HTTP：

请求 1              请求 2              请求 3
├─POST /chat────┐   ├─POST /chat────┐   ├─POST /chat────┐
│ {msg:"北京"}   │   │ {msg:"上海"}   │   │ {msg:"深圳"}   │
└───────────────┘   └───────────────┘   └───────────────┘
        │                   │                   │
        ▼                   ▼                   ▼
   处理完成，忘记         处理完成，忘记         处理完成，忘记

每个请求孤立，无法关联
```

### 2.2 Session 机制

```
有状态的 Session：

        请求 1                    请求 2                    请求 3
┌─────────┐                ┌─────────┐                ┌─────────┐
│  客户端  │──POST /chat───▶│  服务器  │◀──POST /chat──│  客户端  │
│         │  {msg:"北京"}   │         │  {session_id,  │         │
│         │◀──{session_id, │  Session │   msg:"上海"}   │         │
│  保存    │    reply}─────│  Store   │───{reply}────▶│  保存    │
│ session_id│               │ (abc123 │                │ session_id│
└─────────┘                │  →上下文) │                └─────────┘
                           └─────────┘
                                  ▲
                                  │
                           请求 3  │
                           {session_id,
                            msg:"深圳"}
                                  │
                                  ▼
                           ┌─────────┐
                           │  Session│
                           │  Store   │
                           │ (abc123 │
                           │ →更新上下文)
                           └─────────┘

关键：session_id 关联同一用户的多次请求，服务端维护上下文
```

### 2.3 上下文的作用

| 能力 | 说明 | 示例 |
|:---|:---|:---|
| **指代消解** | 理解代词指什么 | "那上海呢" → "那" = 天气 |
| **话题继承** | 记住当前讨论主题 | 连续问多个城市天气 |
| **个性化** | 记住用户信息 | "我叫小明" → "你好小明" |
| **多轮推理** | 多步对话完成复杂任务 | 订机票：出发地→目的地→日期 |

---

## 三、Session 实现

### 3.1 Session ID 生成

```cpp
class SessionManager {
public:
    std::string generate_session_id() {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(0, 15);
        
        const char* hex = "0123456789abcdef";
        std::string id;
        for (int i = 0; i < 32; ++i) {
            id += hex[dis(gen)];
        }
        return id;  // 256-bit，如 "a3f7b2d8e9c1..."
    }
};
```

### 3.2 对话上下文存储

```cpp
struct ChatContext {
    std::string session_id;
    std::string last_intent;     // 上一次的意图（如"weather_query"）
    std::string last_topic;      // 上一次的话题（如"北京"）
    std::vector<std::pair<std::string, std::string>> history;  // 对话历史
    std::chrono::steady_clock::time_point last_time;
    int message_count = 0;
    
    // 检查上下文是否有效（30秒内）
    bool is_valid() const {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - last_time).count();
        return elapsed < 30;
    }
};

class SessionManager {
private:
    std::map<std::string, std::shared_ptr<ChatContext>> sessions_;
    mutable std::mutex mutex_;
    
public:
    std::shared_ptr<ChatContext> get_or_create_session(
        const std::string& session_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = sessions_.find(session_id);
        if (it != sessions_.end()) {
            it->second->last_time = std::chrono::steady_clock::now();
            return it->second;
        }
        
        // 创建新 Session
        auto ctx = std::make_shared<ChatContext>();
        ctx->session_id = session_id;
        sessions_[session_id] = ctx;
        return ctx;
    }
    
    // 清理过期 Session（5分钟未活动）
    void cleanup_expired() {
        std::lock_guard<std::mutex> lock(mutex_);
        auto now = std::chrono::steady_clock::now();
        
        for (auto it = sessions_.begin(); it != sessions_.end();) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                now - it->second->last_time).count();
            if (elapsed > 300) {
                it = sessions_.erase(it);
            } else {
                ++it;
            }
        }
    }
};
```

### 3.3 上下文关联处理

```cpp
class ChatEngine {
public:
    std::string process(const std::string& input, ChatContext& ctx) {
        ctx.message_count++;
        ctx.history.push_back({"user", input});
        
        // 1. 检查是否是上下文相关的提问
        if (is_contextual_question(input, ctx)) {
            std::string reply = handle_contextual(input, ctx);
            ctx.history.push_back({"ai", reply});
            return reply;
        }
        
        // 2. 规则匹配（Step 3 的逻辑）
        for (const auto& rule : rules_) {
            if (std::regex_search(input, rule.pattern)) {
                ctx.last_intent = rule.intent;
                ctx.last_topic = extract_topic(input, rule.intent);
                ctx.last_time = std::chrono::steady_clock::now();
                std::string reply = rule.response(input);
                ctx.history.push_back({"ai", reply});
                return reply;
            }
        }
        
        return "我不理解你的问题，可以试试：你好、时间、北京天气、那上海呢";
    }
    
private:
    // 判断是否是上下文相关的提问
    bool is_contextual_question(const std::string& input, 
                                 const ChatContext& ctx) {
        // 包含代词：那、它、这个、那里
        bool has_pronoun = std::regex_search(input,
            std::regex("那|它|这个|那里", std::regex::icase));
        
        // 且上下文有效（30秒内，有上一个意图）
        return has_pronoun && ctx.is_valid() && !ctx.last_intent.empty();
    }
    
    std::string handle_contextual(const std::string& input, 
                                   ChatContext& ctx) {
        if (ctx.last_intent == "weather_query") {
            std::string reply = get_weather_for_city(input);
            reply += "\n💡 (我理解了你想比较 " + ctx.last_topic + 
                    " 和其他城市的天气)";
            ctx.last_topic = extract_topic(input, "weather_query");
            return reply;
        }
        return "能再说清楚一点吗？或者重新开始一个新话题。";
    }
};
```

---

## 四、HTTP API 设计

### 4.1 请求/响应格式

```cpp
// 请求
POST /chat HTTP/1.1
Content-Type: application/json

{
    "message": "北京天气",
    "session_id": ""  // 首次对话为空，后续带上
}

// 响应
HTTP/1.1 200 OK
Content-Type: application/json

{
    "session_id": "a3f7b2d8e9c1...",  // 首次返回，后续复用
    "reply": "🌤️ 北京今天晴天，25°C",
    "message_count": 1,
    "has_context": true
}
```

### 4.2 完整处理流程

```cpp
HttpResponse handle_chat(const HttpRequest& req) {
    try {
        json::value jv = json::parse(req.body);
        std::string message = std::string(jv.as_object()["message"].as_string());
        
        // 获取或创建 Session
        std::string session_id;
        if (jv.as_object().contains("session_id")) {
            session_id = std::string(jv.as_object()["session_id"].as_string());
        } else {
            session_id = session_manager_.generate_session_id();
        }
        
        auto ctx = session_manager_.get_or_create_session(session_id);
        std::string reply = ai_.process(message, *ctx);
        
        json::object result;
        result["session_id"] = session_id;
        result["reply"] = reply;
        result["message_count"] = ctx->message_count;
        result["has_context"] = !ctx->last_intent.empty();
        
        HttpResponse resp;
        resp.body = json::serialize(result);
        return resp;
        
    } catch (const std::exception& e) {
        HttpResponse resp;
        resp.status_code = 400;
        resp.body = std::string(R"({"error": ")") + e.what() + """}";
        return resp;
    }
}
```

---

## 五、实战测试

### 5.1 首次对话（建立 Session）

```bash
$ curl -X POST http://localhost:8080/chat \
    -H "Content-Type: application/json" \
    -d '{"message":"北京天气"}'

{
    "session_id": "a3f7b2d8e9c1f5b6...",
    "reply": "🌤️ 北京今天晴天，气温 25°C，空气质量良好。",
    "message_count": 1,
    "has_context": true
}
```

### 5.2 上下文关联

```bash
$ curl -X POST http://localhost:8080/chat \
    -H "Content-Type: application/json" \
    -d '{
        "session_id": "a3f7b2d8e9c1f5b6...",
        "message": "那上海呢"
    }'

{
    "session_id": "a3f7b2d8e9c1f5b6...",
    "reply": "☁️ 上海今天多云，气温 22°C，有微风。\n\n💡 (我理解了你想比较 北京 和 上海 的天气)",
    "message_count": 2,
    "has_context": true
}
```

### 5.3 继续对话

```bash
$ curl -X POST http://localhost:8080/chat \
    -H "Content-Type: application/json" \
    -d '{
        "session_id": "a3f7b2d8e9c1f5b6...",
        "message": "深圳呢"
    }'

{
    "reply": "🌧️ 深圳今天小雨，气温 28°C，湿度较高。\n\n💡 (我理解了你在继续询问天气)",
    "message_count": 3
}
```

### 5.4 新 Session（无上下文）

```bash
$ curl -X POST http://localhost:8080/chat \
    -H "Content-Type: application/json" \
    -d '{"message":"那上海呢"}'  // 不带 session_id

{
    "session_id": "b8e2c5d1...",  // 新 Session
    "reply": "❓ 我不太理解\"那上海呢\"是什么意思...",
    "message_count": 1,
    "has_context": false
}
```

---

## 六、进阶场景

### 6.1 个性化信息（当前会话有效）

```cpp
// 记住用户信息（仅本次会话有效）
用户: 我叫小明
Agent: 你好小明，我会在这轮对话中记住你的名字

// 后续对话
用户: 我叫什么？
Agent: 你叫小明呀！（从当前会话上下文中读取）
// ⚠️ 注意：关闭对话后再次打开，Agent 会忘记！
```

### 6.2 多轮任务

```cpp
// 订机票场景
用户: 我要订机票
Agent: 请问从哪里出发？
（context.intent = "book_flight", context.step = 1）

用户: 北京
Agent: 请问目的地是哪里？
（context.slots["from"] = "北京", context.step = 2）

用户: 上海
Agent: 请问什么日期？
（context.slots["to"] = "上海", context.step = 3）

用户: 明天
Agent: 好的，为您查询北京到上海明天的机票...
（context.slots["date"] = "明天"，执行查询）
```

---

## 七、本章小结

**核心收获：**

1. **问题**：Step 3 的单次对话无法满足真实交互需求

2. **解决方案**：
   - Session ID 关联多次请求
   - 服务端维护对话上下文
   - 支持指代消解和话题继承

3. **实现要点**：
   - 随机生成 Session ID
   - 线程安全的 Session 存储
   - 代词检测 + 时间窗口
   - 过期清理机制

4. **局限（重要）**：
   - ⚠️ **会话级上下文**：关闭对话后所有信息丢失
   - 规则 AI 无法理解复杂语义
   - 需要预定义所有意图模式
   - 对新表述缺乏泛化能力

**与长期记忆的区别：**

| | 会话级上下文（本章） | 长期记忆（后续章节） |
|:---|:---|:---|
| **有效时间** | 本次会话（几分钟-几小时） | 跨会话永久保存 |
| **存储位置** | 内存 | 数据库 |
| **内容** | 最近对话、当前话题 | 用户画像、历史偏好 |
| **例子** | "那上海呢"理解指天气 | "我记得你喜欢湖人队" |

> 本章只解决**当前会话内的上下文**，真正的**长期记忆**将在 Step 16 实现。

---

## 八、引出的问题

### 8.1 智能问题

即使有上下文，规则 AI 仍然太死板：

```
用户: "我想去一个不太热、有海的地方"

规则 AI: 无法匹配任何关键词

期望: 理解"不太热"+"有海"，推荐青岛、大连、厦门
```

**问题：如何让 Agent 真正"理解"语义，而不只是匹配关键词？**

### 8.2 扩展问题

单机内存存储 Session 无法水平扩展：

```
用户 A ──▶ Server 1 ──▶ Session 在内存
用户 A ──▶ Server 2 ──▶ 找不到 Session！
```

**问题：多机部署时如何共享 Session？**（提示：Redis）

---

**下一章预告（Step 5）：**

接入 **LLM（大语言模型）**：
- 使用 LLM 理解语义，替代规则匹配
- 实现 **Function Calling**：让 LLM 能调用工具
- **保留 Session**：LLM + 会话上下文 = 既聪明又能多轮对话的 Agent

多轮对话解决了**当前会话内的上下文**问题，接下来要让 Agent 拥有真正的"理解能力"。
**长期记忆将在 Step 16 实现。**