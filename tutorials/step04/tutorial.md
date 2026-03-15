# Step 4: HTTP 会话管理 —— Agent 的上下文保持能力

> 目标：解决 HTTP 无状态问题，使用 Session ID 实现对话上下文持久化
> 
003e 难度：⭐⭐⭐ | 代码量：约 400 行 | 预计学习时间：2-3 小时

---

## 一、HTTP 的无状态问题

### 1.1 Step 3 的局限

Step 3 的规则 AI 使用 HTTP 请求-响应模式，但它是**无状态**的：

```
HTTP 无状态问题：

请求 1：                    请求 2：
┌─────────┐                ┌─────────┐
│  客户端  │──POST /chat──▶│  服务器  │
│         │  {msg:"北京天气"}│ (无记忆)  │
│         │◀───回复─────│         │
└─────────┘                └─────────┘
                              │
                              ▼
                         处理完就忘
                              
请求 2 再来：
POST /chat {msg:"那上海呢"}
        ↓
服务器：？？？什么上海？我不知道你刚才说了北京！
```

**实际场景：**

```
用户: 北京天气怎样？
Agent: 北京今天晴天，25°C

用户: 那上海呢？
Agent: 抱歉，我不太理解。（不知道"那"指的是天气）

问题：HTTP 每次请求独立，服务器不保存上下文
```

### 1.2 解决方案：Session ID

```
有状态的 HTTP Session：

请求 1：
┌─────────┐                              ┌─────────┐
│  客户端  │────POST /chat───────────────▶│  服务器  │
│         │      {msg:"北京天气"}          │         │
│         │◀───────────────{session_id:   │ Session │
│   保存   │                "abc123",      │  Store  │
│ session_id│               reply: "25°C"}  │ (abc123 │
└─────────┘                              │  →北京)  │
                                         └─────────┘

请求 2：
┌─────────┐                              ┌─────────┐
│  客户端  │────POST /chat───────────────▶│  服务器  │
│         │      {session_id:"abc123",    │         │
│         │       msg:"那上海呢"}          │ Session │
│         │◀───────────────{reply:        │  Store  │
│         │                "上海22°C，     │ (abc123 │
│         │                 比北京凉快"}   │ →上海)  │
└─────────┘                              └─────────┘

关键：Session ID 关联多次请求，服务端维护上下文
```

**Session 的优势：**

| 特性 | 无状态 HTTP | HTTP Session |
|:---|:---|:---|
| 上下文保持 | ❌ 无 | ✅ 有 |
| 实现复杂度 | 简单 | 中等 |
| 服务器存储 | 无 | 需要内存/Redis |
| 扩展性 | 好 | 需要粘性会话/共享存储 |
| 适用场景 | 静态资源、简单API | 对话系统、购物车 |

---

## 二、Session 机制详解

### 2.1 Session ID 生成

Session ID 需要满足：
- **唯一性**：不能重复
- **不可预测**：防止伪造
- **足够长度**：通常 128-256 bit

```cpp
class SessionManager {
public:
    // 生成新的 Session ID
    std::string generate_session_id() {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(0, 15);
        
        const char* hex = "0123456789abcdef";
        std::string id;
        for (int i = 0; i < 32; ++i) {
            id += hex[dis(gen)];
        }
        return id;
        // 示例: "a3f7b2d8e9c1..." (256 bit)
    }
};
```

### 2.2 Session 存储

服务端需要存储 Session 数据：

```cpp
struct ChatContext {
    std::string session_id;
    std::string last_intent;    // 上一次的意图
    std::string last_topic;     // 上一次的话题
    std::chrono::steady_clock::time_point last_time;
    int message_count = 0;
    std::vector<std::pair<std::string, std::string>> history;
    
    // 检查上下文是否有效（30秒内）
    bool is_valid() const {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - last_time).count();
        return elapsed < 30;
    }
};

class SessionManager {
private:
    std::map<std::string, std::shared_ptr<ChatContext>> sessions_;
    mutable std::mutex mutex_;  // 线程安全
    
public:
    std::shared_ptr<ChatContext> get_or_create_session(
        const std::string& session_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = sessions_.find(session_id);
        if (it != sessions_.end()) {
            return it->second;  // 返回已有 Session
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

### 2.3 上下文关联处理

```cpp
class ChatEngine {
public:
    std::string process(const std::string& input, ChatContext& ctx) {
        ctx.message_count++;
        ctx.history.push_back({"user", input});
        
        // 1. 检查是否是上下文相关的提问
        if (is_contextual_question(input, ctx)) {
            std::string reply = handle_contextual(input, ctx);
            ctx.last_time = std::chrono::steady_clock::now();
            ctx.history.push_back({"ai", reply});
            return reply;
        }
        
        // 2. 规则匹配
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
        
        return "我不理解你的问题";
    }
    
private:
    // 判断是否是上下文相关的提问
    bool is_contextual_question(const std::string& input, 
                                 const ChatContext& ctx) {
        // 包含代词：那、它、这个、那里
        bool has_pronoun = std::regex_search(input,
            std::regex("那|它|这个|那里", std::regex::icase));
        
        // 且上下文有效（30秒内）
        return has_pronoun && ctx.is_valid();
    }
    
    std::string handle_contextual(const std::string& input, 
                                   ChatContext& ctx) {
        if (ctx.last_intent == "weather_query") {
            std::string reply = get_weather_for_city(input);
            reply += "\n💡 (我理解了你想比较 " + ctx.last_topic + 
                    " 和 " + extract_topic(input, "weather_query") + " 的天气)";
            ctx.last_topic = extract_topic(input, "weather_query");
            return reply;
        }
        return "能再说清楚一点吗？";
    }
};
```

---

## 三、HTTP Session 服务器实现

### 3.1 请求/响应格式

```cpp
// 请求
POST /chat HTTP/1.1
Content-Type: application/json

{
    "message": "北京天气",
    "session_id": ""  // 可选，首次对话为空
}

// 响应
HTTP/1.1 200 OK
Content-Type: application/json

{
    "session_id": "a3f7b2d8e9c1...",
    "reply": "北京今天晴天，25°C",
    "message_count": 1,
    "has_context": false
}
```

### 3.2 完整 Session 实现

```cpp
HttpResponse handle_chat(const HttpRequest& req) {
    try {
        // 1. 解析 JSON body
        json::value jv = json::parse(req.body);
        std::string message = std::string(jv.as_object()["message"].as_string());
        
        // 2. 获取或创建 Session
        std::string session_id;
        if (jv.as_object().contains("session_id")) {
            session_id = std::string(jv.as_object()["session_id"].as_string());
        } else {
            session_id = session_manager_.generate_session_id();
        }
        
        auto ctx = session_manager_.get_or_create_session(session_id);
        
        // 3. 处理消息
        std::string reply = ai_.process(message, *ctx);
        
        // 4. 构建响应
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

## 四、上下文关联示例

### 4.1 场景：天气查询

```bash
# 第 1 轮：建立 Session
$ curl -X POST http://localhost:8080/chat \
    -H "Content-Type: application/json" \
    -d '{"message":"北京天气"}'

{
    "session_id": "a3f7b2d8e9c1f5b6...",
    "reply": "🌤️ 北京今天晴天，气温 25°C，空气质量良好。",
    "message_count": 1,
    "has_context": true
}

# 第 2 轮：使用 Session ID 关联上下文
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

# 第 3 轮：继续关联
$ curl -X POST http://localhost:8080/chat \
    -H "Content-Type: application/json" \
    -d '{
        "session_id": "a3f7b2d8e9c1f5b6...",
        "message": "深圳呢"
    }'

{
    "session_id": "a3f7b2d8e9c1f5b6...",
    "reply": "🌧️ 深圳今天小雨，气温 28°C，湿度较高。\n\n💡 (我理解了你想比较 上海 和 深圳 的天气)",
    "message_count": 3,
    "has_context": true
}
```

**关键点：**
- 首次对话不需要 `session_id`，服务器会生成并返回
- 后续对话必须带上 `session_id`，服务器才能找到上下文
- `has_context` 标识是否有上下文可用

---

## 五、本章小结

**核心收获：**

1. **HTTP 无状态问题**：
   - 每次请求独立，服务器不保存状态
   - 无法支持多轮对话

2. **Session 解决方案**：
   - Session ID 关联多次请求
   - 服务端存储上下文数据
   - 支持上下文关联提问

3. **Session 管理**：
   - 唯一 ID 生成（随机、足够长）
   - 线程安全的存储
   - 过期清理机制

4. **上下文识别**：
   - 代词检测（那、它、这个）
   - 时间窗口（30秒有效）
   - 意图继承

---

## 六、引出的问题

### 6.1 扩展性问题

当前 Session 存储在内存中：

```cpp
std::map<std::string, std::shared_ptr<ChatContext>> sessions_;
```

**问题：**
- 单机内存有限
- 重启后 Session 丢失
- 多机部署无法共享

**需要：** Redis 等分布式存储。

### 6.2 智能问题

规则匹配还是太死板：

```
用户: "我想去一个不太热、有海的地方"

规则 AI: 无法匹配任何关键词

期望: 理解"不太热"+"有海"，推荐青岛、大连、厦门
```

**需要：** LLM 语义理解。

---

**下一章预告（Step 5）：**

接入 **LLM（大语言模型）**：
- 使用 LLM 理解语义，替代规则匹配
- 实现 **Function Calling**：让 LLM 能调用工具
- 构建完整的 **Agent Loop**：理解 → 决策 → 执行 → 回复

Session 提供了对话持久化能力，接下来要让 Agent 拥有真正的"大脑"。
