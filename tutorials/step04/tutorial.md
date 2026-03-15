# Step 4: HTTP 进阶 —— 会话管理与性能优化

> 目标：掌握 HTTP 会话管理、连接复用、中间件模式，构建高性能 AI 服务
> 
003e 难度：⭐⭐⭐ | 代码量：约 450 行 | 预计学习时间：3-4 小时

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
```

### 1.2 解决方案对比

| 方案 | 数据存储位置 | 优点 | 缺点 | 适用场景 |
|:---|:---|:---|:---|:---|
| **Cookie** | 客户端 | 服务端无存储压力 | 容量小(4KB)、不安全 | 记住用户名、追踪 |
| **Session** | 服务端 | 安全、容量大 | 服务端有状态、扩展难 | 登录状态、对话上下文 |
| **JWT** | 客户端（签名） | 无状态、易扩展 | Token 较大、无法撤销 | 微服务认证、API 鉴权 |

**AI 对话系统选择 Session：**
- 对话上下文数据量大（远超 4KB）
- 敏感信息不宜放在客户端
- 需要服务端控制过期时间

---

## 二、Session 机制详解

### 2.1 Session ID 生成

Session ID 需要满足：**唯一、不可预测、足够长**

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

### 2.2 Session 存储方案

```
┌─────────────────────────────────────────────────────────────┐
│                    Session 存储方案对比                       │
├─────────────┬──────────────┬──────────────┬─────────────────┤
│    方案     │     优点     │     缺点     │    适用场景     │
├─────────────┼──────────────┼──────────────┼─────────────────┤
│   内存      │  最快        │  重启丢失    │  单机、开发环境  │
│ (std::map)  │  实现简单    │  无法共享    │                 │
├─────────────┼──────────────┼──────────────┼─────────────────┤
│   Redis     │  持久化      │  增加依赖    │  生产环境、集群  │
│             │  可共享      │  网络开销    │                 │
├─────────────┼──────────────┼──────────────┼─────────────────┤
│  数据库     │  永久存储    │  慢          │  审计、长期分析  │
│ (PostgreSQL)│  可分析      │  复杂        │                 │
└─────────────┴──────────────┴──────────────┴─────────────────┘
```

**代码实现（内存版）：**

```cpp
struct ChatContext {
    std::string session_id;
    std::string last_intent;
    std::string last_topic;
    std::chrono::steady_clock::time_point last_time;
    int message_count = 0;
    std::vector<std::pair<std::string, std::string>> history;
    
    bool is_valid() const {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - last_time).count();
        return elapsed < 30;  // 30秒有效
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
        
        auto ctx = std::make_shared<ChatContext>();
        ctx->session_id = session_id;
        sessions_[session_id] = ctx;
        return ctx;
    }
    
    // 定时清理过期 Session
    void cleanup_expired() {
        std::lock_guard<std::mutex> lock(mutex_);
        auto now = std::chrono::steady_clock::now();
        
        for (auto it = sessions_.begin(); it != sessions_.end();) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                now - it->second->last_time).count();
            if (elapsed > 300) {  // 5分钟过期
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
        
        // 检查是否是上下文相关的提问
        if (is_contextual_question(input, ctx)) {
            std::string reply = handle_contextual(input, ctx);
            ctx.history.push_back({"ai", reply});
            return reply;
        }
        
        // 规则匹配...
        std::string reply = rule_match(input, ctx);
        ctx.history.push_back({"ai", reply});
        return reply;
    }
    
private:
    bool is_contextual_question(const std::string& input, 
                                 const ChatContext& ctx) {
        // 代词检测 + 上下文有效
        bool has_pronoun = std::regex_search(input,
            std::regex("那|它|这个|那里|一样", std::regex::icase));
        return has_pronoun && ctx.is_valid();
    }
    
    std::string handle_contextual(const std::string& input, 
                                   ChatContext& ctx) {
        if (ctx.last_intent == "weather_query") {
            std::string reply = get_weather_for_city(input);
            reply += "\n💡 (我理解了你想比较 " + ctx.last_topic + 
                    " 和其他城市的天气)";
            return reply;
        }
        return "能再说清楚一点吗？";
    }
};
```

---

## 三、HTTP 性能优化

### 3.1 连接复用（Keep-Alive）

**问题：每次请求都新建 TCP 连接？**

```
无 Keep-Alive（每次新建连接）：

请求 1              请求 2              请求 3
├TCP 握手─┤        ├TCP 握手─┤        ├TCP 握手─┤
├─HTTP───┤        ├─HTTP───┤        ├─HTTP───┤
├TCP 断开─┤        ├TCP 断开─┤        ├TCP 断开─┤
└─50ms───┘        └─50ms───┘        └─50ms───┘
总时间: 150ms

有 Keep-Alive（连接复用）：

├TCP 握手─┤
├─HTTP───┤├─HTTP───┤├─HTTP───┤
└────────┘└────────┘└────────┘
总时间: 70ms（节省 53%）
```

**实现：**

```cpp
HttpResponse handle_request(const HttpRequest& req) {
    HttpResponse resp;
    
    // 处理请求...
    resp.body = process(req);
    
    // 启用 Keep-Alive
    resp.headers["Connection"] = "keep-alive";
    resp.headers["Keep-Alive"] = "timeout=60, max=1000";
    
    return resp;
}
```

### 3.2 HTTP 压缩（Gzip）

**减少传输大小，特别对 JSON 响应有效：**

```cpp
// 压缩前：{
//   "reply": "这是一段很长的回复内容...",
//   "session_id": "a3f7b2d8..."
// }  
// 大小：2KB

// 压缩后：1f 8b 08 00 ...（二进制）
// 大小：0.6KB（节省 70%）

HttpResponse compress_if_needed(const HttpResponse& resp, 
                                 const HttpRequest& req) {
    auto it = req.headers.find("Accept-Encoding");
    if (it != req.headers.end() && 
        it->second.find("gzip") != std::string::npos) {
        // 使用 zlib 压缩 body
        resp.body = gzip_compress(resp.body);
        resp.headers["Content-Encoding"] = "gzip";
    }
    return resp;
}
```

### 3.3 HTTP 缓存策略

**减少重复计算：**

```cpp
HttpResponse handle_request(const HttpRequest& req) {
    // 检查客户端缓存
    auto etag_it = req.headers.find("If-None-Match");
    if (etag_it != req.headers.end() && 
        etag_it->second == calculate_etag(req)) {
        HttpResponse resp;
        resp.status_code = 304;  // Not Modified
        return resp;
    }
    
    // 生成响应并设置缓存头
    HttpResponse resp = process(req);
    resp.headers["Cache-Control"] = "max-age=3600";  // 1小时
    resp.headers["ETag"] = calculate_etag(resp.body);
    
    return resp;
}
```

---

## 四、HTTP 中间件模式

### 4.1 什么是中间件？

**中间件是处理 HTTP 请求的"管道"：**

```
请求流程：

Request
    │
    ▼
┌─────────────────────────────────────────────────────┐
│  中间件链（顺序执行）                                  │
│                                                     │
│   ┌──────────┐    ┌──────────┐    ┌──────────┐    │
│   │ 日志记录  │───▶│ 认证检查  │───▶│ 限流控制  │───┼──▶ Handler
│   └──────────┘    └──────────┘    └──────────┘    │      │
│        │                                      │    │      │
│        └──────────────────────────────────────┼────┘      │
│                                               │           │
└───────────────────────────────────────────────┼───────────┘
                                                │           │
                                                ▼           ▼
                                            Response ◀── Response
```

### 4.2 中间件实现

```cpp
// 中间件函数类型
using Middleware = std::function<HttpResponse(HttpRequest, 
    std::function<HttpResponse(HttpRequest)> next)>;

class MiddlewareChain {
public:
    void add(Middleware middleware) {
        middlewares_.push_back(middleware);
    }
    
    HttpResponse execute(HttpRequest req, 
                         std::function<HttpResponse(HttpRequest)> handler) {
        return execute_recursive(req, handler, 0);
    }

private:
    std::vector<Middleware> middlewares_;
    
    HttpResponse execute_recursive(HttpRequest req,
        std::function<HttpResponse(HttpRequest)> handler, size_t index) {
        if (index >= middlewares_.size()) {
            return handler(req);
        }
        
        return middlewares_[index](req, [this, handler, index](HttpRequest r) {
            return execute_recursive(r, handler, index + 1);
        });
    }
};

// 使用示例
void setup_middlewares(MiddlewareChain& chain) {
    // 日志中间件
    chain.add([](HttpRequest req, auto next) {
        auto start = std::chrono::steady_clock::now();
        std::cout << "[Request] " << req.path << std::endl;
        
        auto resp = next(req);
        
        auto elapsed = std::chrono::duration_cast<std::chrono::ms>(
            std::chrono::steady_clock::now() - start).count();
        std::cout << "[Response] " << resp.status_code 
                  << " in " << elapsed << "ms" << std::endl;
        
        return resp;
    });
    
    // 认证中间件
    chain.add([](HttpRequest req, auto next) {
        if (req.path == "/admin" && !is_authenticated(req)) {
            HttpResponse resp;
            resp.status_code = 401;
            resp.body = R"({"error": "unauthorized"})";
            return resp;
        }
        return next(req);
    });
    
    // CORS 中间件
    chain.add([](HttpRequest req, auto next) {
        auto resp = next(req);
        resp.headers["Access-Control-Allow-Origin"] = "*";
        resp.headers["Access-Control-Allow-Methods"] = "GET, POST, OPTIONS";
        return resp;
    });
}
```

### 4.3 中间件 vs 直接编码

| 方式 | 优点 | 缺点 | 适用 |
|:---|:---|:---|:---|
| **直接编码** | 简单直观 | 代码重复、难复用 | 小项目 |
| **中间件模式** | 可复用、可组合、易测试 | 增加抽象层 | 中大型项目 |

---

## 五、RESTful API 设计

### 5.1 URL 设计原则

```
❌ 不好的设计：
GET /getUserInfo?id=123
GET /deleteUser?id=123
POST /createNewOrder

✅ RESTful 设计：
GET    /users/123        # 获取用户信息
DELETE /users/123        # 删除用户
POST   /orders           # 创建订单
PUT    /orders/456       # 更新订单
GET    /orders/456       # 获取订单
```

### 5.2 HTTP 状态码

```cpp
// 2xx 成功
200 OK              // 请求成功
201 Created         // 资源创建成功
204 No Content      // 成功但无返回内容

// 3xx 重定向
301 Moved Permanently   // 永久重定向
304 Not Modified        // 缓存有效

// 4xx 客户端错误
400 Bad Request     // 请求格式错误
401 Unauthorized    // 未认证
403 Forbidden       // 无权限
404 Not Found       // 资源不存在
429 Too Many Requests   // 限流

// 5xx 服务端错误
500 Internal Server Error   // 服务器内部错误
502 Bad Gateway         // 网关错误
503 Service Unavailable // 服务不可用
```

### 5.3 AI 服务的 API 设计

```
我们的 NuClaw API：

POST /chat              # 对话（核心功能）
GET  /health            # 健康检查
GET  /                  # API 信息

可扩展的设计：
GET    /sessions        # 列出所有会话
DELETE /sessions/{id}   # 删除会话
GET    /sessions/{id}/history   # 获取历史记录
POST   /sessions/{id}/clear     # 清空上下文
```

---

## 六、实战测试

### 6.1 Session 上下文测试

```bash
# 第 1 轮：建立 Session
$ curl -X POST http://localhost:8080/chat \
    -H "Content-Type: application/json" \
    -d '{"message":"北京天气"}'

{
    "session_id": "a3f7b2d8e9c1f5b6...",
    "reply": "🌤️ 北京今天晴天，气温 25°C",
    "message_count": 1,
    "has_context": true
}

# 第 2 轮：上下文关联
$ curl -X POST http://localhost:8080/chat \
    -H "Content-Type: application/json" \
    -d '{"session_id":"a3f7b2d8...","message":"那上海呢"}'

{
    "reply": "☁️ 上海今天多云...\n💡 (我理解了你想比较北京和上海的天气)",
    "message_count": 2
}
```

### 6.2 Keep-Alive 测试

```bash
# 使用 HTTP/1.1 默认启用 Keep-Alive
$ curl -v -H "Connection: keep-alive" \
       http://localhost:8080/health

# 观察响应头
< HTTP/1.1 200 OK
< Connection: keep-alive
< Keep-Alive: timeout=60, max=1000
```

### 6.3 性能对比

```bash
# 无 Keep-Alive（每次新建连接）
$ time curl -H "Connection: close" http://localhost:8080/health

# 有 Keep-Alive（连接复用）
$ time curl -H "Connection: keep-alive" http://localhost:8080/health
```

---

## 七、本章小结

**核心收获：**

1. **Session 管理**：
   - Session ID 解决 HTTP 无状态问题
   - 三种存储方案：内存、Redis、数据库
   - 上下文关联实现多轮对话

2. **HTTP 性能优化**：
   - Keep-Alive 连接复用
   - Gzip 压缩减少传输
   - 缓存策略减少重复计算

3. **中间件模式**：
   - 可复用的请求处理管道
   - 日志、认证、CORS 等横切关注点

4. **RESTful 设计**：
   - 统一的 URL 设计规范
   - 正确使用 HTTP 状态码

---

## 八、引出的问题

### 8.1 智能问题

规则匹配还是太死板：

```
用户: "我想去一个不太热、有海的地方"
规则 AI: 无法匹配任何关键词
期望: LLM 理解语义
```

### 8.2 扩展问题

单机 Session 无法水平扩展：

```
用户 A ──▶ Server 1 ──▶ Session 在内存
用户 A ──▶ Server 2 ──▶ 找不到 Session！
```

**需要：** Redis 共享存储或粘性会话。

---

**下一章预告（Step 5）：**

接入 **LLM（大语言模型）**：
- 使用 LLM 理解语义，替代规则匹配
- 实现 **Function Calling**：让 LLM 能调用工具
- 构建完整的 **Agent Loop**

Session 提供了对话持久化，性能优化让服务更高效，接下来要让 Agent 拥有真正的"大脑"。
