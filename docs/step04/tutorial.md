# Step 4: WebSocket 实时通信 - 解决 HTTP 上下文问题

> 目标：理解 HTTP 的局限，掌握 WebSocket，实现真正的多轮对话
> 
> 难度：⭐⭐⭐ (中等)
> 
> 代码量：约 400 行
> 
> 预计学习时间：3-4 小时

---

## 📚 前置知识

### HTTP 协议回顾

**HTTP（超文本传输协议）** 是互联网上最常用的协议。

**HTTP 的工作方式：**
```
客户端                    服务器
   │                        │
   ├── 1. 建立 TCP 连接 ───▶│
   │                        │
   ├── 2. 发送请求 ────────▶│
   │   GET /hello HTTP/1.1  │
   │   Host: localhost      │
   │                        │
   │◀── 3. 返回响应 ────────┤
   │   HTTP/1.1 200 OK      │
   │   Content-Length: 13   │
   │                        │
   │   Hello World!         │
   │                        │
   ├── 4. 关闭连接 ────────▶│
   │◀── 5. 确认关闭 ────────┤
```

**HTTP 的特点：**
1. **请求-响应模式**：一问一答
2. **无状态**：服务器不记住你是谁
3. **短连接**：每次请求后连接关闭

### 为什么 HTTP 不适合实时对话？

**场景：聊天对话**

```
小明: 你好！
小红: 你好！

小明: 今天天气如何？
小红: （不知道怎么回答，因为不知道小明是谁）
```

**HTTP 的问题：**
- 每次请求都是独立的
- 服务器不知道两次请求来自同一个人
- 无法保持对话的连续性

### 解决方案对比

| 方案 | 原理 | 优点 | 缺点 |
|:---|:---|:---|:---|
| **Cookie/Session** | 用 ID 标识用户 | 兼容性好 | 每次请求仍要重建上下文 |
| **长轮询** | 客户端一直等待 | 兼容性好 | 浪费资源 |
| **WebSocket** | 建立长连接 | 真正的双向实时 | 需要专门支持 |
| **SSE** | 服务器推送 | 单向推送简单 | 只能服务器推 |

**WebSocket 是最佳选择！**

---

## 第一步：深刻理解 HTTP 的问题

### Step 3 的失败案例

```cpp
// 第一次请求
void handle_request_1() {
    ChatContext ctx;                    // 新建上下文
    ctx.user_name = "小明";              // 记录用户名
    ctx.last_topic = "问候";             // 记录话题
    
    process("北京天气", ctx);            // 处理
    // ctx 在这里销毁！所有信息丢失！
}

// 第二次请求（完全独立）
void handle_request_2() {
    ChatContext ctx;                    // 新的空上下文！
    // ctx.user_name 是空的
    // ctx.last_topic 是空的
    
    process("那上海呢", ctx);            // 无法理解"那"指代什么
}
```

### 生活中的类比

**HTTP 像打电话（每次挂断）：**
```
第 1 通电话：
小明: 你好，我是小明，我想问北京天气。
客服: 好的，北京今天晴天。
（挂断）

第 2 通电话：
小明: 那上海呢？
客服: 请问您是谁？您想问什么？
（因为换了接线员，之前的对话没人记得）
```

**WebSocket 像一直连线：**
```
持续通话：
小明: 你好，我是小明。
客服: 你好小明！

小明: 北京天气如何？
客服: 北京今天晴天。

小明: 那上海呢？        ← 客服记得小明，也知道是问天气
客服: 上海今天多云。
（通话持续，不需要重新介绍自己）
```

---

## 第二步：WebSocket 详解

### 什么是 WebSocket？

**官方定义**：在单个 TCP 连接上进行**全双工通信**的协议。

**通俗解释**：
- HTTP：你问我答，然后挂断
- WebSocket：建立专线，随时说话

### WebSocket 握手过程

```
客户端                                      服务器
   │                                          │
   │  1. HTTP Upgrade 请求                    │
   │─────────────────────────────────────────▶│
   │  GET /ws HTTP/1.1                        │
   │  Host: localhost:8080                    │
   │  Upgrade: websocket                      │
   │  Connection: Upgrade                     │
   │  Sec-WebSocket-Key: dGhlIHNhbXBsZQ==    │
   │                                          │
   │  2. 同意升级                              │
   │◀─────────────────────────────────────────│
   │  HTTP/1.1 101 Switching Protocols        │
   │  Upgrade: websocket                      │
   │  Connection: Upgrade                     │
   │  Sec-WebSocket-Accept: s3pPLMBiTxaQ...  │
   │                                          │
   ═══════════════════════════════════════════
   │           WebSocket 连接建立              │
   ═══════════════════════════════════════════
   │                                          │
   │  3. WebSocket 帧：文本消息               │
   │─────────────────────────────────────────▶│
   │  FIN=1, opcode=1 (text)                  │
   │  Payload: "你好"                         │
   │                                          │
   │  4. WebSocket 帧：回复                   │
   │◀─────────────────────────────────────────│
   │  FIN=1, opcode=1 (text)                  │
   │  Payload: "你好！很高兴见到你。"         │
   │                                          │
   │  5. 持续通信...                          │
   │  （连接保持，随时发送消息）              │
```

**关键点：**
1. **HTTP 升级**：先用 HTTP 协商，然后升级到 WebSocket
2. **101 状态码**：表示协议切换成功
3. **持久连接**：握手后连接一直保持

### WebSocket 帧结构

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-------+-+-------------+-------------------------------+
|F|R|R|R| opcode|M| Payload len |    Extended payload length    |
|I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
|N|V|V|V|       |S|             |   (if payload len==126/127)   |
| |1|2|3|       |K|             |                               |
+-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
|     Extended payload length continued, if payload len == 127  |
+ - - - - - - - - - - - - - - - +-------------------------------+
|                               |Masking-key, if MASK set to 1  |
+-------------------------------+-------------------------------+
| Masking-key (continued)       |          Payload Data         |
+-------------------------------- - - - - - - - - - - - - - - -+
:                     Payload Data continued ...                :
+ - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
|                     Payload Data continued ...                |
+---------------------------------------------------------------+
```

**简化理解：**
- **FIN**：是否是最后一帧
- **opcode**：消息类型（1=文本，2=二进制，8=关闭）
- **MASK**：是否掩码（客户端必须掩码）
- **Payload Data**：实际消息内容

### HTTP vs WebSocket 对比

| 特性 | HTTP | WebSocket |
|:---|:---|:---|
| **协议** | HTTP/1.1, HTTP/2 | ws://, wss:// |
| **连接** | 短连接，每次请求新建 | 长连接，一次握手持续通信 |
| **通信** | 客户端请求 → 服务器响应 | 双方随时发送 |
| **头部** | 每次请求都有大头部 | 帧头很小（2-14字节）|
| **实时性** | 差（需要轮询）| 好（立即推送）|
| **适用** | 获取资源、REST API | 实时聊天、游戏、直播 |

---

## 第三步：代码实现详解

### 整体架构变化

```
Step 3 (HTTP):
main.cpp
├── Session          (处理单个 HTTP 请求，然后关闭)
│   └── do_read()    (读取请求)
│   └── do_write()   (发送响应，然后关闭连接)

Step 4 (WebSocket):
main.cpp
├── Session          (检测是否为 WebSocket 升级)
│   └── 如果是 WS: 移交 WebSocketSession
│   └── 如果是 HTTP: 正常 HTTP 响应
│
└── WebSocketSession (处理 WebSocket 连接)
    ├── do_read()    (持续读取消息帧)
    ├── on_read()    (处理消息)
    └── do_write()   (发送回复，保持连接)
```

### WebSocket 握手检测

```cpp
// HTTP 请求解析
struct HttpRequest {
    std::string method;
    std::string path;
    std::map<std::string, std::string> headers;
    
    // 检测是否为 WebSocket 升级请求
    bool is_websocket_upgrade() const {
        auto it = headers.find("Upgrade");
        if (it != headers.end() && it->second == "websocket") {
            // 还要检查 Connection: Upgrade
            auto conn = headers.find("Connection");
            return conn != headers.end() && 
                   conn->second.find("Upgrade") != std::string::npos;
        }
        return false;
    }
};

// Session 处理
class Session : public std::enable_shared_from_this<Session> {
    void do_read() {
        socket_.async_read_some(buffer_, [this](auto ec, size_t len) {
            HttpRequest req = parse_http_request(buffer_, len);
            
            // 关键判断：HTTP 还是 WebSocket？
            if (req.is_websocket_upgrade()) {
                // 移交到 WebSocketSession
                std::make_shared<WebSocketSession>(
                    std::move(socket_)
                )->start();
            } else {
                // 普通 HTTP 请求
                handle_http_request(req);
            }
        });
    }
};
```

### WebSocketSession 类（核心）

```cpp
// WebSocketSession - 管理一个 WebSocket 连接
class WebSocketSession : public std::enable_shared_from_this<WebSocketSession> {
public:
    WebSocketSession(tcp::socket socket, ChatEngine& ai)
        : ws_(std::move(socket))   // 从 TCP socket 创建 WebSocket
        , ai_(ai)
    {}
    
    void start() {
        // 1. WebSocket 握手
        ws_.async_accept([self = shared_from_this()](beast::error_code ec) {
            if (!ec) {
                std::cout << "[+] WebSocket 连接建立\n";
                
                // 2. 发送欢迎消息
                self->send_message("欢迎来到 NuClaw AI！");
                
                // 3. 开始监听消息
                self->do_read();
            }
        });
    }

private:
    void do_read() {
        // 持续读取消息（不像 HTTP 读一次就结束）
        ws_.async_read(buffer_, 
            [self = shared_from_this()](beast::error_code ec, size_t) {
                if (ec == websocket::error::closed) {
                    std::cout << "[-] WebSocket 连接关闭\n";
                    return;
                }
                if (ec) {
                    std::cerr << "[!] 读取错误: " << ec.message() << "\n";
                    return;
                }
                
                // 获取消息内容
                std::string msg = beast::buffers_to_string(self->buffer_.data());
                self->buffer_.consume(self->buffer_.size());
                
                std::cout << "[<] 收到: " << msg << "\n";
                
                // 关键：使用同一个 Context！
                std::string reply = self->ai_.process(msg, self->ctx_);
                
                std::cout << "[>] 回复: " << reply.substr(0, 50) << "...\n";
                
                // 发送回复
                self->send_message(reply);
                
                // 继续读取下一条（循环！）
                self->do_read();
            }
        );
    }
    
    void send_message(const std::string& msg) {
        ws_.text(true);  // 设置为文本帧
        ws_.async_write(asio::buffer(msg),
            [](beast::error_code, size_t) { /* 发送完成 */ }
        );
    }
    
    websocket::stream<tcp::socket> ws_;  // WebSocket 流
    ChatEngine& ai_;                       // AI 引擎
    beast::flat_buffer buffer_;            // 读缓冲区
    ChatContext ctx_;                      // ★★★ 关键：持久的上下文！
};
```

### 关键对比：Context 的持久化

```cpp
// Step 3 (HTTP): Context 每次请求新建
void handle_http_request() {
    ChatContext ctx;  // ← 新建，请求结束后销毁
    process(input, ctx);
    // ctx 销毁，数据丢失
}

// Step 4 (WebSocket): Context 随连接保持
class WebSocketSession {
    ChatContext ctx_;  // ← 成员变量，连接期间一直存在
    
    void on_message(string msg) {
        process(msg, ctx_);  // 使用同一个 ctx_
    }
};
```

---

## 第四步：上下文理解终于工作了

### 测试对比

**Step 3 (HTTP) - 失败：**
```bash
$ curl -X POST -d "北京天气" http://localhost:8080/chat
→ "北京今天晴天"

$ curl -X POST -d "那上海呢" http://localhost:8080/chat  
→ "我不理解"  ← Context 重置了
```

**Step 4 (WebSocket) - 成功：**
```bash
$ wscat -c ws://localhost:8080/ws
Connected.

> 北京天气如何？
< 北京今天晴天，25°C。

> 那上海呢？
< 上海今天多云，22°C。
💡 (我理解了你想比较北京和上海的天气)
```

**为什么能成功？**

因为 `ctx_` 是 `WebSocketSession` 的成员变量：

```cpp
// 第一次消息
on_message("北京天气") {
    ctx_.last_intent = "weather_query";  // 记录
    ctx_.last_topic = "北京";             // 记录
    return "北京今天晴天";
}

// 第二次消息（同一个 ctx_）
on_message("那上海呢") {
    // ctx_.last_intent 还是 "weather_query"
    // 理解"那"指代天气，返回上海天气
    ctx_.last_topic = "上海";  // 更新
    return "上海今天多云\n💡 (我理解了...)";
}
```

---

## 第五步：常见问题

### Q1: WebSocket 和 HTTP 能共存吗？

**可以！** 我们的代码就是共存的：
```cpp
// 同一个端口，自动识别
if (request.is_websocket_upgrade()) {
    // 处理 WebSocket
} else {
    // 处理普通 HTTP
}
```

### Q2: WebSocket 断线了怎么办？

**需要重连机制：**
```javascript
// 浏览器端
let ws = new WebSocket("ws://localhost:8080/ws");

ws.onclose = function() {
    console.log("连接断开，5秒后重连...");
    setTimeout(function() {
        ws = new WebSocket("ws://localhost:8080/ws");
    }, 5000);
};
```

### Q3: 一个服务器能支持多少 WebSocket 连接？

**取决于：**
1. **内存**：每个连接占用内存（约 10-100KB）
2. **文件描述符**：Linux 默认限制 1024，可以调大
3. **CPU**：消息处理逻辑复杂度

**优化方法：**
- 使用 epoll/kqueue（高并发）
- 连接池管理
- 心跳检测（清理死连接）

---

## 本节总结

### 核心概念

1. **HTTP 无状态**：每次请求独立，无法保持上下文
2. **WebSocket**：全双工长连接，真正的实时通信
3. **握手过程**：HTTP Upgrade → 101 Switching → WebSocket 通信
4. **上下文持久化**：连接期间保持状态，实现多轮对话

### 代码演进

```
Step 3: HTTP + 规则 AI (350行)
   ↓ 修改约 100 行
Step 4: WebSocket + 持久上下文 (400行)
   + WebSocketSession 类
   + 握手检测
   + 帧处理
   + 持久 Context
```

### Agent Loop 演进

```
Step 3: 输入 → 规则匹配 → 输出
          ↓（失败）
        上下文丢失

Step 4: 输入 → 规则匹配 → 输出
          ↑___________↓
         WebSocket 保持状态
```

### 仍存在的问题

**规则系统太死板：**
```
用户：今天适合出门吗？
AI：我不理解。
```

明明在问天气，但规则只匹配"天气"关键词。

**下一章：LLM 语义理解**
- 不依赖关键词
- 真正理解用户意图
- 处理任意自然语言

---

## 📝 课后练习

### 练习 1：心跳检测
添加心跳机制：
- 服务器每 30 秒发送 ping
- 客户端回复 pong
- 如果 60 秒没有收到 pong，断开连接

### 练习 2：多用户支持
支持多个用户同时连接：
- 每个 WebSocketSession 有独立的 Context
- 服务器同时处理多个对话

### 练习 3：重连恢复
实现上下文恢复：
- 用户断线后重连
- 通过用户 ID 恢复之前的 Context

### 思考题
1. 为什么 WebSocket 的头部比 HTTP 小？
2. WebSocket 适合什么场景？不适合什么场景？
3. 如果要在 WebSocket 上实现文件传输，要注意什么？

---

## 📖 扩展阅读

### WebSocket 应用场景

| 场景 | 为什么用 WebSocket |
|:---|:---|
| **实时聊天** | 消息立即推送，不需要轮询 |
| **在线游戏** | 低延迟，双向通信 |
| **股票行情** | 服务器主动推送价格变化 |
| **协同编辑** | 实时同步光标和内容 |
| **直播弹幕** | 实时显示观众评论 |

### WebSocket 调试工具

1. **wscat**：命令行 WebSocket 客户端
   ```bash
   npm install -g wscat
   wscat -c ws://localhost:8080/ws
   ```

2. **浏览器开发者工具**：
   - F12 → Network → WS 标签
   - 可以看到所有 WebSocket 连接和消息

3. **Postman**：支持 WebSocket 测试

---

**恭喜！** 你现在实现了一个能真正对话的 Agent。下一章我们将升级"大脑"，用 LLM 替代死板的规则系统。
