# Step 3: Agent Loop - 状态机与对话管理

> 目标：构建 AI Agent 的核心循环，管理对话状态和历史
> 
> 难度：⭐⭐⭐ (中等)
> 
> 代码量：约 250 行

## 本节收获

- 理解 Agent Loop 的核心概念
- 掌握状态机设计模式
- 学会使用 WebSocket 实现实时通信
- 实现对话历史管理

---

## 什么是 Agent Loop？

### 传统 HTTP 请求 vs Agent Loop

**传统 HTTP：**
```
客户端 ──请求──▶ 服务器 ──响应──▶ 客户端
   │                                  │
   └──────── 一次性交互 ──────────────┘
```

**Agent Loop：**
```
        ┌─────────────────────────────────────┐
        │                                     │
        ▼                                     │
   ┌─────────┐    ┌─────────┐    ┌────────┐ │
   │  接收   │───▶│  思考   │───▶│  响应  │ │
   │  输入   │    │(LLM/逻辑)│    │        │ │
   └────┬────┘    └────┬────┘    └───┬────┘ │
        │              │             │      │
        │         ┌────┴────┐        │      │
        │         ▼         ▼        │      │
        │      ┌────────┐ ┌────────┐│      │
        │      │工具调用 │ │直接回复 ││      │
        │      └───┬────┘ └────────┘│      │
        │          │                │      │
        └──────────┴────────────────┴──────┘
                     循环往复
```

Agent 不是一次性回答，而是**持续对话、调用工具、迭代思考**。

---

## 状态机设计

### 为什么需要状态机？

Agent 有多种工作状态：
- **IDLE**：等待用户输入
- **THINKING**：正在思考/调用 LLM
- **TOOL_CALLING**：正在执行工具
- **RESPONDING**：正在生成回复

状态机确保：
1. 同一时间只有一个状态
2. 状态转换清晰可控
3. 避免竞态条件

### 状态转换图

```
                    ┌─────────────┐
         ┌─────────│    IDLE     │◄────────┐
         │         │   (空闲)    │         │
         │         └──────┬──────┘         │
         │                │                │
         │                ▼                │
         │         ┌─────────────┐         │
         │    ┌───│  THINKING   │───┐     │
         │    │   │  (思考中)   │   │     │
         │    │   └──────┬──────┘   │     │
         │    │          │          │     │
         │    │    ┌─────┴─────┐    │     │
         │    │    ▼           ▼    │     │
         │    │ ┌────────┐ ┌────────┐│    │
         │    └─┤  需要  │ │不需要  │┘    │
         │      │ 工具   │ │工具   │      │
         │      └───┬────┘ └───┬────┘      │
         │          │          │           │
         │          ▼          ▼           │
         │   ┌─────────────┐ ┌────────┐   │
         │   │ TOOL_CALLING│ │RESPONDING│  │
         │   │ (调用工具)  │ │(响应中) │   │
         │   └──────┬──────┘ └───┬────┘   │
         │          │            │        │
         │          └─────┬──────┘        │
         │                │               │
         └────────────────┴───────────────┘
```

### C++ 状态机实现

```cpp
enum class State {
    IDLE,           // 空闲
    THINKING,       // 思考中
    TOOL_CALLING,   // 调用工具
    RESPONDING      // 响应中
};

class AgentLoop {
    std::map<std::string, Session> sessions_;
    
    std::string process(const std::string& input, 
                       const std::string& session_id) {
        auto& session = sessions_[session_id];
        
        // 状态转换: IDLE -> THINKING
        session.state = State::THINKING;
        
        // 执行业务逻辑...
        
        // 状态转换: THINKING -> IDLE
        session.state = State::IDLE;
        return response;
    }
};
```

---

## WebSocket 实时通信

### HTTP vs WebSocket

| 特性 | HTTP | WebSocket |
|:---|:---|:---|
| 协议 | 请求-响应 | 全双工 |
| 连接 | 短连接/长连接 | 长连接 |
| 实时性 | 差（轮询） | 好（推送） |
| 头部开销 | 大 | 小 |
| 适用场景 | REST API | 实时通信 |

### WebSocket 握手

```
客户端                              服务器
   │                                  │
   ├── ① HTTP Upgrade 请求 ─────────▶│
   │    Connection: Upgrade           │
   │    Upgrade: websocket            │
   │                                  │
   │◀── ② 101 Switching Protocols ──┤
   │    握手成功                      │
   │                                  │
   ═══════════════════════════════════
   │    ③ WebSocket 数据帧           │
   │◄══════ 双向通信 ═══════════════►│
   ═══════════════════════════════════
```

### Boost.Beast WebSocket

```cpp
#include <boost/beast.hpp>

namespace websocket = boost::beast::websocket;

// WebSocket 流包装 TCP socket
websocket::stream<tcp::socket> ws_;

// 接受 WebSocket 握手
ws_.async_accept(callback);

// 读取数据（自动处理 WebSocket 帧）
ws_.async_read(buffer_, callback);

// 发送文本帧
ws_.text(true);
ws_.async_write(data, callback);
```

**Beast 自动处理：**
- WebSocket 握手
- 数据帧封装/解封装
- Ping/Pong 心跳
- 关闭帧

---

## 对话历史管理

### 为什么需要历史？

LLM 是**无状态**的，每次调用都独立。要让 Agent "记得" 之前的对话，需要：

```cpp
struct Message {
    std::string role;      // user / assistant / system
    std::string content;   // 内容
    std::string timestamp; // 时间戳
};

struct Session {
    std::vector<Message> history;
    size_t max_history = 100;  // 防止内存无限增长
};
```

### 历史记录管理

```cpp
void add_message(const std::string& role, 
                 const std::string& content) {
    // 超出限制时移除最旧的消息
    if (history.size() >= max_history) {
        history.erase(history.begin());
    }
    history.push_back({role, content, current_time()});
}
```

**策略：**
- FIFO（先进先出）：简单，但可能丢失重要上下文
- 摘要压缩：保留旧消息的摘要（Step 5 实现）
- 重要性评分：保留重要消息（Step 5 实现）

---

## 完整运行测试

### 1. 编译运行

```bash
cd src/step03
mkdir build && cd build
cmake .. && make
./nuclaw_step03
```

### 2. WebSocket 测试

使用浏览器控制台：

```javascript
// 连接
const ws = new WebSocket('ws://localhost:8081');

// 接收消息
ws.onmessage = e => console.log('Received:', e.data);

// 发送消息
ws.send('Hello, Agent!');
ws.send('What is my session ID?');
```

### 3. 使用 wscat 命令行工具

```bash
# 安装
npm install -g wscat

# 连接
wscat -c ws://localhost:8081

# 交互
> Hello
< Echo: Hello [state: idle, history: 2 msgs]
```

---

## 代码亮点

### 1. `reinterpret_cast<uintptr_t>(this)`

```cpp
session_id_ = "ws_" + std::to_string(
    reinterpret_cast<uintptr_t>(this)
);
```

用对象地址生成唯一 ID，简单有效。

### 2. `beast::bind_front_handler`

```cpp
ws_.async_accept(
    beast::bind_front_handler(
        &WsSession::on_accept, 
        shared_from_this()
    )
);
```

Boost.Beast 提供的辅助函数，自动绑定 `this` 到回调。

### 3. `beast::flat_buffer`

```cpp
beast::flat_buffer buffer_;
```

Beast 的动态缓冲区，自动管理内存，适合网络 I/O。

---

## 下一步

→ **Step 4: Agent Loop - 执行引擎**

我们将实现：
- 工具调用循环检测
- 并行工具执行
- 执行超时控制
