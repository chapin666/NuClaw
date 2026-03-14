# Step 3: JSON 序列化与路由系统

> 目标：掌握 JSON 数据格式，实现结构化请求/响应和路由分发
> 
> 难度：⭐⭐ (进阶)
> 
> 代码量：约 250 行（较 Step 2 新增约 70 行）
> 
> 预计学习时间：2-2.5 小时

---

## 问题引入

**Step 2 的问题：**

我们的服务器返回的数据是硬编码字符串：

```cpp
std::string body = R"({"status":"ok","step":2})";
```

如果要返回动态数据，只能用**字符串拼接**：

```cpp
//  nightmare 代码示例
std::string name = "小明";
int age = 25;
float score = 95.5;

std::string body = "{\"name\":\"" + name + "\"," +
                   "\"age\":" + std::to_string(age) + "," +
                   "\"score\":" + std::to_string(score) + "}";

// 如果 name 包含引号呢？
name = "小明\"\"";
// 结果：{\"name\":\"小明\"\""... 💥 JSON 语法错误！

// 如果 age 是空值呢？
// 需要特殊处理 null vs 0

// 如果嵌套对象呢？
// 噩梦中的噩梦...
```

**问题：**
- 容易出错（引号、转义、逗号）
- 难以阅读和维护
- 没有类型检查
- 无法处理复杂结构

**本章目标：** 使用 JSON 库实现结构化数据序列化。

---

## 解决方案

### 什么是 JSON？

**JSON（JavaScript Object Notation）** 是一种轻量级数据交换格式：

```json
{
    "name": "小明",
    "age": 25,
    "is_student": true,
    "scores": [95.5, 88.0, 92.5],
    "address": {
        "city": "北京",
        "zip": "100000"
    },
    "tags": ["编程", "阅读"]
}
```

**JSON 数据类型：**
| 类型 | 示例 | C++ 对应 |
|:---|:---|:---|
| 字符串 | `"hello"` | `std::string` |
| 数字 | `42`, `3.14` | `int`, `float` |
| 布尔 | `true`, `false` | `bool` |
| null | `null` | `nullptr` |
| 数组 | `[1, 2, 3]` | `std::vector` |
| 对象 | `{"a":1}` | `struct`/`map` |

### nlohmann/json 库

我们将使用 **nlohmann/json** —— 现代 C++ JSON 库：

```cpp
#include <nlohmann/json.hpp>
using json = nlohmann::json;

// 创建 JSON
json j;
j["name"] = "小明";
j["age"] = 25;

// 序列化为字符串
std::string str = j.dump();
// {"name":"小明","age":25}

std::string pretty = j.dump(4);  // 带缩进
// {
//     "name": "小明",
//     "age": 25
// }

// 从字符串解析
json j2 = json::parse(str);
std::string name = j2["name"];  // "小明"
int age = j2["age"];            // 25

// 嵌套对象
j["address"]["city"] = "北京";

// 数组
j["hobbies"] = {"编程", "阅读"};

// 类型安全访问
if (j.contains("name") && j["name"].is_string()) {
    std::string name = j["name"].get<std::string>();
}
```

### 路由系统

**什么是路由？**

路由是将 URL 映射到处理函数的系统：

```
URL          →  处理函数
────────────────────────────
/hello       →  say_hello()
/user/123    →  get_user(123)
/api/data    →  get_data()
```

**为什么需要路由？**

没有路由的代码（if-else 地狱）：

```cpp
// 噩梦代码
void handle_request(const HttpRequest& req) {
    if (req.path == "/hello") {
        return "Hello!";
    } else if (req.path == "/user") {
        return get_user(req);
    } else if (req.path == "/api/data") {
        return get_data(req);
    } else if (req.path == "/api/login") {
        return login(req);
    } else if (...) {
        // 100 个 else if...
    }
}
```

使用路由：

```cpp
Router router;
router.add("/hello", say_hello);
router.add("/user", get_user);
router.add("/api/data", get_data);

// 使用
auto response = router.handle(request.path, request);
```

---

## 代码对比

### Step 2 vs Step 3 架构对比

**Step 2 架构：**
```
Client Request
     │
     ▼
┌─────────────────┐
│   HTTP Parser   │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│   Fixed Response │  ← 固定响应
│   {"status":"ok"}│
└─────────────────┘
```

**Step 3 架构：**
```
Client Request (JSON)
     │
     ▼
┌─────────────────┐
│   HTTP Parser   │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│   JSON Parser   │  ← 解析请求体
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│     Router      │  ← 路由分发
│  /chat → chat() │
│  /user → user() │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│   Handler       │
│   Process Logic │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│   JSON Response │  ← 结构化响应
└─────────────────┘
```

### 代码修改详解

**Step 2 的关键代码：**
```cpp
void handle_request(const HttpRequest& req) {
    std::string body = R"({"status":"ok","step":2})";  // 固定响应
    // ...
}
```

**Step 3 的修改：**

```cpp
// 新增：引入 JSON 库
#include <nlohmann/json.hpp>
using json = nlohmann::json;

// 新增：结构化请求
struct ChatRequest {
    std::string user_id;
    std::string message;
    
    // 从 JSON 字符串解析
    static ChatRequest from_json(const std::string& str) {
        try {
            json j = json::parse(str);
            ChatRequest req;
            req.user_id = j.value("user_id", "anonymous");
            req.message = j.value("message", "");
            return req;
        } catch (...) {
            // 解析失败，使用默认值
            ChatRequest req;
            req.user_id = "anonymous";
            req.message = str;
            return req;
        }
    }
};

// 新增：结构化响应
struct ChatResponse {
    std::string reply;
    int status = 200;
    std::string intent;
    
    // 序列化为 JSON 字符串
    std::string to_json() const {
        json j;
        j["reply"] = reply;
        j["status"] = status;
        j["intent"] = intent;
        return j.dump();
    }
};

// 新增：路由系统
class Router {
public:
    using Handler = std::function<ChatResponse(const ChatRequest&)>;
    
    void add_route(const std::string& path, Handler handler) {
        routes_[path] = handler;
    }
    
    ChatResponse handle(const std::string& path, const ChatRequest& req) {
        auto it = routes_.find(path);
        if (it != routes_.end()) {
            return it->second(req);  // 调用处理函数
        }
        return ChatResponse{.reply = "Not found", .status = 404};
    }

private:
    std::map<std::string, Handler> routes_;
};

// 修改：Session 类
class Session : public std::enable_shared_from_this<Session> {
public:
    Session(tcp::socket socket, Router& router) 
        : socket_(std::move(socket)),
          timer_(socket_.get_executor()),
          router_(router) {}  // ← 引用路由

    void start() {
        do_read();
    }

private:
    void process_request(const std::string& raw) {
        // 解析 HTTP
        auto http_req = parse_http_request(raw);
        
        // 提取请求体
        std::string body = extract_body(raw);
        
        // 解析 JSON 请求
        ChatRequest chat_req = ChatRequest::from_json(body);
        
        // 路由处理
        ChatResponse chat_res = router_.handle(http_req.path, chat_req);
        
        // 发送 JSON 响应
        do_response(chat_res, http_req.keep_alive);
    }
    
    void do_response(const ChatResponse& res, bool keep_alive) {
        std::string body = res.to_json();  // ← JSON 序列化
        
        std::string response = "HTTP/1.1 " + std::to_string(res.status) + " OK\r\n";
        response += "Content-Type: application/json\r\n";
        response += "Content-Length: " + std::to_string(body.size()) + "\r\n";
        response += keep_alive ? "Connection: keep-alive\r\n" : "Connection: close\r\n";
        response += "\r\n" + body;
        
        do_write(response, keep_alive);
    }

    tcp::socket socket_;
    asio::steady_timer timer_;
    Router& router_;  // ← 引用路由
    std::array<char, 4096> buffer_;
};
```

---

## 完整源码

```cpp
#include <boost/asio.hpp>
#include <nlohmann/json.hpp>
#include <iostream>
#include <sstream>
#include <memory>
#include <map>
#include <functional>

namespace asio = boost::asio;
using tcp = asio::ip::tcp;
using json = nlohmann::json;

// HTTP 请求
struct HttpRequest {
    std::string method;
    std::string path;
    bool keep_alive = false;
};

// 聊天请求
struct ChatRequest {
    std::string user_id;
    std::string message;
    
    static ChatRequest from_json(const std::string& str) {
        try {
            json j = json::parse(str);
            ChatRequest req;
            req.user_id = j.value("user_id", "anonymous");
            req.message = j.value("message", "");
            return req;
        } catch (...) {
            ChatRequest req;
            req.user_id = "anonymous";
            req.message = str;
            return req;
        }
    }
};

// 聊天响应
struct ChatResponse {
    std::string reply;
    int status = 200;
    std::string intent = "chat";
    
    std::string to_json() const {
        json j;
        j["reply"] = reply;
        j["status"] = status;
        j["intent"] = intent;
        return j.dump();
    }
};

// 路由系统
class Router {
public:
    using Handler = std::function<ChatResponse(const ChatRequest&)>;
    
    void add_route(const std::string& path, Handler handler) {
        routes_[path] = handler;
    }
    
    ChatResponse handle(const std::string& path, const ChatRequest& req) {
        auto it = routes_.find(path);
        if (it != routes_.end()) {
            return it->second(req);
        }
        return ChatResponse{.reply = "Not found", .status = 404, .intent = "error"};
    }

private:
    std::map<std::string, Handler> routes_;
};

// 工具函数
HttpRequest parse_http_request(const std::string& raw) {
    HttpRequest req;
    std::istringstream stream(raw);
    std::string line;
    
    if (std::getline(stream, line)) {
        std::istringstream line_stream(line);
        line_stream >> req.method >> req.path;
    }
    
    while (std::getline(stream, line) && line != "\r" && !line.empty()) {
        auto pos = line.find(':');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t\r") + 1);
            
            if (key == "Connection" && value == "keep-alive") {
                req.keep_alive = true;
            }
        }
    }
    return req;
}

std::string extract_body(const std::string& raw) {
    auto pos = raw.find("\r\n\r\n");
    if (pos != std::string::npos) {
        return raw.substr(pos + 4);
    }
    return raw;
}

// Session
class Session : public std::enable_shared_from_this<Session> {
public:
    Session(tcp::socket socket, Router& router) 
        : socket_(std::move(socket)),
          timer_(socket_.get_executor()),
          router_(router) {}

    void start() {
        do_read();
    }

private:
    void do_read() {
        auto self = shared_from_this();
        
        timer_.expires_after(std::chrono::seconds(30));
        timer_.async_wait([self](boost::system::error_code ec) {
            if (!ec) self->socket_.close();
        });
        
        socket_.async_read_some(
            asio::buffer(buffer_),
            [this, self](boost::system::error_code ec, std::size_t len) {
                timer_.cancel();
                if (!ec) {
                    process_request(std::string(buffer_.data(), len));
                }
            }
        );
    }

    void process_request(const std::string& raw) {
        auto http_req = parse_http_request(raw);
        std::string body = extract_body(raw);
        
        ChatRequest chat_req = ChatRequest::from_json(body);
        ChatResponse chat_res = router_.handle(http_req.path, chat_req);
        
        do_response(chat_res, http_req.keep_alive);
    }

    void do_response(const ChatResponse& res, bool keep_alive) {
        std::string body = res.to_json();
        
        std::string response = "HTTP/1.1 " + std::to_string(res.status) + " OK\r\n";
        response += "Content-Type: application/json\r\n";
        response += "Content-Length: " + std::to_string(body.size()) + "\r\n";
        response += keep_alive ? "Connection: keep-alive\r\n" : "Connection: close\r\n";
        response += "\r\n" + body;
        
        do_write(response, keep_alive);
    }

    void do_write(const std::string& response, bool keep_alive) {
        auto self = shared_from_this();
        asio::async_write(socket_, asio::buffer(response),
            [this, self, keep_alive](boost::system::error_code ec, std::size_t) {
                if (!ec && keep_alive) {
                    do_read();
                }
            }
        );
    }

    tcp::socket socket_;
    asio::steady_timer timer_;
    Router& router_;
    std::array<char, 4096> buffer_;
};

// Server
class Server {
public:
    Server(asio::io_context& io, unsigned short port, Router& router)
        : acceptor_(io, tcp::endpoint(tcp::v4(), port)),
          router_(router) {
        do_accept();
    }

private:
    void do_accept() {
        acceptor_.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket) {
                if (!ec) {
                    std::make_shared<Session>(std::move(socket), router_)->start();
                }
                do_accept();
            }
        );
    }

    tcp::acceptor acceptor_;
    Router& router_;
};

int main() {
    try {
        asio::io_context io;
        
        // 设置路由
        Router router;
        router.add_route("/chat", [](const ChatRequest& req) {
            ChatResponse res;
            res.reply = "收到: " + req.message;
            res.intent = "echo";
            return res;
        });
        
        router.add_route("/hello", [](const ChatRequest& req) {
            return ChatResponse{.reply = "你好！", .intent = "greeting"};
        });
        
        Server server(io, 8080, router);
        
        std::cout << "Step 3 Server (JSON + Router) listening on port 8080...\n";
        std::cout << "Try: curl -X POST -d '{\"message\":\"hello\"}' http://localhost:8080/chat\n";
        io.run();
    } catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
    }
    return 0;
}
```

---

## CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.14)
project(nuclaw_step03 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost REQUIRED COMPONENTS system)
find_package(Threads REQUIRED)

# nlohmann/json
include(FetchContent)
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.2
)
FetchContent_MakeAvailable(json)

add_executable(nuclaw_step03 main.cpp)
target_link_libraries(nuclaw_step03 
    Boost::system
    Threads::Threads
    nlohmann_json::nlohmann_json
)
```

---

## 编译运行

```bash
cd src/step03
mkdir build && cd build
cmake .. && make -j4
./nuclaw_step03
```

测试：
```bash
# 结构化 JSON 请求
curl -X POST -d '{"user_id":"user123","message":"你好"}' http://localhost:8080/chat
# {"intent":"echo","reply":"收到: 你好","status":200}

# 另一个路由
curl http://localhost:8080/hello
# {"intent":"greeting","reply":"你好！","status":200}
```

---

## 本章总结

- ✅ 解决了 Step 2 的字符串拼接问题
- ✅ 掌握 JSON 数据格式和 nlohmann/json 库
- ✅ 实现结构化请求/响应
- ✅ 添加路由系统实现 URL 分发
- ✅ 代码从 180 行扩展到 250 行

---

## 课后思考

我们的服务器现在有了路由和 JSON 支持，但仍然基于 HTTP 请求-响应模式：

```
客户端：发送请求 → 等待响应
服务器：接收请求 → 处理 → 返回响应
```

这种模式有什么问题？

1. **服务器无法主动推送**：有新消息时，只能等客户端来问
2. **实时性差**：聊天场景需要不断轮询
3. **头部开销大**：每次请求都要带完整 HTTP 头部

如果要实现真正的实时双向通信（比如聊天室），服务器需要能主动发消息给客户端。

有什么协议支持真正的双向通信？

<details>
<summary>点击查看下一章 💡</summary>

**Step 4: WebSocket 实时通信**

我们将学习：
- WebSocket 协议和 HTTP 握手升级
- 全双工通信（服务器可主动推送）
- WebSocket 帧格式解析
- 实现真正的实时聊天

</details>
