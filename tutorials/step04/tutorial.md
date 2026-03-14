# Step 4: WebSocket 实时通信

> 目标：掌握 WebSocket 协议，实现全双工实时通信
> 
> 难度：⭐⭐⭐ | 代码量：~350行 | 预计：3-4小时

---

## 问题引入

### Step 3 的问题

HTTP 是**请求-响应**模式，服务器无法主动推送：

```
客户端              服务器
   │                  │
   ├── 请求1 ────────▶│
   │◀─ 响应1 ─────────┤
   │                  │
   │                  │ ← 有新消息，但无法主动发送！
   │                  │
   ├── 轮询 ────────▶ │ "有新消息吗？"
   │◀─ 有了！ ────────┤ ← 延迟取决于轮询间隔
```

**轮询的问题：** 浪费带宽、高延迟、服务器压力大

---

## 解决方案

### WebSocket 协议

提供**全双工**通信通道：

```
客户端              服务器
   │                  │
   ├── HTTP 握手 ────▶│  ① 升级为 WebSocket
   │◀─ 101 Switching─┤
   │                  │
   ═══════════════════  升级为 WebSocket
   │                  │
   ├── 消息1 ────────▶│  ② 客户端发送
   │◀─ 消息2 ─────────┤  ③ 服务器主动推送
   ├── 消息3 ────────▶│  ④ 客户端发送
   │◀─ 消息4 ─────────┤  ⑤ 服务器主动推送
```

**优势：** 双方随时发送、服务器主动推送、消息头部极小

### WebSocket 握手

**握手是 HTTP 协议的升级：**

```http
客户端请求：
GET /chat HTTP/1.1
Host: server.example.com
Upgrade: websocket              ← 请求升级
Connection: Upgrade
Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==
Sec-WebSocket-Version: 13

服务器响应：
HTTP/1.1 101 Switching Protocols  ← 101 表示协议切换
Upgrade: websocket
Connection: Upgrade
Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=
```

**Sec-WebSocket-Accept 计算：**
```
accept = base64(sha1(key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"))
```

### WebSocket 帧格式

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
+-------------------------------- - - - - - - - - - - - - - - - +
```

**关键字段：**
| 字段 | 说明 |
|:---|:---|
| `FIN` | 是否是最后一帧 |
| `opcode` | 0x1=文本, 0x2=二进制, 0x8=关闭 |
| `MASK` | 客户端必须掩码，服务器不掩码 |
| `Payload len` | 数据长度 |
| `Masking-key` | 4字节掩码（仅客户端发送时） |

**掩码解码：** `decoded[i] = encoded[i] XOR mask[i % 4]`

---

## 核心代码

### Session 状态机

```cpp
class Session : public enable_shared_from_this<Session> {
    enum class State {
        HTTP,       // 等待 HTTP 握手请求
        WEBSOCKET,  // WebSocket 通信中
        CLOSED
    };
    
    State state_ = State::HTTP;
    // ...
};
```

### 握手处理

```cpp
void do_http_read() {
    auto self = shared_from_this();
    socket_.async_read_some(buffer(buffer_),
        [this, self](error_code ec, size_t len) {
            if (!ec) {
                string request(buffer_.data(), len);
                
                if (is_websocket_upgrade(request)) {
                    do_websocket_handshake(request);
                } else {
                    handle_http_request(request);
                }
            }
        }
    );
}

bool is_websocket_upgrade(const string& req) {
    return req.find("Upgrade: websocket") != string::npos;
}

void do_websocket_handshake(const string& request) {
    string key = extract_ws_key(request);
    string accept = compute_ws_accept(key);
    
    string response =
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: " + accept + "\r\n"
        "\r\n";
    
    auto self = shared_from_this();
    async_write(socket_, buffer(response),
        [this, self](error_code ec, size_t) {
            if (!ec) {
                state_ = State::WEBSOCKET;
                do_ws_read();  // 开始 WebSocket 通信
            }
        }
    );
}
```

### 帧解析

```cpp
void handle_ws_frame(uint8_t* data, size_t len) {
    if (len < 2) return;
    
    uint8_t byte1 = data[0];
    uint8_t byte2 = data[1];
    
    bool fin = (byte1 & 0x80) != 0;
    uint8_t opcode = byte1 & 0x0F;
    bool masked = (byte2 & 0x80) != 0;
    uint64_t payload_len = byte2 & 0x7F;
    
    size_t pos = 2;
    
    // 处理扩展长度
    if (payload_len == 126) {
        payload_len = (data[2] << 8) | data[3];
        pos = 4;
    } else if (payload_len == 127) {
        // 64位长度处理...
        pos = 10;
    }
    
    // 读取掩码
    uint8_t mask[4] = {0};
    if (masked) {
        memcpy(mask, data + pos, 4);
        pos += 4;
    }
    
    // 解码 payload
    string payload;
    for (size_t i = 0; i < payload_len; i++) {
        payload += data[pos + i] ^ mask[i % 4];
    }
    
    // 处理 opcode
    switch (opcode) {
        case 0x01: on_text_message(payload); break;  // 文本帧
        case 0x08: close(); break;                   // 关闭帧
    }
}
```

### 帧发送

```cpp
void ws_write(const string& message) {
    vector<uint8_t> frame;
    frame.push_back(0x81);  // FIN=1, opcode=text(0x1)
    
    // payload 长度（服务器发送不掩码）
    if (message.size() < 126) {
        frame.push_back(message.size());
    } else if (message.size() < 65536) {
        frame.push_back(126);
        frame.push_back((message.size() >> 8) & 0xFF);
        frame.push_back(message.size() & 0xFF);
    }
    
    // payload 数据
    frame.insert(frame.end(), message.begin(), message.end());
    
    // 异步发送
    auto self = shared_from_this();
    async_write(socket_, buffer(frame),
        [this, self](error_code, size_t) {}
    );
}
```

### 广播系统

```cpp
class SessionManager {
public:
    void add(shared_ptr<Session> session) {
        sessions_.insert(session);
    }
    
    void remove(shared_ptr<Session> session) {
        sessions_.erase(session);
    }
    
    void broadcast(const string& message, 
                   shared_ptr<Session> exclude = nullptr) {
        for (auto& session : sessions_) {
            if (session != exclude) {
                session->send(message);
            }
        }
    }

private:
    set<shared_ptr<Session>> sessions_;
};
```

---

## 架构图

### 通信流程

```
┌─────────────────────────────────────────────────────────────┐
│                    WebSocket Connection                      │
│                     (全双工通信)                              │
├─────────────────────────────────────────────────────────────┤
│  客户端          服务器              客户端          服务器  │
│    │               │                  │               │     │
│    ├── 消息1 ─────▶│                  │◀─ 消息2 ───────┤     │
│    │               │                  │               │     │
│    ├── 消息3 ─────▶│                  │◀─ 消息4 ───────┤     │
│    │               ├── 广播给所有人 ─▶│               │     │
│    │               │                  │               │     │
│    ◀─ 消息5 ───────┤                  ├── 消息6 ──────▶│     │
└─────────────────────────────────────────────────────────────┘
```

### 状态转换

```
        ┌─────────────┐
        │    Start    │
        └──────┬──────┘
               │
               ▼
        ┌─────────────┐
        │ HTTP Read   │◀────────────────┐
        └──────┬──────┘                 │
               │                        │
      ┌────────┴────────┐               │
      │                 │               │
   升级请求            普通请求         │
      │                 │               │
      ▼                 ▼               │
┌──────────┐    ┌──────────────┐        │
│Handshake │    │HTTP Response │        │
└────┬─────┘    └──────┬───────┘        │
     │                 │                │
     ▼                 │                │
┌──────────┐           │                │
│WS Read   │───────────┘                │
└────┬─────┘                            │
     │                                   │
     ▼                                   │
┌──────────┐                            │
│Handle    │                            │
│Message   │                            │
└────┬─────┘                            │
     │                                   │
     ▼                                   │
┌──────────┐                            │
│Broadcast │────────────────────────────┘
└──────────┘
```

---

## CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.14)
project(nuclaw_step04 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
find_package(Boost REQUIRED COMPONENTS system)
find_package(OpenSSL REQUIRED)  # 用于 SHA1 计算

include(FetchContent)
FetchContent_Declare(json GIT_REPOSITORY ...)
FetchContent_MakeAvailable(json)

add_executable(nuclaw_step04 main.cpp)
target_link_libraries(nuclaw_step04 
    Boost::system
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
)
```

---

## 编译运行

```bash
cd src/step04
mkdir build && cd build
cmake .. && make -j4
./nuclaw_step04
```

测试：
```bash
# 安装 wscat
npm install -g wscat

# 连接 WebSocket
wscat -c ws://localhost:8080

# 发送消息
> {"type":"chat","content":"你好"}

# 收到广播（开多个终端测试）
< {"content":"你好","timestamp":1234567890,"type":"message"}
```

---

## 本章总结

- ✅ 服务器可以主动推送消息
- ✅ WebSocket 帧格式解析
- ✅ 消息广播系统

---

## 课后思考

我们的服务器现在可以实时通信了，但如果用户问：

```
用户: 今天北京天气怎么样？
```

服务器只能转发消息，无法智能回答。

**方案 1：** 规则匹配 `if (包含"天气") return "晴天"` —— 硬编码，数据不真实

**方案 2：** 调用外部天气 API —— 需要理解用户意图（不只是匹配关键词）

如何让 Agent 真正"智能"地回答问题？

<details>
<summary>点击查看下一章 💡</summary>

**Step 5: Agent 核心 — LLM 接入与工具调用**

我们将学习：
- LLM API 接入（OpenAI/Claude）
- Function Calling（工具调用）
- Agent Loop：理解 → 决策 → 执行

</details>
