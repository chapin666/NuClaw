# 演示程序架构说明

本文档详细介绍演示程序使用的通用组件架构。

## 目录

- [架构概览](#架构概览)
- [HTTP 客户端](#http-客户端)
- [LLM 客户端](#llm-客户端)
- [HTTP 服务器](#http-服务器)
- [数据库组件](#数据库组件)
- [设计决策](#设计决策)

---

## 架构概览

```
src/demos/
├── include/common/          # 通用组件
│   ├── http_client.hpp      # HTTP 客户端
│   ├── llm_http_client.hpp  # LLM HTTP 客户端
│   ├── http_server.hpp      # HTTP 服务器
│   └── database.hpp         # PostgreSQL 封装
│
├── step06_rest_demo.cpp     # 演示：REST API
├── step07_llm_demo.cpp      # 演示：真实 LLM
├── step08_db_demo.cpp       # 演示：数据库集成
├── step10_test_demo.cpp     # 演示：测试策略
├── test_http_components.cpp # 组件测试
└── test_database.cpp        # 数据库测试
```

---

## HTTP 客户端

**文件：** `include/common/http_client.hpp`

**用途：** 提供同步 HTTP 请求能力，用于调用外部 API（如 OpenAI）。

### 设计

```cpp
class Client {
public:
    // 同步 GET
    Response get(const std::string& url, 
                 const Headers& headers = {},
                 Duration timeout = 30s);
    
    // 同步 POST
    Response post(const std::string& url,
                  const Headers& headers,
                  const std::string& body,
                  Duration timeout = 30s);
};

struct Response {
    int status_code;
    std::map<std::string, std::string> headers;
    std::string body;
    bool success;
    std::string error;
};
```

### 实现要点

- **基于 Boost.Beast：** Header-only，无需额外链接库
- **同步接口：** 简化调用，适合简单场景
- **自动重连：** 每次请求新建连接（简化实现）
- **超时控制：** 可配置超时时间

### 使用示例

```cpp
http::Client client;

// GET 请求
auto resp = client.get("https://api.example.com/data");
if (resp.success) {
    auto data = json::parse(resp.body);
}

// POST 请求
auto resp = client.post(
    "https://api.openai.com/v1/chat/completions",
    {{"Authorization", "Bearer " + key}},
    request_body,
    std::chrono::seconds(60)
);
```

---

## LLM 客户端

**文件：** `include/common/llm_http_client.hpp`

**用途：** 封装 OpenAI/Moonshot API，提供对话能力。

### 设计

```cpp
class LLMHttpClient {
public:
    struct Config {
        std::string provider;   // "openai" 或 "moonshot"
        std::string api_key;
        std::string model;
        std::string base_url;
        float temperature = 0.7f;
        int max_tokens = 1000;
    };
    
    // 从环境变量加载配置
    LLMHttpClient();
    
    // 简单对话
    LLMResponse chat(const std::string& message);
    
    // 带历史记录的对话
    LLMResponse chat_with_history(
        const std::vector<std::pair<std::string, std::string>>& history,
        const std::string& message
    );
};

struct LLMResponse {
    bool success;
    std::string content;
    std::string error;
    int status_code;
    int tokens_used;
    std::string model;
    double latency_ms;
};
```

### 实现要点

- **多提供商支持：** OpenAI 和 Moonshot 统一接口
- **环境变量配置：** 自动读取 `OPENAI_API_KEY` 等
- **历史记录支持：** 维护对话上下文
- **指标采集：** 自动计算延迟和 token 使用量

### 使用示例

```cpp
// 自动从环境变量加载配置
LLMHttpClient llm;

if (!llm.is_configured()) {
    std::cerr << "请设置 OPENAI_API_KEY\n";
    return 1;
}

// 简单对话
auto resp = llm.chat("你好");
if (resp.success) {
    std::cout << resp.content << "\n";
    std::cout << "Tokens: " << resp.tokens_used << "\n";
    std::cout << "Latency: " << resp.latency_ms << "ms\n";
}

// 带历史记录
std::vector<std::pair<std::string, std::string>> history = {
    {"user", "你好"},
    {"assistant", "你好！有什么可以帮助你？"}
};
auto resp = llm.chat_with_history(history, "今天天气如何？");
```

---

## HTTP 服务器

**文件：** `include/common/http_server.hpp`

**用途：** 提供简单的 REST API 服务器。

### 设计

```cpp
class Server {
public:
    Server(asio::io_context& io, unsigned short port);
    
    // 注册路由
    void get(const std::string& path, Handler handler);
    void post(const std::string& path, Handler handler);
    
    // 启动（非阻塞）
    void start();
    void stop();

private:
    using Handler = std::function<json::value(const json::value&)>;
    std::map<std::string, Handler> routes_;
};
```

### 实现要点

- **基于 Boost.Beast：** 异步 I/O，单线程可处理多个连接
- **路由注册：** 简单的字符串匹配
- **JSON 处理：** 自动解析请求 body，序列化响应
- **错误处理：** 自动返回 400/404 等状态码

### 使用示例

```cpp
boost::asio::io_context io;
http::Server server(io, 8080);

// 注册路由
server.post("/chat", [](const json::value& req) {
    auto message = std::string(req.at("message").as_string());
    
    // 处理...
    std::string reply = "收到: " + message;
    
    return json::object({
        {"reply", reply},
        {"success", true}
    });
});

server.get("/health", [](const json::value&) {
    return json::object({{"status", "ok"}});
});

server.start();
io.run();
```

---

## 数据库组件

**文件：** `include/common/database.hpp`

**用途：** PostgreSQL 连接池和 ORM 简化封装。

### 设计

```cpp
class Database {
public:
    struct Config {
        std::string host;
        int port;
        std::string database;
        std::string user;
        std::string password;
        int pool_size = 10;
        
        static Config from_env();  // 从环境变量加载
    };
    
    explicit Database(const Config& config);
    static Database from_env();
    
    // 查询（返回结果）
    template<typename... Args>
    pqxx::result query(const std::string& sql, Args... args);
    
    // 执行更新（返回影响行数）
    template<typename... Args>
    size_t execute(const std::string& sql, Args... args);
    
    // 事务
    template<typename Func>
    void transaction(Func&& func);
    
    // 测试连接
    bool ping();
    
    // 初始化表结构
    void init_schema();
};

// 连接池（内部使用）
class ConnectionPool {
    // ...
};
```

### 实现要点

- **连接池：** 复用数据库连接，减少开销
- **RAII 守卫：** `ConnectionGuard` 自动归还连接
- **参数化查询：** 防止 SQL 注入
- **事务支持：** 保证数据一致性

### 使用示例

```cpp
// 从环境变量加载配置
Database db = Database::from_env();

// 测试连接
if (!db.ping()) {
    std::cerr << "数据库连接失败\n";
    return 1;
}

// 初始化表结构
db.init_schema();

// 执行查询
auto result = db.query(
    "SELECT * FROM users WHERE id = $1 AND status = $2",
    user_id, "active"
);

for (const auto& row : result) {
    std::cout << row["name"].c_str() << "\n";
}

// 执行更新
size_t affected = db.execute(
    "UPDATE users SET last_login = NOW() WHERE id = $1",
    user_id
);

// 事务
db.transaction([](pqxx::work& txn) {
    txn.exec("INSERT INTO orders (...) VALUES (...)");
    txn.exec("UPDATE inventory SET count = count - 1 WHERE ...");
    // 自动提交或回滚
});
```

---

## 设计决策

### 为什么选择 Boost.Beast 而不是其他 HTTP 库？

| 方案 | 优点 | 缺点 | 选择 |
|:---|:---|:---|:---:|
| Boost.Beast | Header-only, C++ 标准, 与 Asio 集成好 | 学习曲线陡峭 | ✅ |
| libcurl | 功能丰富, 稳定 | 需要链接库, C API | |
| cpp-httplib | Header-only, 简单易用 | 功能相对简单 | |

**决策理由：**
- 项目已使用 Boost.Asio，Beast 与之完美集成
- Header-only，简化构建
- 性能优秀，生产可用

### 为什么使用同步 HTTP 客户端？

**现状：** 当前实现是同步的（`http::Client`）

**原因：**
- 简化调用代码，易于理解
- LLM API 调用本身就是阻塞操作
- 演示程序不需要极高并发

**未来改进：**
- 可添加异步接口 `async_get`/`async_post`
- 或使用协程（C++20）

### 为什么选择 libpqxx？

- PostgreSQL 官方 C++ 客户端
- 支持现代 C++（RAII, 异常, 模板）
- 与 PostgreSQL 特性完美集成

### 为什么有连接池？

数据库连接是昂贵资源：
- TCP 握手 + SSL 协商
- 认证过程
- 内存分配

连接池复用连接，显著提升性能。

---

## 扩展建议

### 添加新的 LLM 提供商

在 `llm_http_client.hpp` 的 `load_config_from_env()` 中添加：

```cpp
else if (config_.provider == "anthropic") {
    config_.api_key = std::getenv("ANTHROPIC_API_KEY") ?: "";
    config_.base_url = "https://api.anthropic.com/v1/messages";
    config_.model = "claude-3-sonnet";
}
```

### 添加缓存层

在 `LLMHttpClient` 中添加 Redis 缓存：

```cpp
class LLMHttpClient {
    // ...
    std::optional<std::string> get_cache(const std::string& prompt);
    void set_cache(const std::string& prompt, const std::string& response);
};
```

### 添加连接池监控

在 `ConnectionPool` 中添加指标：

```cpp
struct Stats {
    size_t total_connections;
    size_t active_connections;
    size_t idle_connections;
    size_t wait_queue_length;
};
```
