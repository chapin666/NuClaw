# Step 3: JSON 序列化与路由系统

> 目标：掌握 JSON 数据格式，实现结构化请求/响应和路由分发
> 
> 难度：⭐⭐ | 代码量：~250行 | 预计：2-2.5小时

---

## 问题引入

### Step 2 的问题

返回的数据是硬编码字符串：

```cpp
string body = R"({"status":"ok","step":2})";
```

动态数据只能用**字符串拼接**：

```cpp
string name = "小明";
int age = 25;
string body = "{\"name\":\"" + name + "\"," +
              "\"age\":" + to_string(age) + "}";

// name 包含引号？💥 JSON 语法错误！
// age 是空值？需要特殊处理 null vs 0
```

### 我们需要什么？

- 结构化数据序列化（告别字符串拼接）
- URL 路由分发（告别 if-else 地狱）

---

## 解决方案

### JSON 数据类型

| 类型 | 示例 | C++ 对应 |
|:---|:---|:---|
| 字符串 | `"hello"` | `string` |
| 数字 | `42`, `3.14` | `int`, `float` |
| 布尔 | `true` | `bool` |
| null | `null` | `nullptr` |
| 数组 | `[1,2,3]` | `vector` |
| 对象 | `{"a":1}` | `struct` |

### nlohmann/json 库

```cpp
#include <nlohmann/json.hpp>
using json = nlohmann::json;

// 创建 JSON
json j;
j["name"] = "小明";
j["age"] = 25;

// 序列化
string str = j.dump();        // 压缩
string pretty = j.dump(4);    // 带缩进

// 解析
json j2 = json::parse(str);
string name = j2["name"];     // "小明"
```

### 路由系统

**没有路由的噩梦（if-else 地狱）：**
```cpp
void handle_request(const HttpRequest& req) {
    if (req.path == "/hello") {
        return say_hello();
    } else if (req.path == "/user") {
        return get_user(req);
    } else if (req.path == "/api/data") {
        return get_data(req);
    } else if (...) {
        // 100 个 else if...
    }
}
```

**使用路由：**
```cpp
Router router;
router.add("/hello", say_hello);
router.add("/user", get_user);

auto response = router.handle(request.path, request);
```

---

## 核心代码

### 结构化数据定义

```cpp
// 请求结构
struct ChatRequest {
    string user_id;
    string message;
    
    static ChatRequest from_json(const string& str) {
        try {
            json j = json::parse(str);
            return ChatRequest{
                j.value("user_id", "anonymous"),
                j.value("message", "")
            };
        } catch (...) {
            return ChatRequest{"anonymous", str};
        }
    }
};

// 响应结构
struct ChatResponse {
    string reply;
    int status = 200;
    string intent;
    
    string to_json() const {
        json j;
        j["reply"] = reply;
        j["status"] = status;
        j["intent"] = intent;
        return j.dump();
    }
};
```

### 路由系统

```cpp
class Router {
public:
    using Handler = function<ChatResponse(const ChatRequest&)>;
    
    void add_route(const string& path, Handler handler) {
        routes_[path] = handler;
    }
    
    ChatResponse handle(const string& path, const ChatRequest& req) {
        auto it = routes_.find(path);
        if (it != routes_.end()) {
            return it->second(req);  // 调用处理函数
        }
        return ChatResponse{"Not found", 404, "error"};
    }

private:
    map<string, Handler> routes_;
};
```

### Session 类（核心修改）

```cpp
class Session : public enable_shared_from_this<Session> {
public:
    Session(tcp::socket socket, Router& router) 
        : socket_(move(socket)),
          timer_(socket_.get_executor()),
          router_(router) {}  // ← 引用路由

    void start() { do_read(); }

private:
    void process_request(const string& raw) {
        // 解析 HTTP
        auto http_req = parse_http_request(raw);
        
        // 提取请求体
        string body = extract_body(raw);
        
        // 解析 JSON 请求
        ChatRequest chat_req = ChatRequest::from_json(body);
        
        // 路由处理
        ChatResponse chat_res = router_.handle(http_req.path, chat_req);
        
        // 发送 JSON 响应
        do_response(chat_res, http_req.keep_alive);
    }

    void do_response(const ChatResponse& res, bool keep_alive) {
        string body = res.to_json();  // ← JSON 序列化
        
        string response = "HTTP/1.1 " + to_string(res.status) + " OK\r\n";
        response += "Content-Type: application/json\r\n";
        response += "Content-Length: " + to_string(body.size()) + "\r\n";
        response += keep_alive ? 
            "Connection: keep-alive\r\n" : "Connection: close\r\n";
        response += "\r\n" + body;
        
        do_write(response, keep_alive);
    }

    tcp::socket socket_;
    steady_timer timer_;
    Router& router_;  // ← 路由引用
    array<char, 4096> buffer_;
};
```

### 入口函数（路由注册）

```cpp
int main() {
    io_context io;
    
    // 设置路由
    Router router;
    router.add_route("/chat", [](const ChatRequest& req) {
        return ChatResponse{
            "收到: " + req.message,
            200,
            "echo"
        };
    });
    
    router.add_route("/hello", [](const ChatRequest& req) {
        return ChatResponse{"你好！", 200, "greeting"};
    });
    
    Server server(io, 8080, router);
    
    cout << "Server listening on port 8080...\n";
    io.run();
    return 0;
}
```

---

## 架构图

### 请求处理流程

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
│   JSON Parser   │  ← ChatRequest::from_json()
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
│     Handler     │  ← 业务逻辑
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│   JSON Response │  ← ChatResponse::to_json()
└─────────────────┘
```

---

## CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.14)
project(nuclaw_step03 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
find_package(Boost REQUIRED COMPONENTS system)

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
curl -X POST -d '{"user_id":"user123","message":"你好"}' \
     http://localhost:8080/chat
# {"intent":"echo","reply":"收到: 你好","status":200}

# 另一个路由
curl http://localhost:8080/hello
# {"intent":"greeting","reply":"你好！","status":200}
```

---

## 本章总结

- ✅ 结构化数据序列化（nlohmann/json）
- ✅ 路由系统实现 URL 分发
- ✅ 代码从 180 行扩展到 250 行

---

## 课后思考

我们的服务器现在有了路由和 JSON 支持，但仍然基于 HTTP 请求-响应模式：

```
客户端：发送请求 → 等待响应
服务器：接收请求 → 处理 → 返回响应
```

**问题：**
1. 服务器无法主动推送
2. 聊天场景需要不断轮询
3. 每次请求都要带完整 HTTP 头部

如果要实现真正的实时双向通信，有什么协议支持？

<details>
<summary>点击查看下一章 💡</summary>

**Step 4: WebSocket 实时通信**

我们将学习：
- WebSocket 协议和 HTTP 握手升级
- 全双工通信（服务器可主动推送）
- WebSocket 帧格式解析

</details>
