# Step 2: HTTP 长连接优化 (Keep-Alive)

> 目标：实现 HTTP Keep-Alive，支持连接复用
> 
> 难度：⭐⭐ (进阶)
> 
> 代码量：约 180 行

## 本节收获

- 理解 HTTP Keep-Alive 机制
- 掌握 Asio 定时器的使用
- 实现连接超时管理

---

## 什么是 Keep-Alive？

### HTTP 短连接的问题

Step 1 中，每个 HTTP 请求后都关闭连接：

```
客户端              服务器
   │                  │
   ├──① 建立 TCP ────▶│  (三次握手 ~1-2ms)
   │                  │
   ├──② 发送请求 ────▶│
   │                  │
   │◀──③ 返回响应 ───┤
   │                  │
   ├──④ 关闭连接 ────▶│  (四次挥手 ~1-2ms)
```

**问题：** 频繁的连接建立/关闭消耗大量资源。

### Keep-Alive 解决方案

```
客户端              服务器
   │                  │
   ├──① 建立 TCP ────▶│  (只建立一次)
   │                  │
   ├──② 请求 1 ──────▶│
   │◀──③ 响应 1 ─────┤
   │                  │
   ├──④ 请求 2 ──────▶│  (复用同一连接)
   │◀──⑤ 响应 2 ─────┤
   │                  │
   ├──⑥ 请求 3 ──────▶│
   │◀──⑦ 响应 3 ─────┤
   │                  │
   ├──⑧ 关闭/超时 ──▶│  (最后才关闭)
```

**优势：** 减少 TCP 握手开销，提高吞吐量。

---

## HTTP Keep-Alive 协议

### 请求头

客户端发送 `Connection: keep-alive` 表示希望保持连接：

```http
GET /api/data HTTP/1.1
Host: localhost:8080
Connection: keep-alive        ← 关键头部
```

### 响应头

服务器回应 `Connection: keep-alive` 表示同意保持：

```http
HTTP/1.1 200 OK
Content-Type: application/json
Content-Length: 42
Connection: keep-alive        ← 关键头部
```

或者 `Connection: close` 表示关闭：

```http
Connection: close             ← 本次后关闭
```

---

## 核心实现

### 1. 检测 Keep-Alive 头部

```cpp
HttpRequest parse_request(const std::string& raw) {
    HttpRequest req;
    // ... 解析其他头部 ...
    
    // 检查 Connection: keep-alive
    if (boost::iequals(key, "Connection") && 
        boost::iequals(value, "keep-alive")) {
        req.keep_alive = true;
    }
}
```

`boost::iequals`：忽略大小写的字符串比较（"Keep-Alive" 和 "keep-alive" 都能匹配）。

### 2. 连接超时管理

长连接不能永远保持，需要设置超时：

```cpp
class Session {
    asio::steady_timer timer_{socket_.get_executor()};
    
    void do_read() {
        // 设置 30 秒超时
        timer_.expires_after(30s);
        timer_.async_wait([self](boost::system::error_code ec) {
            if (!ec) {
                // 超时了，关闭连接
                self->socket_.close();
            }
        });
        
        socket_.async_read_some(..., [this, self](...) {
            timer_.cancel();  // 有数据到达，取消超时
            // 处理请求...
        });
    }
};
```

**超时机制：**
- 每次读取前设置 30 秒定时器
- 如果 30 秒内没有新请求，关闭连接
- 如果有请求到达，取消定时器

### 3. 循环处理请求

```cpp
void do_write(const std::string& body, bool keep_alive) {
    asio::async_write(socket_, ..., 
        [this, self, keep_alive](...) {
            if (keep_alive) {
                // 保持连接，继续读取下一个请求
                do_read();
            }
            // 否则让 Session 对象析构，自动关闭连接
        }
    );
}
```

**关键点：**
- `keep_alive = true`：继续调用 `do_read()`，循环处理
- `keep_alive = false`：不调用 `do_read()`，Session 析构时关闭连接

---

## 定时器详解

### Asio 定时器的两种模式

| 模式 | 函数 | 用途 |
|:---|:---|:---|
| 同步 | `timer.wait()` | 阻塞等待（少用） |
| 异步 | `timer.async_wait(callback)` | 非阻塞，超时后回调 |

### 定时器与 io_context 的关系

```cpp
asio::steady_timer timer{socket_.get_executor()};
// 或
asio::steady_timer timer{io_context};
```

定时器使用与 socket 相同的 `executor`（执行器），确保回调在正确的线程执行。

### 取消定时器

```cpp
timer_.cancel();
```

取消后，回调会被调用，但 `error_code` 为 `operation_aborted`。

---

## 完整运行测试

### 1. 编译运行

```bash
cd src/step02
mkdir build && cd build
cmake .. && make
./nuclaw_step02
```

### 2. 测试 Keep-Alive

```bash
# 使用 HTTP/1.1 默认 Keep-Alive
curl -v http://localhost:8080/
```

注意响应头：
```
< Connection: keep-alive
```

### 3. 测试连接复用

使用 `nc` 发送多个请求：

```bash
# 连接到服务器
nc localhost 8080

# 然后输入两个请求（注意空行）：
GET / HTTP/1.1
Host: localhost
Connection: keep-alive

GET / HTTP/1.1
Host: localhost
Connection: close

```

你会看到两个响应，证明连接复用了！

### 4. 测试超时

```bash
# 连接后 30 秒不发送请求，看连接是否自动关闭
nc localhost 8080
# （等待 30 秒）
```

---

## 性能对比

使用 `wrk` 压测短连接 vs 长连接：

```bash
# 短连接（每次新建连接）
wrk -t4 -c100 -d30s --latency http://localhost:8080/

# 长连接（复用连接）
wrk -t4 -c100 -d30s --latency -H "Connection: keep-alive" http://localhost:8080/
```

**预期结果：** 长连接的吞吐量通常是短连接的 2-5 倍。

---

## 常见问题

### Q: 连接没有保持，每次都关闭

检查：
1. 请求是否包含 `Connection: keep-alive`
2. 响应是否返回 `Connection: keep-alive`
3. 客户端是否复用了连接

### Q: 连接一直不关闭，资源泄漏

检查：
1. 超时定时器是否正确设置
2. 异常情况下是否有关闭连接的处理

### Q: 第二个请求解析失败

注意：`async_read_some` 可能一次读取多个请求的数据。生产环境应使用更完善的 HTTP 解析器（如 Boost.Beast）。

---

## 下一步

→ **Step 3: Agent Loop - 状态机与对话管理**

我们将进入 AI Agent 的核心：
- 状态机设计
- WebSocket 实时通信
- 对话历史管理
