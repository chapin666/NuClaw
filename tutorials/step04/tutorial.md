# Step 4: WebSocket 实时通信

> 目标：掌握 WebSocket 协议，实现全双工实时通信
> 
> 难度：⭐⭐⭐ (中等)
> 
> 代码量：约 350 行（较 Step 3 新增约 100 行）
> > 预计学习时间：3-4 小时

---

## 问题引入

**Step 3 的问题：**

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
   │                  │
   ├── 轮询 ────────▶ │ "有新消息吗？"
```

**轮询的问题：**
- 浪费带宽（大量空请求）
- 高延迟（最坏情况下延迟 = 轮询间隔）
- 服务器压力大

**真实场景：** 聊天应用、股票行情、游戏同步都需要服务器主动推送。

---

## 解决方案

### WebSocket 协议

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
   │                  │
   │  (双向随时发送)  │
```

**优势：**
- 建立后，**双方随时可以发送消息**
- 服务器可以**主动推送**
- 消息头部极小（2-14字节，vs HTTP 几百字节）

### WebSocket 握手详解

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

### WebSocket 帧格式

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
| 字段 | 说明 |
|:---|:---|
| `FIN` | 是否是最后一帧 |
| `opcode` | 帧类型：0x1=文本, 0x2=二进制, 0x8=关闭 |
| `MASK` | 客户端发送必须掩码，服务器发送不掩码 |
| `Payload len` | 数据长度：0-125直接表示，126=16位扩展，127=64位扩展 |
| `Masking-key` | 4字节掩码（仅客户端发送时） |
| `Payload Data` | 实际数据（客户端发送时需要 XOR 解码） |

**掩码解码：**
```
decoded[i] = encoded[i] XOR mask[i % 4]
```

---

## 代码对比

### Step 3 vs Step 4 架构对比

**Step 3 架构（HTTP）：**
```
┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│   Request   │────▶│   Process   │────▶│  Response   │
│   (JSON)    │     │             │     │   (JSON)    │
└─────────────┘     └─────────────┘     └─────────────┘
       │                                    │
       │        一次请求-响应                │
       └────────────────────────────────────┘
                    连接保持但需轮询
```

**Step 4 架构（WebSocket）：**
```
┌─────────────────────────────────────────────────────────────┐
│                    WebSocket Connection                      │
│                     (全双工通信)                              │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  客户端          服务器              客户端          服务器  │
│    │               │                  │               │     │
│    ├── 消息1 ─────▶│                  │◀─ 消息2 ───────┤     │
│    │               │                  │               │     │
│    ├── 消息3 ─────▶│                  │◀─ 消息4 ───────┤     │
│    │               ├── 广播给所有人 ─▶│               │     │
│    │               │                  │               │     │
│    ◀─ 消息5 ───────┤                  ├── 消息6 ──────▶│     │
│    │               │                  │               │     │
│                                                             │
│  特点：双方随时发送，服务器可主动推送，支持广播              │
└─────────────────────────────────────────────────────────────┘
```

### 代码修改详解

**Step 3 的关键代码：**
```cpp
// HTTP 请求-响应
void process_request(const std::string& raw) {
    auto http_req = parse_http_request(raw);
    auto chat_req = ChatRequest::from_json(extract_body(raw));
    auto chat_res = router_.handle(http_req.path, chat_req);
    do_response(chat_res, http_req.keep_alive);
}
```

**Step 4 的修改：**

```cpp
// 新增：WebSocket 握手和帧处理
class Session : public std::enable_shared_from_this<Session> {
public:
    void start() {
        do_http_read();  // ← 先读 HTTP 握手请求
    }

private:
    // 阶段1：读取 HTTP 请求，判断是否为 WebSocket 升级
    void do_http_read() {
        auto self = shared_from_this();
        socket_.async_read_some(asio::buffer(buffer_),
            [this, self](error_code ec, size_t len) {
                if (!ec) {
                    std::string request(buffer_.data(), len);
                    if (is_websocket_upgrade(request)) {
                        do_websocket_handshake(request);
                    } else {
                        handle_http_request(request);  // 普通 HTTP
                    }
                }
            }
        );
    }
    
    // 检查是否是 WebSocket 升级请求
    bool is_websocket_upgrade(const std::string& req) {
        return req.find("Upgrade: websocket") != std::string::npos;
    }
    
    // 阶段2：执行 WebSocket 握手
    void do_websocket_handshake(const std::string& request) {
        // 提取 Sec-WebSocket-Key
        std::string key = extract_ws_key(request);
        
        // 计算 Sec-WebSocket-Accept
        std::string accept = compute_ws_accept(key);
        
        // 发送 101 响应
        std::string response =
            "HTTP/1.1 101 Switching Protocols\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Accept: " + accept + "\r\n"
            "\r\n";
        
        auto self = shared_from_this();
        asio::async_write(socket_, asio::buffer(response),
            [this, self](error_code ec, size_t) {
                if (!ec) {
                    is_websocket_ = true;
                    do_ws_read();  // ← 开始 WebSocket 通信
                }
            }
        );
    }
    
    // 阶段3：WebSocket 帧读取
    void do_ws_read() {
        auto self = shared_from_this();
        
        // 先读取帧头（2字节）
        ws_buffer_.resize(2);
        asio::async_read(socket_, asio::buffer(ws_buffer_),
            [this, self](error_code ec, size_t) {
                if (!ec) {
                    parse_ws_frame_header();
                }
            }
        );
    }
    
    // 解析 WebSocket 帧
    void parse_ws_frame_header() {
        uint8_t byte1 = ws_buffer_[0];
        uint8_t byte2 = ws_buffer_[1];
        
        bool fin = (byte1 & 0x80) != 0;
        uint8_t opcode = byte1 & 0x0F;
        bool masked = (byte2 & 0x80) != 0;
        uint64_t payload_len = byte2 & 0x7F;
        
        // 处理不同长度
        if (payload_len == 126) {
            // 16位扩展长度
            read_extended_length(2);
        } else if (payload_len == 127) {
            // 64位扩展长度
            read_extended_length(8);
        } else {
            // 直接读取 payload
            read_payload(payload_len, masked);
        }
    }
    
    // 发送 WebSocket 消息
    void ws_write(const std::string& message) {
        // 构造文本帧
        std::vector<uint8_t> frame;
        frame.push_back(0x81);  // FIN=1, opcode=text(0x1)
        
        // payload 长度（服务器发送不掩码）
        if (message.size() < 126) {
            frame.push_back(message.size());  // MASK=0, len=size
        } else if (message.size() < 65536) {
            frame.push_back(126);
            frame.push_back((message.size() >> 8) & 0xFF);
            frame.push_back(message.size() & 0xFF);
        }
        
        // payload 数据
        frame.insert(frame.end(), message.begin(), message.end());
        
        // 异步发送
        auto self = shared_from_this();
        asio::async_write(socket_, asio::buffer(frame),
            [this, self](error_code, size_t) {}
        );
    }

    tcp::socket socket_;
    bool is_websocket_ = false;
    std::vector<uint8_t> ws_buffer_;  // WebSocket 帧缓冲区
    std::array<char, 4096> buffer_;
};
```

---

## 完整源码（简化版）

```cpp
#include <boost/asio.hpp>
#include <nlohmann/json.hpp>
#include <iostream>
#include <sstream>
#include <memory>
#include <vector>
#include <set>
#include <openssl/sha.h>  // 需要 OpenSSL
#include <boost/beast/core/detail/base64.hpp>

namespace asio = boost::asio;
using tcp = asio::ip::tcp;
using json = nlohmann::json;

// 前向声明
class Session;

// 会话管理器
class SessionManager {
public:
    void add(std::shared_ptr<Session> session);
    void remove(std::shared_ptr<Session> session);
    void broadcast(const std::string& message, std::shared_ptr<Session> exclude = nullptr);

private:
    std::set<std::shared_ptr<Session>> sessions_;
};

// WebSocket Session
class Session : public std::enable_shared_from_this<Session> {
public:
    Session(tcp::socket socket, SessionManager& manager)
        : socket_(std::move(socket)), manager_(manager) {}

    void start() {
        manager_.add(shared_from_this());
        do_http_read();
    }
    
    void send(const std::string& message);

private:
    void do_http_read();
    bool is_websocket_upgrade(const std::string& req);
    void do_websocket_handshake(const std::string& request);
    std::string extract_ws_key(const std::string& request);
    std::string compute_ws_accept(const std::string& key);
    void handle_http_request(const std::string& request);
    void do_ws_read();
    void handle_ws_frame(uint8_t* data, size_t len);
    void on_message(const std::string& message);
    void ws_write(const std::string& message);

    tcp::socket socket_;
    SessionManager& manager_;
    bool is_websocket_ = false;
    std::array<char, 4096> buffer_;
};

// SessionManager 实现
void SessionManager::add(std::shared_ptr<Session> session) {
    sessions_.insert(session);
}

void SessionManager::remove(std::shared_ptr<Session> session) {
    sessions_.erase(session);
}

void SessionManager::broadcast(const std::string& message, std::shared_ptr<Session> exclude) {
    for (auto& session : sessions_) {
        if (session != exclude) {
            session->send(message);
        }
    }
}

// Session 实现
void Session::start() {
    manager_.add(shared_from_this());
    do_http_read();
}

void Session::send(const std::string& message) {
    if (is_websocket_) {
        ws_write(message);
    }
}

void Session::do_http_read() {
    auto self = shared_from_this();
    socket_.async_read_some(asio::buffer(buffer_),
        [this, self](boost::system::error_code ec, std::size_t len) {
            if (!ec) {
                std::string request(buffer_.data(), len);
                if (is_websocket_upgrade(request)) {
                    do_websocket_handshake(request);
                } else {
                    handle_http_request(request);
                }
            }
        }
    );
}

bool Session::is_websocket_upgrade(const std::string& req) {
    return req.find("Upgrade: websocket") != std::string::npos ||
           req.find("upgrade: websocket") != std::string::npos;
}

std::string Session::extract_ws_key(const std::string& request) {
    auto pos = request.find("Sec-WebSocket-Key: ");
    if (pos != std::string::npos) {
        pos += 19;
        auto end = request.find("\r\n", pos);
        return request.substr(pos, end - pos);
    }
    return "";
}

std::string Session::compute_ws_accept(const std::string& key) {
    std::string magic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    std::string combined = key + magic;
    
    unsigned char hash[20];
    SHA1(reinterpret_cast<const unsigned char*>(combined.data()), 
         combined.size(), hash);
    
    return boost::beast::detail::base64_encode(hash, 20);
}

void Session::do_websocket_handshake(const std::string& request) {
    std::string key = extract_ws_key(request);
    std::string accept = compute_ws_accept(key);
    
    std::string response =
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: " + accept + "\r\n"
        "\r\n";
    
    auto self = shared_from_this();
    asio::async_write(socket_, asio::buffer(response),
        [this, self](boost::system::error_code ec, std::size_t) {
            if (!ec) {
                is_websocket_ = true;
                do_ws_read();
            }
        }
    );
}

void Session::handle_http_request(const std::string& request) {
    std::string body = "Please connect via WebSocket";
    std::string response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: " + std::to_string(body.size()) + "\r\n"
        "\r\n" + body;
    
    auto self = shared_from_this();
    asio::async_write(socket_, asio::buffer(response),
        [this, self](boost::system::error_code, std::size_t) {
            manager_.remove(shared_from_this());
        }
    );
}

void Session::do_ws_read() {
    auto self = shared_from_this();
    socket_.async_read_some(asio::buffer(buffer_),
        [this, self](boost::system::error_code ec, std::size_t len) {
            if (!ec) {
                handle_ws_frame(reinterpret_cast<uint8_t*>(buffer_.data()), len);
                do_ws_read();
            } else {
                manager_.remove(shared_from_this());
            }
        }
    );
}

void Session::handle_ws_frame(uint8_t* data, size_t len) {
    if (len < 2) return;
    
    uint8_t opcode = data[0] & 0x0F;
    bool masked = (data[1] & 0x80) != 0;
    uint64_t payload_len = data[1] & 0x7F;
    
    size_t pos = 2;
    
    if (payload_len == 126) {
        payload_len = (data[2] << 8) | data[3];
        pos = 4;
    }
    
    uint8_t mask[4] = {0};
    if (masked) {
        memcpy(mask, data + pos, 4);
        pos += 4;
    }
    
    std::string payload;
    payload.reserve(payload_len);
    for (size_t i = 0; i < payload_len && pos + i < len; i++) {
        payload += data[pos + i] ^ mask[i % 4];
    }
    
    if (opcode == 0x01) {
        on_message(payload);
    } else if (opcode == 0x08) {
        manager_.remove(shared_from_this());
    }
}

void Session::on_message(const std::string& message) {
    try {
        json j = json::parse(message);
        std::string type = j.value("type", "");
        
        if (type == "chat") {
            std::string content = j.value("content", "");
            
            json response;
            response["type"] = "message";
            response["content"] = content;
            response["timestamp"] = std::time(nullptr);
            
            manager_.broadcast(response.dump(), nullptr);
        }
    } catch (...) {
        ws_write("{\"error\":\"invalid json\"}");
    }
}

void Session::ws_write(const std::string& message) {
    std::vector<uint8_t> frame;
    frame.push_back(0x81);  // FIN=1, text
    
    if (message.size() < 126) {
        frame.push_back(message.size());
    } else {
        frame.push_back(126);
        frame.push_back((message.size() >> 8) & 0xFF);
        frame.push_back(message.size() & 0xFF);
    }
    
    frame.insert(frame.end(), message.begin(), message.end());
    
    auto self = shared_from_this();
    asio::async_write(socket_, asio::buffer(frame),
        [this, self](boost::system::error_code, std::size_t) {}
    );
}

// Server
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
                    std::make_shared<Session>(std::move(socket), manager_)->start();
                }
                do_accept();
            }
        );
    }

    tcp::acceptor acceptor_;
    SessionManager manager_;
};

int main() {
    try {
        asio::io_context io;
        Server server(io, 8080);
        
        std::cout << "Step 4 WebSocket Server listening on port 8080...\n";
        std::cout << "Connect with: wscat -c ws://localhost:8080\n";
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
project(nuclaw_step04 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost REQUIRED COMPONENTS system)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

include(FetchContent)
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.2
)
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

# 收到广播
< {"content":"你好","timestamp":1234567890,"type":"message"}

# 开多个终端测试广播功能
```

---

## 本章总结

- ✅ 解决了 Step 3 的服务器无法主动推送问题
- ✅ 掌握 WebSocket 协议和握手过程
- ✅ 理解 WebSocket 帧格式
- ✅ 实现消息广播功能
- ✅ 代码从 250 行扩展到 350 行

---

## 课后思考

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
if (message.find("天气") != std::string::npos) {
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
