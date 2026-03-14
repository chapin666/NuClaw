# Step 2: HTTP Keep-Alive 长连接优化

> 目标：实现 HTTP Keep-Alive，支持连接复用
> 
> 难度：⭐⭐ | 代码量：~180行 | 预计：1.5-2小时

---

## 问题引入

### Step 1 的问题

每次请求后都关闭连接，效率极低。

### TCP 连接的成本

```
客户端              服务器
   │                  │
   │  SYN ──────────▶ │  ① 三次握手 ~1-2ms
   │  SYN-ACK ◀───────│
   │  ACK ──────────▶ │
   ═══════════════════  连接建立
   │  GET / ────────▶ │  ② HTTP 请求
   │  200 OK ◀────────│  ③ HTTP 响应
   ═══════════════════  数据传输完成
   │  FIN ──────────▶ │  ④ 四次挥手 ~1-2ms
   │  ...             │
```

**现实场景：** 一个网页 50 个资源 → 50 次握手 + 50 次挥手 = **连接开销占比 80%**

### 短连接 vs 长连接

```
短连接（Step 1）：
┌────────┐    ┌────────┐    ┌────────┐
│ 请求 1 │    │ 请求 2 │    │ 请求 3 │
│握手-传-关│    │握手-传-关│    │握手-传-关│
└────────┘    └────────┘    └────────┘
    ↑           ↑           ↑
    └───────────┴───────────┘
        3 次握手 + 3 次关闭

长连接（Keep-Alive）：
┌──────────────────────────────────────┐
│           同一个 TCP 连接             │
│  ═══════════════════════════════════ │
│     请求 1    请求 2    请求 3       │
│     ────      ────      ────        │
│  ═══════════════════════════════════ │
│       握手 1 次 + 关闭 1 次          │
└──────────────────────────────────────┘
```

**性能提升：4-10 倍吞吐量！**

---

## 解决方案

### HTTP Keep-Alive 协议

**请求头：**
```http
GET /api/data HTTP/1.1
Connection: keep-alive        ← 请求保持连接
```

**响应头：**
```http
HTTP/1.1 200 OK
Content-Length: 42
Connection: keep-alive        ← 同意保持连接
```

### 超时机制设计

```
时间轴 ───────────────────────────────────────▶

连接建立
  │
  ▼
┌─────────────────────────────────────┐
│         定时器启动 (30秒)            │
│              ▲                      │
│              │                      │
│   请求到达 ──┘ 重置定时器            │
│              │                      │
│              ▼                      │
│         30秒无请求                  │
│              │                      │
│              ▼                      │
│         超时触发 → 关闭连接          │
└─────────────────────────────────────┘
```

---

## 核心代码

### Step 1 → Step 2 关键修改

**Step 1 的写入回调（关闭连接）：**
```cpp
void do_write(size_t len) {
    auto self = shared_from_this();
    async_write(socket_, buffer(buffer_, len),
        [this, self](error_code, size_t) {
            socket_.close();  // ← 写完后立即关闭
        }
    );
}
```

**Step 2 的写入回调（保持连接）：**
```cpp
void do_write(const string& response, bool keep_alive) {
    auto self = shared_from_this();
    async_write(socket_, buffer(response),
        [this, self, keep_alive](error_code ec, size_t) {
            if (!ec && keep_alive) {
                do_read();  // ← 关键：继续读取！
            }
            // 否则不调用 do_read()，Session 销毁，连接关闭
        }
    );
}
```

### Session 类（完整核心逻辑）

```cpp
class Session : public enable_shared_from_this<Session> {
public:
    Session(tcp::socket socket) 
        : socket_(move(socket)),
          timer_(socket_.get_executor()) {}  // ← 新增定时器

    void start() { do_read(); }

private:
    void do_read() {
        auto self = shared_from_this();
        
        // 设置 30 秒超时
        timer_.expires_after(chrono::seconds(30));
        timer_.async_wait([self](error_code ec) {
            if (!ec) self->socket_.close();
        });
        
        socket_.async_read_some(buffer(buffer_),
            [this, self](error_code ec, size_t len) {
                timer_.cancel();  // ← 有数据到达，取消超时
                if (!ec) {
                    auto req = parse_request(string(buffer_.data(), len));
                    handle_request(req);
                }
            }
        );
    }
    
    void handle_request(const HttpRequest& req) {
        string body = R"({"status":"ok","step":2})";
        
        string response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Length: " + to_string(body.size()) + "\r\n";
        response += req.keep_alive ? 
            "Connection: keep-alive\r\n" : "Connection: close\r\n";
        response += "\r\n" + body;
        
        do_write(response, req.keep_alive);
    }

    void do_write(const string& response, bool keep_alive) {
        auto self = shared_from_this();
        async_write(socket_, buffer(response),
            [this, self, keep_alive](error_code ec, size_t) {
                if (!ec && keep_alive) {
                    do_read();  // ← 循环读取
                }
            }
        );
    }

    tcp::socket socket_;
    steady_timer timer_;  // ← 新增超时定时器
    array<char, 1024> buffer_;
};
```

### HTTP 请求解析（核心）

```cpp
struct HttpRequest {
    string method;
    string path;
    bool keep_alive = false;
};

HttpRequest parse_request(const string& raw) {
    HttpRequest req;
    istringstream stream(raw);
    string line;
    
    // 解析请求行：GET /path HTTP/1.1
    if (getline(stream, line)) {
        istringstream line_stream(line);
        line_stream >> req.method >> req.path;
    }
    
    // 解析请求头
    while (getline(stream, line) && line != "\r") {
        if (line.find("Connection: keep-alive") != string::npos) {
            req.keep_alive = true;
        }
    }
    return req;
}
```

---

## 架构图

### Step 1 vs Step 2 对比

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
  │                               └──▶ socket.close() 💥
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
  │       │       │                      │
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
# 使用 nc 发送多个请求
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
# 长连接测试
wrk -t4 -c100 -d30s -H "Connection: keep-alive" http://localhost:8080/

# 短连接测试（修改代码强制关闭）
# wrk -t4 -c100 -d30s http://localhost:8080/
```

| 模式 | 吞吐量 |
|:---|:---|
| 短连接 | ~10,000 req/s |
| 长连接 | ~50,000 req/s |
| **提升** | **5 倍！** |

---

## 本章总结

- ✅ 解决了连接频繁建立/关闭问题
- ✅ `asio::steady_timer` 实现超时管理
- ✅ 根据 `Connection` 头决定是否保持连接

---

## 课后思考

我们的服务器现在可以返回 JSON 数据了，但如果要返回动态数据：

```cpp
string name = "小明";
int age = 25;
string body = "{\"name\":\"" + name + "\",\"age\":" + to_string(age) + "}";
// 引号匹配、特殊字符转义、噩梦...
```

有没有标准的方式来序列化/反序列化结构化数据？

<details>
<summary>点击查看下一章 💡</summary>

**Step 3: JSON 序列化与路由系统**

我们将学习：
- JSON 数据格式标准
- nlohmann/json 现代 C++ 库
- 路由系统实现
- 告别字符串拼接！

</details>
