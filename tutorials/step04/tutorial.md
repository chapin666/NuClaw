# Step 4: WebSocket 实时通信 —— Agent 的实时对话能力

> 目标：实现 WebSocket 协议，支持全双工通信和对话上下文管理
> 
003e 难度：⭐⭐⭐ | 代码量：约 400 行 | 预计学习时间：3-4 小时

---

## 一、为什么需要 WebSocket？

### 1.1 HTTP 的局限

Step 3 的规则 AI 使用 HTTP 请求-响应模式：

```
HTTP 通信模式：
┌─────────┐              ┌─────────┐
│  客户端  │──────────────▶│  服务器  │
│         │   请求         │         │
│   等待   │◀──────────────│  处理中  │
│         │   响应         │         │
└─────────┘              └─────────┘

问题：
1. 服务器无法主动推送（只能被动响应）
2. 每次请求都要建立新连接（或复用 Keep-Alive，但有头部开销）
3. 无法实时显示处理进度
```

**实际场景需求：**

```
用户: 写一段Python快速排序代码

期望的交互：
Agent: 好的，正在为您生成代码...
Agent: [==========>        ] 50%
Agent: [================>  ] 80%
Agent: [===================] 完成！
Agent: ```python
Agent: def quicksort(arr):
Agent:     ...
Agent: ```

HTTP 的限制：
- 用户必须等到全部代码生成完成才能看到
- 无法显示"正在生成..."的进度
- 无法实现打字机效果
```

### 1.2 WebSocket 的优势

```
WebSocket 通信模式：

建立连接（HTTP Upgrade）
┌─────────┐    GET /ws HTTP/1.1     ┌─────────┐
│         │ ── Connection: Upgrade ─▶│         │
│  客户端  │    Upgrade: websocket   │  服务器  │
│         │◀── 101 Switching Protocol│         │
└────┬────┘                        └────┬────┘
     │                                  │
     ═══════════════════════════════════  ← WebSocket 连接建立
     │                                  │
     │  双向通信（任何时候都可以发送）      │
     │  ─────────────────────────────▶   │
     │  ◀─────────────────────────────   │
     │                                  │
```

**WebSocket 特点：**

| 特性 | HTTP | WebSocket |
|:---|:---|:---|
| 通信模式 | 请求-响应 | 全双工 |
| 服务器推送 | ❌ 不支持 | ✅ 支持 |
| 连接开销 | 每次请求有头部 | 首次握手后无头部 |
| 实时性 | 差 | 好 |
| 适用场景 | 静态资源、API | 实时聊天、游戏、推送 |

---

## 二、WebSocket 协议详解

### 2.1 握手过程

WebSocket 连接通过 HTTP Upgrade 建立：

**客户端请求：**
```http
GET /chat HTTP/1.1
Host: localhost:8080
Upgrade: websocket
Connection: Upgrade
Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==
Sec-WebSocket-Version: 13
```

**服务器响应：**
```http
HTTP/1.1 101 Switching Protocols
Upgrade: websocket
Connection: Upgrade
Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=
```

**`Sec-WebSocket-Accept` 计算：**
```
1. 取客户端的 Sec-WebSocket-Key
2. 拼接固定字符串 "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
3. 计算 SHA-1 哈希
4. Base64 编码

示例：
Key: dGhlIHNhbXBsZSBub25jZQ==
+ magic: 258EAFA5-E914-47DA-95CA-C5AB0DC85B11
= dGhlIHNhbXBsZSBub25jZQ==258EAFA5-E914-47DA-95CA-C5AB0DC85B11
SHA-1: 0x1b3e4b5c...
Base64: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=
```

### 2.2 帧格式

WebSocket 数据以**帧（Frame）**为单位传输：

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

**关键字段：**

| 字段 | 说明 |
|:---|:---|
| **FIN** | 是否是最后一帧（1=是，0=后面还有）|
| **opcode** | 帧类型：0x1=文本，0x2=二进制，0x8=关闭，0x9=ping，0xA=pong |
| **MASK** | 客户端发送必须掩码（1），服务器发送不掩码（0）|
| **Payload len** | 数据长度：0-125直接表示，126=后面2字节是长度，127=后面8字节 |
| **Masking-key** | 4字节随机密钥（仅客户端发送时有）|
| **Payload Data** | 实际数据（客户端发送时已用 Masking-key 异或）|

**掩码算法（客户端）：**
```cpp
// 客户端发送前必须掩码数据
for (size_t i = 0; i < payload_len; i++) {
    payload[i] ^= masking_key[i % 4];
}
```

**为什么需要掩码？** 防止代理缓存污染攻击（Cache Poisoning）。

---

## 三、WebSocket 服务器实现

### 3.1 握手处理

```cpp
class WebSocketSession : public std::enable_shared_from_this<WebSocketSession> {
public:
    WebSocketSession(tcp::socket socket)
        : socket_(std::move(socket)) {}
    
    void start() {
        // 第一步：处理 HTTP Upgrade 请求
        do_read_handshake();
    }

private:
    void do_read_handshake() {
        auto self = shared_from_this();
        
        socket_.async_read_some(
            asio::buffer(buffer_),
            [this, self](error_code ec, size_t length) {
                if (ec) return;
                
                // 解析 HTTP 请求
                std::string request(buffer_.data(), length);
                
                if (is_websocket_upgrade(request)) {
                    // 提取 Sec-WebSocket-Key
                    std::string key = extract_key(request);
                    
                    // 计算响应
                    std::string response = build_handshake_response(key);
                    
                    // 发送 101 响应
                    do_write_handshake(response);
                }
            }
        );
    }
    
    bool is_websocket_upgrade(const std::string& request) {
        return request.find("Upgrade: websocket") != std::string::npos;
    }
    
    std::string extract_key(const std::string& request) {
        size_t pos = request.find("Sec-WebSocket-Key: ");
        if (pos == std::string::npos) return "";
        
        size_t start = pos + 19;  // "Sec-WebSocket-Key: " 长度
        size_t end = request.find("\r\n", start);
        return request.substr(start, end - start);
    }
    
    std::string build_handshake_response(const std::string& key) {
        // WebSocket Magic String
        const std::string magic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
        std::string combined = key + magic;
        
        // 计算 SHA-1
        unsigned char hash[SHA_DIGEST_LENGTH];
        SHA1(reinterpret_cast<const unsigned char*>(combined.c_str()), 
             combined.length(), hash);
        
        // Base64 编码
        std::string accept = base64_encode(hash, SHA_DIGEST_LENGTH);
        
        return "HTTP/1.1 101 Switching Protocols\r\n"
               "Upgrade: websocket\r\n"
               "Connection: Upgrade\r\n"
               "Sec-WebSocket-Accept: " + accept + "\r\n"
               "\r\n";
    }

    tcp::socket socket_;
    std::array<char, 8192> buffer_;
};
```

### 3.2 帧解析与生成

```cpp
struct WebSocketFrame {
    bool fin;           // 是否是最后一帧
    uint8_t opcode;     // 操作码
    bool masked;        // 是否掩码
    uint64_t length;    // 数据长度
    std::array<uint8_t, 4> masking_key;  // 掩码密钥
    std::string payload; // 数据内容
};

class WebSocketCodec {
public:
    // 解析帧，返回 true 表示解析成功
    bool parse_frame(const uint8_t* data, size_t len, 
                     WebSocketFrame& frame, size_t& consumed);
    
    // 生成帧（服务器发送，不掩码）
    std::vector<uint8_t> build_frame(const std::string& payload, 
                                     uint8_t opcode = 0x01);

private:
    std::vector<uint8_t> buffer_;  // 未解析完的残留数据
};

bool WebSocketCodec::parse_frame(const uint8_t* data, size_t len,
                                  WebSocketFrame& frame, size_t& consumed) {
    if (len < 2) return false;  // 至少需要 2 字节
    
    size_t pos = 0;
    
    // 第一个字节：FIN + RSV + opcode
    frame.fin = (data[pos] & 0x80) != 0;
    frame.opcode = data[pos] & 0x0F;
    pos++;
    
    // 第二个字节：MASK + Payload length
    frame.masked = (data[pos] & 0x80) != 0;
    uint8_t payload_len = data[pos] & 0x7F;
    pos++;
    
    // 解析 Payload length
    if (payload_len < 126) {
        frame.length = payload_len;
    } else if (payload_len == 126) {
        if (len < pos + 2) return false;
        frame.length = (data[pos] << 8) | data[pos + 1];
        pos += 2;
    } else {  // payload_len == 127
        if (len < pos + 8) return false;
        frame.length = 0;
        for (int i = 0; i < 8; i++) {
            frame.length = (frame.length << 8) | data[pos + i];
        }
        pos += 8;
    }
    
    // 解析 Masking key
    if (frame.masked) {
        if (len < pos + 4) return false;
        std::copy(data + pos, data + pos + 4, frame.masking_key.begin());
        pos += 4;
    }
    
    // 检查数据是否完整
    if (len < pos + frame.length) return false;
    
    // 提取 Payload
    frame.payload.assign(reinterpret_cast<const char*>(data + pos), 
                         frame.length);
    
    // 如果掩码，解码数据
    if (frame.masked) {
        for (size_t i = 0; i < frame.length; i++) {
            frame.payload[i] ^= frame.masking_key[i % 4];
        }
    }
    
    consumed = pos + frame.length;
    return true;
}

std::vector<uint8_t> WebSocketCodec::build_frame(const std::string& payload,
                                                 uint8_t opcode) {
    std::vector<uint8_t> frame;
    
    // 第一个字节：FIN=1, opcode
    frame.push_back(0x80 | opcode);
    
    // Payload length
    size_t len = payload.length();
    if (len < 126) {
        frame.push_back(static_cast<uint8_t>(len));
    } else if (len < 65536) {
        frame.push_back(126);
        frame.push_back((len >> 8) & 0xFF);
        frame.push_back(len & 0xFF);
    } else {
        frame.push_back(127);
        for (int i = 7; i >= 0; i--) {
            frame.push_back((len >> (i * 8)) & 0xFF);
        }
    }
    
    // 服务器发送不掩码，直接附加数据
    frame.insert(frame.end(), payload.begin(), payload.end());
    
    return frame;
}
```

### 3.3 完整 Session 实现

```cpp
class WebSocketSession : public std::enable_shared_from_this<WebSocketSession> {
public:
    WebSocketSession(tcp::socket socket, RuleEngine& engine)
        : socket_(std::move(socket)),
          engine_(engine) {}
    
    void start() {
        do_read_handshake();
    }
    
    // 服务器主动推送消息
    void send(const std::string& message) {
        auto frame = codec_.build_frame(message, 0x01);
        
        auto self = shared_from_this();
        asio::async_write(
            socket_,
            asio::buffer(frame),
            [this, self](error_code ec, size_t) {
                if (ec) {
                    std::cerr << "Send error: " << ec.message() << "\n";
                }
            }
        );
    }

private:
    // 握手后进入 WebSocket 模式
    void do_read_websocket() {
        auto self = shared_from_this();
        
        socket_.async_read_some(
            asio::buffer(buffer_),
            [this, self](error_code ec, size_t length) {
                if (ec) {
                    handle_disconnect();
                    return;
                }
                
                // 解析 WebSocket 帧
                WebSocketFrame frame;
                size_t consumed = 0;
                
                if (codec_.parse_frame(
                    reinterpret_cast<const uint8_t*>(buffer_.data()), 
                    length, frame, consumed)) {
                    
                    handle_frame(frame);
                }
                
                // 继续读取
                do_read_websocket();
            }
        );
    }
    
    void handle_frame(const WebSocketFrame& frame) {
        switch (frame.opcode) {
            case 0x01:  // 文本帧
                handle_text_message(frame.payload);
                break;
                
            case 0x08:  // 关闭帧
                handle_close();
                break;
                
            case 0x09:  // Ping
                send_pong();
                break;
                
            case 0x0A:  // Pong
                // 忽略，用于保活检测
                break;
                
            default:
                std::cerr << "Unknown opcode: " << (int)frame.opcode << "\n";
        }
    }
    
    void handle_text_message(const std::string& message) {
        // 使用规则引擎处理消息
        IntentResult result = engine_.recognize(message);
        
        std::string reply;
        if (result.confidence > 0.5f) {
            auto intent = engine_.find_intent(result.intent);
            if (intent) {
                reply = intent->handler(result.entities);
            }
        } else {
            reply = "抱歉，我不太理解。您可以问天气、时间等。";
        }
        
        // 发送回复
        send(reply);
    }
    
    void send_pong() {
        auto frame = codec_.build_frame("", 0x0A);
        asio::async_write(socket_, asio::buffer(frame), 
                         [](error_code, size_t) {});
    }

    tcp::socket socket_;
    RuleEngine& engine_;
    WebSocketCodec codec_;
    std::array<char, 8192> buffer_;
    
    // 对话上下文
    std::vector<std::pair<std::string, std::string>> conversation_history_;
};
```

---

## 四、对话上下文管理

### 4.1 上下文的价值

```
无上下文：                              有上下文：
────────────────────────────────────────────────────────────────
用户: 你好，我叫小明                    用户: 你好，我叫小明
Agent: 你好小明！                       Agent: 你好小明！
                                        （保存：用户姓名=小明）
用户: 我叫什么？                        用户: 我叫什么？
Agent: 抱歉，我不知道                   Agent: 你叫小明呀！
                                        （查询上下文找到姓名）
                                        
用户: 今天天气怎样？                    用户: 今天天气怎样？
Agent: 今天北京天气晴朗                  Agent: 今天北京天气晴朗
                                        （保存：当前话题=天气）
用户: 明天呢？                          用户: 明天呢？
Agent: 明天什么？                       Agent: 明天北京多云，有雨
                                        （继承话题：天气）
```

### 4.2 简单上下文实现

```cpp
class ConversationContext {
public:
    void add_message(const std::string& role, const std::string& content) {
        history_.push_back({role, content});
        
        // 限制历史长度，防止无限增长
        if (history_.size() > MAX_HISTORY_SIZE) {
            history_.erase(history_.begin());
        }
    }
    
    void set_slot(const std::string& key, const std::string& value) {
        slots_[key] = value;
    }
    
    std::string get_slot(const std::string& key) const {
        auto it = slots_.find(key);
        return it != slots_.end() ? it->second : "";
    }
    
    const std::vector<Message>& get_history() const {
        return history_;
    }

private:
    struct Message {
        std::string role;     // user / assistant / system
        std::string content;
    };
    
    std::vector<Message> history_;
    std::map<std::string, std::string> slots_;  // 关键信息槽位
    
    static constexpr size_t MAX_HISTORY_SIZE = 20;
};

// 在 Agent 中使用上下文
class AgentSession {
    // ...
    
    void handle_text_message(const std::string& message) {
        // 保存用户消息
        context_.add_message("user", message);
        
        // 意图识别（可以结合上下文）
        IntentResult result = engine_.recognize(message, context_);
        
        // 生成回复
        std::string reply = generate_reply(result, context_);
        
        // 保存助手回复
        context_.add_message("assistant", reply);
        
        // 发送
        send(reply);
    }
};
```

---

## 五、本章小结

**核心收获：**

1. **WebSocket 协议**：
   - HTTP Upgrade 握手
   - 帧格式解析
   - 全双工通信机制

2. **实时通信能力**：
   - 服务器主动推送
   - 流式数据发送
   - Ping/Pong 保活

3. **上下文管理**：
   - 对话历史存储
   - 关键信息槽位
   - 多轮对话支持

---

## 六、引出的问题

### 6.1 智能问题

规则 AI 还是太死板：

```
用户: "我想去一个不太热、有海的地方"

规则 AI: 无法匹配任何关键词

期望的 Agent:
- 理解"不太热" = 温度适中/凉爽
- 理解"有海" = 海滨城市
- 推理：推荐青岛、大连、厦门等
```

**问题：** 如何让 Agent 真正"理解"语义，而不只是匹配关键词？

### 6.2 工具调用问题

目前的回复都是预设的：

```cpp
.handler = [](const std::map<std::string, std::string>& entities) {
    return "今天北京天气晴朗";  // 假数据！
};
```

**实际应该：**
- 调用真实的天气 API 获取数据
- 根据 API 结果生成回复
- 处理 API 错误、超时等情况

---

**下一章预告（Step 5）：**

我们将接入 **LLM（大语言模型）**：
- 使用 GPT-4/Claude 等大模型理解语义
- 实现 **Function Calling**：让 LLM 能调用外部工具
- 构建完整的 **Agent Loop**：理解 → 决策 → 执行 → 回复

WebSocket 提供了实时通信通道，上下文管理支持了多轮对话，接下来要让 Agent 拥有真正的"大脑"。
