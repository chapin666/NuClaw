# Step 2: HTTP Keep-Alive 长连接优化

> 目标：实现 HTTP Keep-Alive，支持连接复用
> 
> 难度：⭐⭐ | 代码量：约 180 行 | 预计学习时间：1.5-2 小时

---

## 一、问题引入

### 1.1 Step 1 的问题

我们的服务器每次请求后都关闭连接，效率极低。

### 1.2 TCP 连接的成本

```
客户端              服务器
   │                  │
   │  SYN ──────────▶ │  ① 客户端发送 SYN
   │  SYN-ACK ◀───────│  ② 服务器回应 SYN-ACK
   │  ACK ──────────▶ │  ③ 客户端发送 ACK
   │                  │
   ═══════════════════  连接建立（三次握手 ~1-2ms）
   │                  │
   │  GET / ────────▶ │  ④ HTTP 请求
   │  200 OK ◀────────│  ⑤ HTTP 响应
   │                  │
   ═══════════════════  数据传输完成
   │                  │
   │  FIN ──────────▶ │  ⑥ 关闭请求
   │  ACK ◀───────────│  ⑦ 确认关闭
   │  FIN ◀───────────│  ⑧ 服务器关闭
   │  ACK ──────────▶ │  ⑨ 确认关闭
   │                  │
   ═══════════════════  连接关闭（四次挥手 ~1-2ms）
```

**现实场景：**
- 一个网页有 50 个资源（HTML + CSS + JS + 图片）
- 每个资源都要：三次握手 → 传输 → 四次挥手
- **连接开销占比：80%**，实际数据传输只占 20%

### 1.3 HTTP 短连接 vs 长连接对比

```
短连接（Step 1）：
┌────────┐    ┌────────┐    ┌────────┐    ┌────────┐
│ 请求 1 │    │ 请求 2 │    │ 请求 3 │    │ 请求 4 │
│ ═══════│    │ ═══════│    │ ═══════│    │ ═══════│
│握手-传-关│    │握手-传-关│    │握手-传-关│    │握手-传-关│
└────────┘    └────────┘    └────────┘    └────────┘
    ↑           ↑           ↑           ↑
    └───────────┴───────────┴───────────┘
            4 次握手 + 4 次关闭 = 8 次往返

长连接（Keep-Alive）：
┌──────────────────────────────────────────────┐
│              同一个 TCP 连接                  │
├──────────────────────────────────────────────┤
│ ════════════════════════════════════════════ │
│    请求 1    请求 2    请求 3    请求 4      │
│    ────      ────      ────      ────       │
│ ════════════════════════════════════════════ │
│           握手 1 次 + 关闭 1 次              │
└──────────────────────────────────────────────┘
            1 次握手 + 1 次关闭 = 2 次往返
```

**性能提升：4-10 倍吞吐量！**

---

## 二、HTTP Keep-Alive 协议详解

### 2.1 协议规范

**HTTP/1.0** 默认短连接，需要显式声明：
```http
GET /api/data HTTP/1.0
Connection: keep-alive        ← 请求保持连接
```

**HTTP/1.1** 默认长连接，需要显式关闭：
```http
GET /api/data HTTP/1.1
Connection: close             ← 请求关闭连接
```

### 2.2 请求头与响应头

**客户端请求：**
```http
GET /api/data HTTP/1.1
Host: localhost:8080
Connection: keep-alive        ← 请求保持连接
```

**服务器响应（同意保持）：**
```http
HTTP/1.1 200 OK
Content-Type: application/json
Content-Length: 42            ← 必须有 Content-Length
Connection: keep-alive        ← 同意保持连接

{"data": "value"}
```

**服务器响应（拒绝保持）：**
```http
HTTP/1.1 200 OK
Content-Length: 42
Connection: close             ← 本次后关闭
```

### 2.3 为什么需要 Content-Length？

HTTP 协议规定，长连接下必须能够确定响应何时结束：

**方案 1：Content-Length（Content-Length 头）**
```http
Content-Length: 42
```
客户端读取 42 字节后，知道响应结束。

**方案 2：Chunked Transfer Encoding**
```http
Transfer-Encoding: chunked

1a\r\n                      ← 第一个 chunk 大小（十六进制）
{"data": "value"}\r\n       ← 数据
0\r\n                       ← 结束标记
\r\n
```

如果没有这两个头，客户端无法确定响应何时结束，只能关闭连接。

### 2.4 连接管理策略

**策略 1：超时关闭**
- N 秒无请求则关闭
- 防止空闲连接占用资源

**策略 2：最大请求数**
- 处理 N 个请求后关闭
- 防止内存碎片

**策略 3：主动关闭**
- 服务器可以任何时候发送 `Connection: close`

---

## 三、定时器机制详解

### 3.1 为什么需要定时器？

长连接不能永远保持，否则：
- 资源泄漏：空闲连接占用内存和文件描述符
- 连接数上限：操作系统对打开的文件描述符有限制

### 3.2 Asio 定时器

**steady_timer** - 高精度定时器：

```cpp
#include <boost/asio/steady_timer.hpp>

asio::steady_timer timer(io);

// 设置 30 秒后超时
timer.expires_after(std::chrono::seconds(30));

// 异步等待
timer.async_wait([](boost::system::error_code ec) {
    if (!ec) {
        // 超时触发（没有被取消）
        socket.close();
    }
    // 如果 ec == asio::error::operation_aborted，表示被取消
});

// 取消定时器
timer.cancel();
```

### 3.3 超时重置逻辑

```
时间轴 ───────────────────────────────────────▶

连接建立
  │
  ▼
启动定时器 (30秒) ────────────────────▶ 超时关闭
       │
       │
   请求到达 ←──────────────────────────┐
       │                               │
       ▼                               │
   取消定时器                           │
       │                               │
       ▼                               │
   处理请求                            │
       │                               │
       ▼                               │
   响应完成                            │
       │                               │
       ▼                               │
   重新启动定时器 (30秒) ──────────────┘
```

---

## 四、代码结构详解

### 4.1 Step 1 vs Step 2 对比

**Step 1 架构：**
```
Session
  │
  ├──▶ do_read()
  │       │
  │       └──▶ async_read_some ──▶ callback
  │               │
  │               └──▶ do_write()
  │                       │
  │                       └──▶ async_write ──▶ callback
  │                               │
  │                               └──▶ socket.close()  💥 关闭连接
  │
  └──▶ (结束，不再读取)
```

**Step 2 架构（Keep-Alive）：**
```
Session
  │
  ├──▶ do_read()
  │       │
  │       ├──▶ 启动定时器 (30秒)
  │       │       │
  │       │       ├──▶ 数据到达 ──▶ 取消定时器
  ��       │       │                      │
  │       │       │                      ▼
  │       │       │              do_response()
  │       │       │                      │
  │       │       │                      ├──▶ do_write()
  │       │       │                      │       │
  │       │       │                      │       ▼
  │       │       │                      │   callback
  │       │       │                      │       │
  │       │       │                      │       └──▶ do_read() ──▶ 循环！
  │       │       │                      │
  │       │       └──▶ 超时 ──▶ socket.close()
  │       │
  │       └──▶ async_read_some
```

### 4.2 Session 类修改详解

**Step 1 的关键代码（关闭连接）：**
```cpp
void do_write(std::size_t len) {
    auto self = shared_from_this();
    asio::async_write(socket_, asio::buffer(buffer_, len),
        [this, self](boost::system::error_code, std::size_t) {
            socket_.close();  // ← 写完立即关闭
        }
    );
}
```

**Step 2 的修改（保持连接）：**

```cpp
class Session : public std::enable_shared_from_this<Session> {
public:
    Session(tcp::socket socket) 
        : socket_(std::move(socket)),
          timer_(socket_.get_executor()) {}  // ← 新增定时器

    void start() {
        do_read();
    }

private:
    void do_read() {
        auto self = shared_from_this();
        
        // 设置 30 秒超时
        timer_.expires_after(std::chrono::seconds(30));
        timer_.async_wait([self](boost::system::error_code ec) {
            if (!ec) {
                // 超时且没有被取消，关闭连接
                self->socket_.close();
            }
        });
        
        socket_.async_read_some(
            asio::buffer(buffer_),
            [this, self](boost::system::error_code ec, std::size_t len) {
                timer_.cancel();  // ← 有数据到达，取消超时
                
                if (!ec) {
                    auto req = parse_request(std::string(buffer_.data(), len));
                    handle_request(req);
                }
            }
        );
    }
    
    void handle_request(const HttpRequest& req) {
        std::string body = R"({"status":"ok","step":2})";
        
        std::string response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: application/json\r\n";
        response += "Content-Length: " + std::to_string(body.size()) + "\r\n";
        
        // 根据请求决定是否保持连接
        if (req.keep_alive) {
            response += "Connection: keep-alive\r\n";
        } else {
            response += "Connection: close\r\n";
        }
        response += "\r\n" + body;
        
        do_write(response, req.keep_alive);
    }

    void do_write(const std::string& response, bool keep_alive) {
        auto self = shared_from_this();
        asio::async_write(socket_, asio::buffer(response),
            [this, self, keep_alive](boost::system::error_code ec, std::size_t) {
                if (!ec) {
                    if (keep_alive) {
                        do_read();  // ← 关键：继续读取！
                    }
                    // 否则不调用 do_read()，Session 销毁，连接关闭
                }
            }
        );
    }

    tcp::socket socket_;
    asio::steady_timer timer_;  // ← 新增超时定时器
    std::array<char, 1024> buffer_;
};
```

### 4.3 HTTP 请求解析

```cpp
struct HttpRequest {
    std::string method;
    std::string path;
    std::map<std::string, std::string> headers;
    bool keep_alive = false;
};

HttpRequest parse_request(const std::string& raw) {
    HttpRequest req;
    std::istringstream stream(raw);
    std::string line;
    
    // 解析请求行：GET /path HTTP/1.1
    if (std::getline(stream, line)) {
        std::istringstream line_stream(line);
        line_stream >> req.method >> req.path;
    }
    
    // 解析请求头
    while (std::getline(stream, line) && line != "\r") {
        auto pos = line.find(':');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            
            // 去除首尾空格
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t\r") + 1);
            
            req.headers[key] = value;
            
            // 检查 Keep-Alive
            if (key == "Connection" && value == "keep-alive") {
                req.keep_alive = true;
            }
        }
    }
    return req;
}
```

### 4.4 CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.14)
project(nuclaw_step02 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost REQUIRED COMPONENTS system)
find_package(Threads REQUIRED)

add_executable(nuclaw_step02 main.cpp)
target_link_libraries(nuclaw_step02 
    Boost::system 
    Threads::Threads
)
```

---

## 五、编译运行

### 5.1 编译

```bash
cd src/step02
mkdir build && cd build
cmake .. && make -j4
./nuclaw_step02
```

### 5.2 测试连接复用

```bash
# 使用 nc 发送多个请求
nc localhost 8080

# 发送第一个请求（Keep-Alive）
GET / HTTP/1.1
Host: localhost
Connection: keep-alive

# 发送第二个请求
GET / HTTP/1.1
Host: localhost
Connection: close

# 你会看到两个响应，证明连接复用了！
```

### 5.3 性能对比

```bash
# 安装 wrk
sudo apt-get install wrk

# 长连接测试
wrk -t4 -c100 -d30s --latency -H "Connection: keep-alive" http://localhost:8080/

# 短连接测试（修改代码强制关闭）
# wrk -t4 -c100 -d30s --latency http://localhost:8080/
```

| 模式 | 吞吐量 |
|:---|:---|
| 短连接 | ~10,000 req/s |
| 长连接 | ~50,000 req/s |
| **提升** | **5 倍！** |

---

## 六、本章总结

- ✅ 解决了 Step 1 的连接频繁建立/关闭问题
- ✅ 理解 HTTP Keep-Alive 协议
- ✅ 掌握 `asio::steady_timer` 超时管理
- ✅ 实现连接复用循环
- ✅ 代码从 120 行扩展到 180 行

---

## 七、课后思考

我们的服务器现在可以返回 JSON 数据了：

```json
{"status":"ok","step":2}
```

但如果要返回动态数据（比如用户信息），我们只能用字符串拼接：

```cpp
std::string name = "小明";
int age = 25;
std::string body = "{\"name\":\"" + name + "\",\"age\":" + std::to_string(age) + "}";
//      ↑
// 噩梦：引号匹配、特殊字符转义、空值处理...
```

这种方式容易出错，难以维护。

有没有标准的方式来序列化/反序列化结构化数据？

<details>
<summary>点击查看下一章 💡</summary>

**Step 3: JSON 序列化与结构化数据**

我们将学习：
- JSON 数据格式标准
- nlohmann/json 现代 C++ 库
- 结构化请求/响应
- 告别字符串拼接！

</details>
