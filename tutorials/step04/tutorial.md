# Step 4: WebSocket 实时通信

> 目标：掌握 WebSocket 协议，实现全双工实时通信
> 
> 难度：⭐⭐⭐ | 代码量：约 350 行 | 预计学习时间：3-4 小时

---

## 一、问题引入

### 1.1 Step 3 的问题

HTTP 是**请求-响应**模式，服务器无法主动推送消息：

```
客户端              服务器
   │                  │
   ├── 请求1 ────────▶│
   │◀─ 响应1 ─────────┤
   │                  │
   │                  │ ← 服务器有新消息，但无法主动发送！
   │                  │
   ├── 轮询 ────────▶ │ "有新消息吗？"
   │◀─ 没有 ──────────┤
   │                  │
   ├── 轮询 ────────▶ │ "有新消息吗？"
   │◀─ 有了！ ────────┤ ← 延迟取决于轮询间隔
```

**轮询的问题：**
- 浪费带宽（大量空请求）
- 高延迟（最坏情况下延迟 = 轮询间隔）
- 服务器压力大

**真实场景：** 聊天应用、股票行情、游戏同步都需要服务器主动推送。

---

## 二、WebSocket 协议详解

### 2.1 什么是 WebSocket？

WebSocket 提供**全双工**通信通道：

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

**优势：**
- 建立后，**双方随时可以发送消息**
- 服务器可以**主动推送**
- 消息头部极小（2-14字节，vs HTTP 几百字节）

### 2.2 WebSocket 握手详解

**握手是 HTTP 协议的升级：**

```http
客户端请求：
GET /chat HTTP/1.1
Host: server.example.com
Upgrade: websocket              ← 请求升级
Connection: Upgrade
Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==  ← Base64 随机密钥
Sec-WebSocket-Version: 13

服务器响应：
HTTP/1.1 101 Switching Protocols  ← 101 状态码表示协议切换
Upgrade: websocket
Connection: Upgrade
Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=  ← 计算后的密钥
```

**Sec-WebSocket-Accept 计算：**
```
accept = base64(sha1(key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"))
```

**GUID 常量：** "258EAFA5-E914-47DA-95CA-C5AB0DC85B11" 是 RFC 6455 规定的固定值。

### 2.3 WebSocket 帧格式

WebSocket 使用**帧**（Frame）传输数据：

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
:                     Payload Data continued ...                :
+ - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
|                     Payload Data continued ...                |
+---------------------------------------------------------------+
```

**关键字段：**
| 字段 | 位 | 说明 |
|:---|:---|:---|
| `FIN` | 1 bit | 是否是最后一帧（1=是，0=后面还有） |
| `RSV1-3` | 3 bits | 保留位，必须为 0 |
| `opcode` | 4 bits | 帧类型 |
| `MASK` | 1 bit | 是否掩码（客户端必须设1，服务器必须设0） |
| `Payload len` | 7 bits | 数据长度编码 |
| `Masking-key` | 32 bits | 4字节掩码（仅MASK=1时存在） |

**Opcode 定义：**
| 值 | 含义 |
|:---|:---|
| 0x0 | 继续帧（Continuation Frame） |
| 0x1 | 文本帧（Text Frame） |
| 0x2 | 二进制帧（Binary Frame） |
| 0x8 | 关闭连接（Close） |
| 0x9 | Ping |
| 0xA | Pong |

**Payload Length 编码：**
| 值 | 含义 |
|:---|:---|
| 0-125 | 直接表示长度 |
| 126 | 后面2字节表示长度（16位无符号整数） |
| 127 | 后面8字节表示长度（64位无符号整数） |

### 2.4 掩码机制

**为什么需要掩码？**

防止客户端发送恶意构造的帧被中间代理误解释为 HTTP 请求（缓存污染攻击）。

**掩码规则：**
- 客户端发送给服务器的帧**必须**掩码（MASK=1）
- 服务器发送给客户端的帧**不能**掩码（MASK=0）

**掩码解码：**
```
decoded[i] = encoded[i] XOR mask[i % 4]
```

---

## 三、代码结构详解

### 3.1 Step 3 vs Step 4 架构对比

**Step 3 架构（HTTP）：**
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
│   JSON Parser   │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│     Router      │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│   Handler       │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│   JSON Response │
└─────────────────┘
```

**Step 4 架构（WebSocket）：**
```
Client Request
     │
     ▼
┌─────────────────┐
│  HTTP Upgrade   │  ← 判断是否为 WebSocket
└────────┬────────┘
         │
    ┌────┴────┐
    │         │
普通 HTTP   WebSocket
    │         │
    ▼         ▼
┌───────┐  ┌─────────────────┐
│ HTTP  │  │ WebSocket Frame │
│处理   │  │ Parser          │
└───────┘  └────────┬────────┘
                    │
                    ▼
            ┌─────────────────┐
            │   Frame Handler │
            │   • Text        │
            │   • Binary      │
            │   • Close       │
            └────────┬────────┘
                     │
                     ▼
            ┌─────────────────┐
            │    Broadcast    │  ← 服务器主动推送
            └─────────────────┘
```

### 3.2 Session 状态机

```cpp
class Session : public std::enable_shared_from_this<Session> {
    enum class State {
        HTTP,       // 等待 HTTP 握手请求
        WEBSOCKET,  // WebSocket 通信中
        CLOSED
    };
    
    State state_ = State::HTTP;
    // ...
};
```

### 3.3 HTTP 升级检测

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
    return req.find("Upgrade: websocket") != string::npos ||
           req.find("upgrade: websocket") != string::npos;
}
```

### 3.4 WebSocket 握手

```cpp
void do_websocket_handshake(const string& request) {
    // 提取 Sec-WebSocket-Key
    string key = extract_ws_key(request);
    
    // 计算 Sec-WebSocket-Accept
    string accept = compute_ws_accept(key);
    
    // 发送 101 响应
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

string extract_ws_key(const string& request) {
    auto pos = request.find("Sec-WebSocket-Key: ");
    if (pos != string::npos) {
        pos += 19;
        auto end = request.find("\r\n", pos);
        return request.substr(pos, end - pos);
    }
    return "";
}

string compute_ws_accept(const string& key) {
    string magic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    string combined = key + magic;
    
    // SHA1 计算
    unsigned char hash[20];
    SHA1(reinterpret_cast<const unsigned char*>(combined.data()), 
         combined.size(), hash);
    
    // Base64 编码
    return base64_encode(hash, 20);
}
```

### 3.5 WebSocket 帧解析

```cpp
void handle_ws_frame(uint8_t* data, size_t len) {
    if (len < 2) return;
    
    // 解析帧头
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
        // 64位长度处理（省略）
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
    payload.reserve(payload_len);
    for (size_t i = 0; i < payload_len; i++) {
        payload += data[pos + i] ^ mask[i % 4];
    }
    
    // 处理 opcode
    switch (opcode) {
        case 0x01: on_text_message(payload); break;
        case 0x08: close(); break;
        case 0x09: send_pong(); break;
    }
}
```

### 3.6 WebSocket 帧发送

```cpp
void ws_write(const string& message) {
    vector<uint8_t> frame;
    
    // FIN=1, opcode=text(0x1)
    frame.push_back(0x81);
    
    // payload 长度（服务器发送不掩码）
    if (message.size() < 126) {
        frame.push_back(message.size());
    } else if (message.size() < 65536) {
        frame.push_back(126);
        frame.push_back((message.size() >> 8) & 0xFF);
        frame.push_back(message.size() & 0xFF);
    } else {
        frame.push_back(127);
        // 64位长度（省略）
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

### 3.7 广播系统

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

### 3.8 CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.14)
project(nuclaw_step04 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost REQUIRED COMPONENTS system)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)  # 用于 SHA1 计算

include(FetchContent)
FetchContent_Declare(json ...)
FetchContent_MakeAvailable(json)

add_executable(nuclaw_step04 main.cpp)
target_link_libraries(nuclaw_step04 
    Boost::system
    Threads::Threads
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
)
```

---

## 四、编译运行

### 4.1 编译

```bash
cd src/step04
mkdir build && cd build
cmake .. && make -j4
./nuclaw_step04
```

### 4.2 测试

```bash
# 安装 wscat
npm install -g wscat

# 连接 WebSocket
wscat -c ws://localhost:8080

# 发送消息
> {"type":"chat","content":"你好"}

# 收到广播
< {"content":"你好","timestamp":1234567890,"type":"message"}

# 开多个终端测试广播功能
```

---

## 五、本章总结

- ✅ 解决了 Step 3 的服务器无法主动推送问题
- ✅ 掌握 WebSocket 协议和握手过程
- ✅ 理解 WebSocket 帧格式
- ✅ 实现消息广播功能
- ✅ 代码从 250 行扩展到 350 行

---

## 六、课后思考

我们的服务器现在可以实时通信了，但它只能做简单的消息转发：

```
用户: {"type":"chat","content":"你好"}
服务器: 广播给所有人
```

如果用户问：

```
用户: 今天北京天气怎么样？
```

服务器应该如何回答？

**方案 1：规则匹配**（Step 3 用过的）
```cpp
if (message.find("天气") != string::npos) {
    return "晴天，25°C";  // 硬编码，数据不真实
}
```

**方案 2：调用外部 API**
- 需要 HTTP 客户端调用天气服务
- 需要理解用户意图（不只是匹配关键词）
- 需要整合 API 结果生成自然语言回复

如何让 Agent 真正"智能"地回答问题？

<details>
<summary>点击查看下一章 💡</summary>

**Step 5: Agent 核心 — LLM 接入与工具调用**

我们将学习：
- LLM API 接入（OpenAI/Claude）
- Function Calling（工具调用）
- Agent Loop：理解 → 决策 → 执行
- 让 AI 真正理解用户意图并调用工具获取数据！

</details>
