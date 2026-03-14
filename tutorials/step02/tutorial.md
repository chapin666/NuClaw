# Step 2: HTTP Keep-Alive 长连接优化

> 目标：实现 HTTP Keep-Alive，支持连接复用
> 
> 难度：⭐⭐ (进阶)
> 
> 代码量：约 180 行（较 Step 1 新增约 60 行）
> 
> 预计学习时间：1.5-2 小时

---

## 问题引入

**Step 1 的问题：**

我们的服务器每次请求后都关闭连接，效率极低。

### TCP 连接的成本

```
客户端              服务器
   │                  │
   │  SYN ──────────▶ │  ① 客户端发送 SYN
   │                  │
   │  SYN-ACK ◀───────│  ② 服务器回应 SYN-ACK
   │                  │
   │  ACK ──────────▶ │  ③ 客户端发送 ACK
   │                  │
   ═══════════════════  连接建立（三次握手 ~1-2ms）
   │                  │
   │  GET / ────────▶ │  ④ HTTP 请求
   │                  │
   │  200 OK ◀────────│  ⑤ HTTP 响应
   │                  │
   ═══════════════════  数据传输完成
   │                  │
   │  FIN ──────────▶ │  ⑥ 关闭请求
   │                  │
   │  ACK ◀───────────│  ⑦ 确认关闭
   │                  │
   │  FIN ◀───────────│  ⑧ 服务器关闭
   │                  │
   │  ACK ──────────▶ │  ⑨ 确认关闭
   │                  │
   ═══════════════════  连接关闭（四次挥手 ~1-2ms）
```

**现实场景：**
- 一个网页有 50 个资源（HTML + CSS + JS + 图片）
- 每个资源都要：三次握手 → 传输 → 四次挥手
- **连接开销占比：80%**，实际数据传输只占 20%

### HTTP 短连接 vs 长连接

```
短连接（Step 1）：
┌────────┐    ┌────────┐    ┌────────┐    ┌────────┐
│ 请求 1 │    │ 请求 2 │    │ 请求 3 │    │ 请求 4 │
│ ═══════│    │ ═══════│    │ ═══════│    │ ═══════│
│握手-传输-关闭│    │握手-传输-关闭│    │握手-传输-关闭│    │握手-传输-关闭│
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

**性能提升：** 4-10 倍吞吐量提升！

---

## 解决方案

### HTTP Keep-Alive 协议

**请求头（客户端发送）：**
```http
GET /api/data HTTP/1.1
Host: localhost:8080
Connection: keep-alive        ← 请求保持连接
```

**响应头（服务器回应）：**
```http
HTTP/1.1 200 OK
Content-Type: application/json
Content-Length: 42
Connection: keep-alive        ← 同意保持连接

{"data": "value"}
```

**或拒绝保持：**
```http
Connection: close             ← 本次后关闭
```

### 长连接的挑战

**问题 1：如何知道请求结束？**

HTTP 协议规定：
- 必须有 `Content-Length` 头，或
- 使用 `Transfer-Encoding: chunked`

否则客户端无法确定响应何时结束。

**问题 2：连接何时关闭？**

不能永远保持，需要策略：
- **超时关闭**：N 秒无请求则关闭（防止资源泄漏）
- **最大请求数**：处理 N 个请求后关闭（防止内存碎片）
- **主动关闭**：服务器可以任何时候发送 `Connection: close`

### 超时机制设计

```
时间轴 ─────────────────────────────────────────────────▶

连接建立
  │
  ▼
┌─────────────────────────────────────┐
│         定时器启动 (30秒)           │
│              ▲                      │
│              │                      │
│   请求到达 ──┘ 重置定时器           │
│              │                      │
│              ▼                      │
│         30秒无请求                  │
│              │                      │
│              ▼                      │
│         超时触发                    │
│              │                      │
│              ▼                      │
│         关闭连接                    │
└─────────────────────────────────────┘
```

---

## 代码对比

### Step 1 vs Step 2 架构对比

**Step 1 架构：**
```
Session
  │
  ├──▶ do_read()
  │       │
  │       ├──▶ async_read_some
  │       │       │
  │       │       ▼
  │       └──▶ callback
  │               │
  │               ├──▶ do_write()
  │               │       │
  │               │       ├──▶ async_write
  │               │       │       │
  │               │       │       ▼
  │               │       └──▶ callback
  │               │               │
  │               │               └──▶ socket.close()  💥 关闭连接
  │               │
  │               └──▶ Session 销毁
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
  │       │       ▼
  │       │   [等待数据或超时]
  │       │       │
  │       │       ├──▶ 数据到达 ──▶ 取消定时器
  │       │       │                      │
  │       │       │                      ▼
  │       │       │               do_response()
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

### 代码修改详解

**Step 1 的关键代码：**
```cpp
class Session {
    void do_write(std::size_t len) {
        auto self = shared_from_this();
        asio::async_write(socket_, asio::buffer(buffer_, len),
            [this, self](boost::system::error_code, std::size_t) {
                socket_.close();  // ← 写完立即关闭
            }
        );
    }
};
```

**Step 2 的修改：**

```cpp
// 新增：HTTP 请求结构
struct HttpRequest {
    std::string method;
    std::string path;
    std::map<std::string, std::string> headers;
    bool keep_alive = false;
};

// 新增：解析 HTTP 请求
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

// 修改：Session 类
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
        // 构造响应
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
                        do_read();  // ← 保持连接，继续读取！
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

---

## 完整源码

```cpp
#include <boost/asio.hpp>
#include <iostream>
#include <sstream>
#include <memory>
#include <map>

namespace asio = boost::asio;
using tcp = asio::ip::tcp;

// HTTP 请求结构
struct HttpRequest {
    std::string method;
    std::string path;
    std::map<std::string, std::string> headers;
    bool keep_alive = false;
};

// 解析 HTTP 请求
HttpRequest parse_request(const std::string& raw) {
    HttpRequest req;
    std::istringstream stream(raw);
    std::string line;
    
    // 解析请求行
    if (std::getline(stream, line)) {
        std::istringstream line_stream(line);
        line_stream >> req.method >> req.path;
    }
    
    // 解析头部
    while (std::getline(stream, line) && line != "\r") {
        auto pos = line.find(':');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t\r") + 1);
            
            req.headers[key] = value;
            if (key == "Connection" && value == "keep-alive") {
                req.keep_alive = true;
            }
        }
    }
    return req;
}

// 会话类
class Session : public std::enable_shared_from_this<Session> {
public:
    Session(tcp::socket socket) 
        : socket_(std::move(socket)),
          timer_(socket_.get_executor()) {}

    void start() {
        do_read();
    }

private:
    void do_read() {
        auto self = shared_from_this();
        
        // 30秒超时
        timer_.expires_after(std::chrono::seconds(30));
        timer_.async_wait([self](boost::system::error_code ec) {
            if (!ec) self->socket_.close();
        });
        
        socket_.async_read_some(
            asio::buffer(buffer_),
            [this, self](boost::system::error_code ec, std::size_t len) {
                timer_.cancel();
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
        response += req.keep_alive ? "Connection: keep-alive\r\n" : "Connection: close\r\n";
        response += "\r\n" + body;
        
        do_write(response, req.keep_alive);
    }

    void do_write(const std::string& response, bool keep_alive) {
        auto self = shared_from_this();
        asio::async_write(socket_, asio::buffer(response),
            [this, self, keep_alive](boost::system::error_code ec, std::size_t) {
                if (!ec && keep_alive) {
                    do_read();  // 循环读取
                }
            }
        );
    }

    tcp::socket socket_;
    asio::steady_timer timer_;
    std::array<char, 1024> buffer_;
};

// 服务器类
class Server {
public:
    Server(asio::io_context& io, unsigned short port)
        : acceptor_(io, tcp::endpoint(tcp::v4(), port)) {
        do_accept();
    }

private:
    void do_accept() {
        acceptor_.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket) {
                if (!ec) {
                    std::make_shared<Session>(std::move(socket))->start();
                }
                do_accept();
            }
        );
    }

    tcp::acceptor acceptor_;
};

int main() {
    try {
        asio::io_context io;
        Server server(io, 8080);
        
        std::cout << "Step 2 Server (Keep-Alive) listening on port 8080...\n";
        io.run();
    } catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
    }
    return 0;
}
```

---

## 编译运行

```bash
cd src/step02
mkdir build && cd build
cmake .. && make -j4
./nuclaw_step02
```

测试连接复用：
```bash
# 使用 HTTP/1.1 默认 Keep-Alive
curl -v http://localhost:8080/
# 注意响应头：Connection: keep-alive

# 使用 nc 发送多个请求（验证连接复用）
nc localhost 8080
GET / HTTP/1.1
Host: localhost
Connection: keep-alive

GET / HTTP/1.1
Host: localhost
Connection: close

# 你会看到两个响应，证明连接复用了！
```

---

## 性能对比

```bash
# 安装 wrk
sudo apt-get install wrk

# 测试长连接
wrk -t4 -c100 -d30s --latency -H "Connection: keep-alive" http://localhost:8080/

# 对比：短连接（修改代码强制关闭）
# wrk -t4 -c100 -d30s --latency http://localhost:8080/
```

**预期结果：**
- 长连接吞吐量：~50,000 req/s
- 短连接吞吐量：~10,000 req/s
- **提升 5 倍！**

---

## 本章总结

- ✅ 解决了 Step 1 的连接频繁建立/关闭问题
- ✅ 理解 HTTP Keep-Alive 协议
- ✅ 掌握 `asio::steady_timer` 超时管理
- ✅ 实现连接复用循环
- ✅ 代码从 120 行扩展到 180 行

---

## 课后思考

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
