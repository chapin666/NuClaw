# Step 4: WebSocket 实时通信

> 目标：掌握 WebSocket 协议，实现全双工实时通信
> 
> 难度：⭐⭐⭐ (中等)
> 
> 代码量：约 350 行（较 Step 3 新增约 100 行）

---

## 问题引入

**Step 3 的问题：**

HTTP 是请求-响应模式，服务器无法主动推送消息：

```
客户端              服务器
   │                  │
   ├── 请求1 ────────▶│
   │◀─ 响应1 ─────────┤
   │                  │ ← 服务器有新消息，但无法主动发送！
   ├── 轮询 ────────▶ │ "有新消息吗？"
   │◀─ 没有 ──────────┤
   ├── 轮询 ────────▶ │ "有新消息吗？"
   │◀─ 有了！ ────────┤ ← 延迟高，浪费资源
```

**问题：**
- 轮询浪费带宽和 CPU
- 消息延迟取决于轮询间隔
- 无法做到真正的实时

**本章目标：** 使用 WebSocket 实现全双工通信，服务器可以主动推送消息。

---

## 解决方案

### WebSocket 协议

WebSocket 在 TCP 之上提供**全双工**通信通道：

```
客户端              服务器
   │                  │
   ├── HTTP 握手 ────▶│  (升级为 WebSocket)
   │◀─ 101 Switching─┤
   │                  │
   ═══════════════════  ← 升级为 WebSocket 连接
   │                  │
   ├── 消息1 ────────▶│  (随时发送)
   │◀─ 消息2 ─────────┤  (服务器主动推送)
   ├── 消息3 ────────▶│
   │◀─ 消息4 ─────────┤
   │                  │
   │  (保持连接，直到一方关闭)
```

**优势：**
- 建立连接后，双方随时可以发送消息
- 服务器可以主动推送
- 消息头部极小（2-14字节）

### WebSocket 握手

```http
客户端请求：
GET /chat HTTP/1.1
Host: server.example.com
Upgrade: websocket
Connection: Upgrade
Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==
Sec-WebSocket-Version: 13

服务器响应：
HTTP/1.1 101 Switching Protocols
Upgrade: websocket
Connection: Upgrade
Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=
```

---

## 代码对比

### Step 3 的关键代码

```cpp
// HTTP 请求-响应模式
void do_read() {
    socket_.async_read_some(buffer_,
        [this](error_code ec, size_t len) {
            auto req = parse_request(buffer_);
            auto res = router_.handle(req);
            do_write(res);  // ← 必须等待客户端请求才能响应
        }
    );
}
```

### Step 4 的修改

**主要改动：**
1. 实现 WebSocket 握手
2. 添加 WebSocket 帧解析/编码
3. 支持双向消息传递
4. 添加消息广播功能

```cpp
// 新增：WebSocket 握手
class Session : public std::enable_shared_from_this<Session> {
public:
    // 阶段1：HTTP 握手
    void start() {
        do_http_read();
    }

private:
    void do_http_read() {
        auto self = shared_from_this();
        socket_.async_read_some(asio::buffer(buffer_),
            [this, self](error_code ec, size_t len) {
                if (!ec) {
                    std::string request(buffer_.data(), len);
                    if (is_websocket_upgrade(request)) {
                        do_websocket_handshake(request);
                    }
                }
            }
        );
    }
    
    // 检查是否是 WebSocket 升级请求
    bool is_websocket_upgrade(const std::string& req) {
        return req.find("Upgrade: websocket") != std::string::npos;
    }
    
    // 执行 WebSocket 握手
    void do_websocket_handshake(const std::string& request) {
        // 提取 Sec-WebSocket-Key
        std::string key = extract_key(request);
        
        // 计算 Sec-WebSocket-Accept
        // accept = base64(sha1(key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"))
        std::string accept = compute_accept(key);
        
        // 发送握手响应
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
    
    // WebSocket 帧解析
    void do_ws_read() {
        auto self = shared_from_this();
        
        // 读取帧头（至少2字节）
        ws_buffer_.resize(2);
        asio::async_read(socket_, asio::buffer(ws_buffer_),
            [this, self](error_code ec, size_t) {
                if (!ec) {
                    parse_ws_frame();
                }
            }
        );
    }
    
    // 解析 WebSocket 帧
    void parse_ws_frame() {
        uint8_t fin = (ws_buffer_[0] & 0x80) != 0;
        uint8_t opcode = ws_buffer_[0] & 0x0F;
        uint8_t masked = (ws_buffer_[1] & 0x80) != 0;
        uint64_t payload_len = ws_buffer_[1] & 0x7F;
        
        // 处理不同 payload 长度
        size_t header_len = 2;
        if (payload_len == 126) {
            header_len += 2;  // 扩展长度（2字节）
        } else if (payload_len == 127) {
            header_len += 8;  // 扩展长度（8字节）
        }
        
        if (masked) {
            header_len += 4;  // Masking key（4字节）
        }
        
        // 继续读取完整帧...
        // 简化版：这里只处理文本消息
        if (opcode == 0x01) {  // 文本帧
            // 读取 payload 并解码
            // ...
        } else if (opcode == 0x08) {  // 关闭帧
            close();
        }
    }
    
    // 发送 WebSocket 文本消息
    void ws_write(const std::string& message) {
        // 构造 WebSocket 文本帧
        std::vector<uint8_t> frame;
        frame.push_back(0x81);  // FIN=1, opcode=text
        
        // payload 长度
        if (message.size() < 126) {
            frame.push_back(message.size());
        } else if (message.size() < 65536) {
            frame.push_back(126);
            frame.push_back((message.size() >> 8) & 0xFF);
            frame.push_back(message.size() & 0xFF);
        } else {
            // 64位长度（简化，省略）
        }
        
        // payload 数据
        frame.insert(frame.end(), message.begin(), message.end());
        
        // 发送
        auto self = shared_from_this();
        asio::async_write(socket_, asio::buffer(frame),
            [this, self](error_code, size_t) {
                // 发送完成
            }
        );
    }
    
    // 新增：服务器主动推送
    void push_message(const std::string& msg) {
        if (is_websocket_) {
            ws_write(msg);
        }
    }

    tcp::socket socket_;
    bool is_websocket_ = false;
    std::vector<uint8_t> ws_buffer_;
    std::array<char, 4096> buffer_;
};
```

---

## 文件变更清单

| 文件 | 变更类型 | 说明 |
|:---|:---|:---|
| `main.cpp` | 修改 | 添加 WebSocket 握手、帧解析、双向通信 |

---

## 完整源码（简化版）

为了可读性，这里提供一个简化但完整的 WebSocket 实现：

```cpp
#include <boost/asio.hpp>
#include <nlohmann/json.hpp>
#include <iostream>
#include <sstream>
#include <memory>
#include <vector>
#include <set>
#include <crypto/sha.h>  // 需要 OpenSSL 或类似库
#include <base64.hpp>

namespace asio = boost::asio;
using tcp = asio::ip::tcp;
using json = nlohmann::json;

// 前向声明
class Session;

// 会话管理器（支持广播）
class SessionManager {
public:
    void add(std::shared_ptr<Session> session) {
        sessions_.insert(session);
    }
    
    void remove(std::shared_ptr<Session> session) {
        sessions_.erase(session);
    }
    
    void broadcast(const std::string& message, std::shared_ptr<Session> exclude = nullptr);

private:
    std::set<std::shared_ptr<Session>> sessions_;
};

// WebSocket 会话
class Session : public std::enable_shared_from_this<Session> {
public:
    Session(tcp::socket socket, SessionManager& manager)
        : socket_(std::move(socket)), manager_(manager) {}

    void start() {
        manager_.add(shared_from_this());
        do_http_read();
    }
    
    void send(const std::string& message) {
        if (is_websocket_) {
            ws_write(message);
        }
    }

private:
    void do_http_read() {
        auto self = shared_from_this();
        socket_.async_read_some(asio::buffer(buffer_),
            [this, self](boost::system::error_code ec, std::size_t len) {
                if (!ec) {
                    std::string request(buffer_.data(), len);
                    if (is_websocket_upgrade(request)) {
                        do_websocket_handshake(request);
                    } else {
                        // 普通 HTTP 请求
                        handle_http_request(request);
                    }
                }
            }
        );
    }
    
    bool is_websocket_upgrade(const std::string& req) {
        return req.find("Upgrade: websocket") != std::string::npos ||
               req.find("upgrade: websocket") != std::string::npos;
    }
    
    void do_websocket_handshake(const std::string& request) {
        std::string key = extract_key(request);
        std::string accept = compute_accept(key);
        
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
    
    std::string extract_key(const std::string& request) {
        auto pos = request.find("Sec-WebSocket-Key: ");
        if (pos != std::string::npos) {
            pos += 19;
            auto end = request.find("\r\n", pos);
            return request.substr(pos, end - pos);
        }
        return "";
    }
    
    std::string compute_accept(const std::string& key) {
        std::string magic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
        std::string combined = key + magic;
        
        // SHA1 哈希
        unsigned char hash[20];
        SHA1(reinterpret_cast<const unsigned char*>(combined.data()), 
             combined.size(), hash);
        
        // Base64 编码
        return base64_encode(hash, 20);
    }
    
    void handle_http_request(const std::string& request) {
        // 简单 HTTP 响应
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
    
    void do_ws_read() {
        auto self = shared_from_this();
        socket_.async_read_some(asio::buffer(buffer_),
            [this, self](boost::system::error_code ec, std::size_t len) {
                if (!ec) {
                    handle_ws_frame(reinterpret_cast<uint8_t*>(buffer_.data()), len);
                    do_ws_read();  // 继续读取
                } else {
                    manager_.remove(shared_from_this());
                }
            }
        );
    }
    
    void handle_ws_frame(uint8_t* data, size_t len) {
        if (len < 2) return;
        
        uint8_t opcode = data[0] & 0x0F;
        bool masked = (data[1] & 0x80) != 0;
        uint64_t payload_len = data[1] & 0x7F;
        
        size_t pos = 2;
        
        // 处理扩展长度
        if (payload_len == 126) {
            payload_len = (data[2] << 8) | data[3];
            pos = 4;
        }
        
        // 获取 masking key
        uint8_t mask[4] = {0};
        if (masked) {
            memcpy(mask, data + pos, 4);
            pos += 4;
        }
        
        // 解码 payload
        std::string payload;
        payload.reserve(payload_len);
        for (size_t i = 0; i < payload_len && pos + i < len; i++) {
            payload += data[pos + i] ^ mask[i % 4];
        }
        
        // 处理消息
        if (opcode == 0x01) {  // 文本帧
            on_message(payload);
        } else if (opcode == 0x08) {  // 关闭帧
            manager_.remove(shared_from_this());
        }
    }
    
    void on_message(const std::string& message) {
        try {
            json j = json::parse(message);
            std::string type = j.value("type", "");
            
            if (type == "chat") {
                std::string content = j.value("content", "");
                
                // 广播给所有客户端（包括发送者）
                json response;
                response["type"] = "message";
                response["content"] = content;
                response["timestamp"] = std::time(nullptr);
                
                manager_.broadcast(response.dump(), nullptr);
            }
        } catch (...) {
            // 解析失败，原样返回
            ws_write("{\"error\":\"invalid json\"}");
        }
    }
    
    void ws_write(const std::string& message) {
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

    tcp::socket socket_;
    SessionManager& manager_;
    bool is_websocket_ = false;
    std::array<char, 4096> buffer_;
};

void SessionManager::broadcast(const std::string& message, 
                               std::shared_ptr<Session> exclude) {
    for (auto& session : sessions_) {
        if (session != exclude) {
            session->send(message);
        }
    }
}

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
find_package(OpenSSL REQUIRED)  # 用于 SHA1

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
```

---

## 本章总结

- ✅ 解决了 Step 3 的服务器无法主动推送问题
- ✅ 实现 WebSocket 握手协议
- ✅ 实现 WebSocket 帧解析和编码
- ✅ 添加会话管理和消息广播
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

**方案 1：规则匹配**（Step 3 用过的方法）
```cpp
if (message.find("天气") != std::string::npos) {
    return "晴天，25°C";  // 硬编码，不够智能
}
```

**方案 2：调用外部 API**
- 需要 HTTP 客户端调用天气服务
- 如何异步等待 API 响应？

如果要让 Agent 真正"智能"，它需要：
1. 理解用户意图（不只是关键词匹配）
2. 调用外部工具获取数据
3. 整合信息后生成回复

这该怎么做？

<details>
<summary>点击查看下一章 💡</summary>

**Step 5: Agent 核心 — LLM 接入与工具调用**

我们将学习：
- LLM API 接入（OpenAI/Claude）
- 异步 HTTP 客户端
- Function Calling（工具调用）
- Agent Loop：理解 → 决策 → 执行

预期效果：Agent 能真正理解用户问题，并调用工具获取信息来回答！

</details>
