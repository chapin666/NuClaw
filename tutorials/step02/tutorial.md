# Step 2: HTTP 协议解析与路由系统 —— Agent 的请求理解

> 目标：完整解析 HTTP 协议，实现 URL 路由分发，让 Agent 能识别不同请求
> 
003e 难度：⭐⭐ | 代码量：约 271 行 | 预计学习时间：2-3 小时

---

## 一、为什么需要 HTTP 路由？

### 1.1 Step 1 的局限

Step 1 实现了异步 I/O，但请求处理还是太简单：

```cpp
void on_read(...) {
    // 不管什么请求，都返回相同的内容
    std::string response = "HTTP/1.1 200 OK\r\n\r\nHello";
    async_write(socket, response, ...);
}
```

**实际 Agent 需要区分不同请求：**

```
GET /chat?message=你好     ──▶  聊天接口
POST /api/tools/weather    ──▶  工具调用接口  
GET /health                ──▶  健康检查
GET /                      ──▶  首页
```

### 1.2 路由的核心价值

路由系统是 Web 框架的基础，它解决**"请求该由谁处理"**的问题：

```
┌─────────────────────────────────────────────┐
│              HTTP Request                   │
│  GET /api/chat?msg=hello HTTP/1.1           │
└──────────────────┬──────────────────────────┘
                   │
                   ▼
┌─────────────────────────────────────────────┐
│              Router (路由器)                 │
│                                             │
│   if path == "/"          →  HomeHandler   │
│   if path == "/chat"      →  ChatHandler   │
│   if path == "/api/tools" →  ToolHandler   │
│   else                    →  404Handler    │
└──────────────────┬──────────────────────────┘
                   │
                   ▼
┌─────────────────────────────────────────────┐
│            Handler (处理器)                  │
│  执行业务逻辑，生成响应                       │
└─────────────────────────────────────────────┘
```

---

## 二、HTTP 协议深度解析

### 2.1 请求报文结构

一个完整的 HTTP 请求：

```
请求行（Request Line）
├─ 方法：GET
├─ 路径：/api/chat
├─ 查询参数：?user=alice&msg=hello
└─ 协议：HTTP/1.1

请求头（Headers）
├─ Host: api.example.com
├─ Content-Type: application/json
├─ Content-Length: 42
├─ Authorization: Bearer xxx
└─ ...

空行（Empty Line）
└─ \r\n

请求体（Body，可选）
└─ {"message": "你好", "session_id": "abc123"}
```

**关键字段解析：**

| 字段 | 说明 | 示例 |
|:---|:---|:---|
| `Host` | 目标主机 | `localhost:8080` |
| `Content-Type` | 正文格式 | `application/json` |
| `Content-Length` | 正文长度（字节） | `42` |
| `Authorization` | 认证信息 | `Bearer token` |
| `Connection` | 连接方式 | `keep-alive` / `close` |

### 2.2 响应报文结构

```
状态行（Status Line）
├─ 协议：HTTP/1.1
├─ 状态码：200
└─ 状态描述：OK

响应头（Headers）
├─ Content-Type: application/json
├─ Content-Length: 56
└─ ...

空行
└─ \r\n

响应体（Body）
└─ {"reply": "你好！有什么可以帮你的？"}
```

**常见状态码：**

| 状态码 | 含义 | 使用场景 |
|:---|:---|:---|
| 200 | OK | 请求成功 |
| 400 | Bad Request | 请求参数错误 |
| 404 | Not Found | 路径不存在 |
| 405 | Method Not Allowed | 方法不允许（如用 POST 访问只支持 GET 的接口）|
| 500 | Internal Server Error | 服务器内部错误 |

### 2.3 URL 编码与解码

URL 中的特殊字符需要编码：

```
原始：你好世界
编码：%E4%BD%A0%E5%A5%BD%E4%B8%96

原始：hello world
编码：hello%20world

原始：a=b&c=d
编码：a%3Db%26c%3Dd
```

**编码规则：** `%` + 两位十六进制表示字符的 ASCII/UTF-8 值

---

## 三、请求解析器设计

### 3.1 Request 结构体

```cpp
struct Request {
    // 请求行
    std::string method;           // GET, POST, PUT, DELETE...
    std::string path;             // /api/chat（不含查询参数）
    std::string query_string;     // user=alice&msg=hello
    std::string version;          // HTTP/1.1
    
    // 请求头（key-value 存储）
    std::map<std::string, std::string> headers;
    
    // 查询参数（已解析）
    std::map<std::string, std::string> query_params;
    
    // 请求体
    std::string body;
    
    // 获取请求头的便捷方法
    std::string get_header(const std::string& key) const {
        auto it = headers.find(key);
        return it != headers.end() ? it->second : "";
    }
    
    // 获取查询参数的便捷方法
    std::string get_param(const std::string& key) const {
        auto it = query_params.find(key);
        return it != query_params.end() ? it->second : "";
    }
};
```

### 3.2 解析器实现

```cpp
class HttpParser {
public:
    // 解析 HTTP 请求
    // 返回 true 表示解析成功，false 表示数据不完整或格式错误
    bool parse(const char* data, size_t length, Request& req) {
        buffer_.append(data, length);
        
        // Step 1: 查找请求头结束标记（\r\n\r\n）
        size_t header_end = buffer_.find("\r\n\r\n");
        if (header_end == std::string::npos) {
            // 头部还没收完，继续等待
            return false;
        }
        
        // Step 2: 解析请求行和头部
        if (!parse_headers(buffer_.substr(0, header_end), req)) {
            return false;
        }
        
        // Step 3: 处理请求体（如果有 Content-Length）
        size_t body_start = header_end + 4;  // 跳过 \r\n\r\n
        auto it = req.headers.find("Content-Length");
        if (it != req.headers.end()) {
            size_t content_length = std::stoul(it->second);
            
            if (buffer_.length() < body_start + content_length) {
                // 正文还没收完
                return false;
            }
            
            req.body = buffer_.substr(body_start, content_length);
        }
        
        // 解析完成，清空缓冲区
        buffer_.clear();
        return true;
    }

private:
    bool parse_headers(const std::string& header_data, Request& req) {
        std::istringstream stream(header_data);
        std::string line;
        
        // 解析请求行：GET /path?query HTTP/1.1
        if (!std::getline(stream, line)) {
            return false;
        }
        
        // 去除 \r
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        
        std::istringstream request_line(line);
        if (!(request_line >> req.method >> req.path >> req.version)) {
            return false;
        }
        
        // 分离路径和查询参数
        size_t query_pos = req.path.find('?');
        if (query_pos != std::string::npos) {
            req.query_string = req.path.substr(query_pos + 1);
            req.path = req.path.substr(0, query_pos);
            parse_query_string(req.query_string, req.query_params);
        }
        
        // 解析请求头
        while (std::getline(stream, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            
            if (line.empty()) continue;
            
            size_t colon = line.find(':');
            if (colon == std::string::npos) continue;
            
            std::string key = line.substr(0, colon);
            std::string value = line.substr(colon + 1);
            
            // 去除首尾空格
            trim(value);
            
            req.headers[key] = value;
        }
        
        return true;
    }
    
    void parse_query_string(const std::string& qs, 
                           std::map<std::string, std::string>& params) {
        std::istringstream stream(qs);
        std::string pair;
        
        while (std::getline(stream, pair, '&')) {
            size_t eq = pair.find('=');
            if (eq == std::string::npos) continue;
            
            std::string key = url_decode(pair.substr(0, eq));
            std::string value = url_decode(pair.substr(eq + 1));
            
            params[key] = value;
        }
    }
    
    std::string url_decode(const std::string& encoded) {
        std::string decoded;
        for (size_t i = 0; i < encoded.length(); ++i) {
            if (encoded[i] == '%' && i + 2 < encoded.length()) {
                int hex = std::stoi(encoded.substr(i + 1, 2), nullptr, 16);
                decoded += static_cast<char>(hex);
                i += 2;
            } else if (encoded[i] == '+') {
                decoded += ' ';
            } else {
                decoded += encoded[i];
            }
        }
        return decoded;
    }
    
    void trim(std::string& s) {
        size_t start = s.find_first_not_of(" \t");
        size_t end = s.find_last_not_of(" \t");
        if (start == std::string::npos) {
            s.clear();
        } else {
            s = s.substr(start, end - start + 1);
        }
    }

    std::string buffer_;  // 累积的接收数据
};
```

**关键技术点：**

1. **增量解析**：TCP 是流式协议，数据可能分多次到达。需要累积数据直到收到完整的 HTTP 报文。

2. **请求行格式**：`METHOD PATH VERSION`，用空格分隔。

3. **查询参数解析**：
   - `?` 分隔路径和查询串
   - `&` 分隔键值对
   - `=` 分隔键和值
   - 需要 URL 解码（处理 `%XX` 和 `+`）

4. **Content-Length**：HTTP 头部告诉正文有多少字节，必须收完才能处理。

---

## 四、路由系统设计

### 4.1 路由表结构

```cpp
// Handler 类型：接收 Request，返回 Response
using Handler = std::function<std::string(const Request&)>;

class Router {
public:
    // 注册路由
    // method: "GET", "POST", "*"（表示任意方法）
    // path: "/api/chat", "/users/:id"（支持参数）
    void add_route(const std::string& method, 
                   const std::string& path, 
                   Handler handler);
    
    // 路由匹配
    // 返回匹配的路由和路径参数
    std::optional<RouteMatch> match(const std::string& method,
                                     const std::string& path) const;

private:
    struct Route {
        std::string method;
        std::string path;
        Handler handler;
        std::vector<std::string> param_names;  // 路径参数名
    };
    
    std::vector<Route> routes_;
};

struct RouteMatch {
    Handler handler;
    std::map<std::string, std::string> path_params;
};
```

### 4.2 路由匹配实现

```cpp
void Router::add_route(const std::string& method,
                       const std::string& path,
                       Handler handler) {
    Route route;
    route.method = method;
    route.handler = handler;
    
    // 解析路径参数（如 /users/:id 中的 :id）
    std::istringstream stream(path);
    std::string segment;
    std::string pattern;
    
    while (std::getline(stream, segment, '/')) {
        if (segment.empty()) continue;
        
        if (segment[0] == ':') {
            // 路径参数
            route.param_names.push_back(segment.substr(1));
            pattern += "/([^/]+)";  // 正则：匹配任意非/字符
        } else {
            pattern += "/" + segment;
        }
    }
    
    route.path = pattern;
    routes_.push_back(std::move(route));
}

std::optional<RouteMatch> Router::match(const std::string& method,
                                         const std::string& path) const {
    for (const auto& route : routes_) {
        // 方法匹配（* 表示任意方法）
        if (route.method != "*" && route.method != method) {
            continue;
        }
        
        // 路径匹配（正则）
        std::regex route_regex(route.path);
        std::smatch match;
        
        if (std::regex_match(path, match, route_regex)) {
            RouteMatch result;
            result.handler = route.handler;
            
            // 提取路径参数
            for (size_t i = 0; i < route.param_names.size(); ++i) {
                result.path_params[route.param_names[i]] = match[i + 1];
            }
            
            return result;
        }
    }
    
    return std::nullopt;
}
```

### 4.3 使用示例

```cpp
Router router;

// 注册路由
router.add_route("GET", "/", [](const Request& req) {
    return "HTTP/1.1 200 OK\r\n\r\nWelcome to Agent Server!";
});

router.add_route("GET", "/health", [](const Request& req) {
    return "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n{\"status\":\"ok\"}";
});

router.add_route("GET", "/api/chat", [](const Request& req) {
    std::string msg = req.get_param("message");
    std::string response = "Echo: " + msg;
    
    return "HTTP/1.1 200 OK\r\n"
           "Content-Type: text/plain\r\n"
           "Content-Length: " + std::to_string(response.length()) + "\r\n"
           "\r\n" + response;
});

router.add_route("POST", "/api/chat", [](const Request& req) {
    // 处理 POST 请求，req.body 包含 JSON 数据
    return "HTTP/1.1 200 OK\r\n\r\nReceived: " + req.body;
});

router.add_route("GET", "/users/:id", [](const Request& req) {
    // 路径参数在 req.path_params 中
    // 实际实现需要传递 RouteMatch 或使用上下文
    return "HTTP/1.1 200 OK\r\n\r\nUser Profile";
});

// 在 Session 中使用
void Session::on_read(...) {
    Request req;
    if (parser_.parse(buffer, length, req)) {
        auto match = router_.match(req.method, req.path);
        
        if (match) {
            std::string response = match->handler(req);
            async_write(response, ...);
        } else {
            // 404 Not Found
            std::string response = "HTTP/1.1 404 Not Found\r\n\r\nPage not found";
            async_write(response, ...);
        }
    }
}
```

---

## 五、集成到 Session

### 5.1 更新 Session 类

```cpp
class Session : public std::enable_shared_from_this<Session> {
public:
    Session(tcp::socket socket, Router& router)
        : socket_(std::move(socket)),
          router_(router) {}
    
    void start() {
        do_read();
    }

private:
    void do_read() {
        auto self = shared_from_this();
        
        socket_.async_read_some(
            asio::buffer(buffer_),
            [this, self](error_code ec, size_t length) {
                if (ec) {
                    // 连接关闭或出错
                    return;
                }
                
                // 尝试解析请求
                if (parser_.parse(buffer_.data(), length, request_)) {
                    // 解析成功，处理请求
                    handle_request();
                } else {
                    // 数据不完整，继续读取
                    do_read();
                }
            }
        );
    }
    
    void handle_request() {
        auto match = router_.match(request_.method, request_.path);
        
        if (match) {
            response_body_ = match->handler(request_);
        } else {
            response_body_ = "HTTP/1.1 404 Not Found\r\n\r\nNot Found";
        }
        
        do_write();
    }
    
    void do_write() {
        auto self = shared_from_this();
        
        asio::async_write(
            socket_,
            asio::buffer(response_body_),
            [this, self](error_code ec, size_t) {
                if (ec) {
                    return;
                }
                
                // 检查是否保持连接
                auto it = request_.headers.find("Connection");
                if (it != request_.headers.end() && it->second == "close") {
                    socket_.close();
                } else {
                    // HTTP Keep-Alive：继续等待下一个请求
                    request_ = Request();  // 清空请求
                    do_read();
                }
            }
        );
    }

    tcp::socket socket_;
    Router& router_;
    HttpParser parser_;
    Request request_;
    std::array<char, 8192> buffer_;
    std::string response_body_;
};
```

---

## 六、本章小结

**核心收获：**

1. **HTTP 协议解析**：
   - 理解请求行、请求头、请求体的结构
   - 掌握 URL 编码解码规则
   - 学会增量解析（处理 TCP 分包）

2. **路由系统设计**：
   - 路由表的概念：路径到处理函数的映射
   - 路径参数提取（如 `/users/:id`）
   - 方法匹配（GET/POST/PUT/DELETE）

3. **请求处理流程**：
   ```
   收到数据 ──▶ HTTP 解析 ──▶ 路由匹配 ──▶ Handler 执行 ──▶ 发送响应
   ```

---

## 七、引出的问题

### 7.1 智能处理问题

目前的路由处理还是静态响应：

```cpp
router.add_route("GET", "/chat", [](const Request& req) {
    return "Echo: " + req.get_param("message");
});
```

**问题：**
- 如何根据用户输入的"你好"、"Hi"、"Hello"识别相同意图？
- 如何提取"查询北京天气"中的"北京"这个参数？
- 如何让 Agent 真正"理解"用户？

**这引出了下一章：规则 AI —— 基于关键词的意图识别。**

### 7.2 上下文问题

当前的每个请求都是独立的：

```
用户: 你好
Agent: 你好！有什么可以帮你的？

用户: 我叫小明  
Agent: 你好小明！

用户: 我叫什么？
Agent: （不知道，因为之前的状态没有保存）
```

**问题：** Agent 没有记忆能力，无法维持对话上下文。

**未来解决方案：** Session 状态管理 + 对话历史存储。

---

**下一章预告（Step 3）：**

我们将实现基于规则的 AI Agent：
- 关键词匹配和正则表达式
- 意图识别和实体提取
- 简单的对话状态管理
- 让 Agent 能"听懂"用户的话

HTTP 路由解决了"请求到哪去"的问题，接下来要让 Agent 能"理解请求说什么"。
