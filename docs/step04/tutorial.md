# Step 4: WebSocket 实时通信 - 解决 HTTP 上下文问题

> 目标：解决 Step 3 的 HTTP 无状态问题，实现真正的多轮对话
> 
> 难度：⭐⭐⭐ (中等)
> 
> 代码量：约 400 行

## 本节收获

- 理解 HTTP 无状态的局限性
- 掌握 WebSocket 协议（握手、帧、双向通信）
- 实现长连接下的上下文持久化
- **Agent Loop 初步完整**：可以维护状态了

---

## 第一步：回顾 Step 3 的核心问题

Step 3 实现了规则 AI，但有个致命问题：

```
用户：北京天气如何？
AI：北京今天晴天。

用户：那上海呢？  ← 应该理解为"上海天气"
AI：我不理解。  ← 失败！
```

**根本原因：HTTP 无状态**

```cpp
// 每次请求都是新的连接
Request 1: POST /chat "北京天气"
    → 新建 Context → 处理 → 销毁 Context → 响应
    
Request 2: POST /chat "那上海呢"
    → 新建 Context（空的！）→ 不理解 → 响应
```

HTTP 的"请求-响应"模式适合获取资源，但不适合**持续对话**。

---

## 第二步：WebSocket 解决方案

### 什么是 WebSocket？

**HTTP 的问题：**
- 半双工：客户端请求，服务器响应，然后连接关闭
- 服务器不能主动推送
- 每次请求都要带 HTTP 头（开销大）

**WebSocket 的优势：**
- 全双工：双方随时发送消息
- 长连接：一次握手，持续通信
- 服务器可以主动推送
- 帧头小（2-14 字节 vs HTTP 几百字节）

### WebSocket 握手过程

```
客户端                           服务器
   │                              │
   ├── HTTP Upgrade 请求 ────────▶│
   │  GET /ws HTTP/1.1            │
   │  Upgrade: websocket          │
   │  Connection: Upgrade         │
   │                              │
   │◀── 101 Switching Protocols ─┤
   │  同意升级                    │
   │                              │
   ═══ WebSocket 连接建立 ════════
   │                              │
   ├── WebSocket 帧: "你好" ─────▶│
   │◀── WebSocket 帧: "你好！" ──┤
   │                              │
   ├── WebSocket 帧: "天气" ─────▶│
   │◀── WebSocket 帧: "晴天" ────┤
   │                              │
   │  （连接保持，持续对话）        │
```

### 与 HTTP 的关键区别

| 特性 | HTTP | WebSocket |
|:---|:---|:---|
| 连接 | 短连接 | 长连接 |
| 通信 | 客户端请求 | 双方主动 |
| 开销 | 大（HTTP 头） | 小（2-14 字节） |
| 适用 | 获取资源 | 实时通信 |

---

## 第三步：代码演进

从 Step 3 演进（新增约 100 行）：

```diff
+ // 新增：检测 WebSocket 升级
+ bool is_websocket_upgrade() const {
+     return headers["Upgrade"] == "websocket";
+ }

+ // 新增：WebSocketSession 类
+ class WebSocketSession {
+     void start() {
+         ws_.async_accept(...);  // 握手
+     }
+     
+     void do_read() {
+         ws_.async_read(...);    // 读取消息帧
+     }
+     
+     void on_read(...) {
+         string msg = parse_websocket_frame(...);
+         string reply = ai.process(msg, ctx_);  // ← 关键：ctx_ 持久！
+         ws_.async_write(reply);
+     }
+     
+     ChatContext ctx_;  // ← 关键：连接期间一直存在！
+ };
```

### 关键改进：上下文持久化

```cpp
// Step 3 (HTTP): Context 每次请求新建
void handle_request() {
    ChatContext ctx;  // ← 新建
    process(input, ctx);
    // ctx 销毁
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

因为 WebSocket 连接保持期间，`ChatContext` 一直存在：

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
    return "上海今天多云";
}
```

---

## 第五步：这就是 Agent Loop 的雏形

现在我们有了一个**真正可工作的 Agent Loop**：

```
输入 → 理解（规则+上下文）→ 响应
 ↑___________________________↓
        （循环，状态保持）
```

虽然规则还很简陋，但**结构完整了**：
- ✅ 输入处理（WebSocket 消息）
- ✅ 理解（规则匹配 + 上下文）
- ✅ 状态保持（Context 持久化）
- ✅ 输出生成（回复用户）

### 仍存在的问题

**规则太死板：**
- 只能匹配预设关键词
- "今天怎么样？" → 无法理解（不是"天气"关键词）
- 每增加功能都要改代码

**下一章解决方案：LLM 语义理解**

---

## 第六步：运行测试

### 编译运行

```bash
g++ -std=c++17 main.cpp -o server -lboost_system -lpthread
./server
```

### WebSocket 测试

```bash
# 安装 wscat
npm install -g wscat

# 连接
wscat -c ws://localhost:8080/ws

# 测试多轮对话
> 你好
< 你好！我是 NuClaw AI。

> 北京天气
< 北京今天晴天，25°C。

> 那上海呢  ← 关键测试！
< 上海今天多云，22°C。
< 💡 (我理解了你想比较...)

> 现在几点
< 当前时间是 2024-01-15 14:30:25
```

---

## 本节总结

### 我们解决了什么？

**Step 3 的问题：HTTP 无状态 → 上下文丢失**

**Step 4 的解决：WebSocket 长连接 → 上下文持久**

### Agent Loop 演进

```
Step 3: 输入 → 规则匹配 → 输出
          ↓（失败）
        上下文丢失

Step 4: 输入 → 规则匹配 → 输出
          ↑___________↓
         WebSocket 保持状态
```

### 代码演进

```
Step 3: HTTP + 规则 AI (350行)
   ↓ +100 行
Step 4: WebSocket + 持久上下文 (400行)
   - WebSocket 握手
   - WebSocketSession 类
   - 上下文持久化
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

## 附：WebSocket vs HTTP 对比代码

```cpp
// HTTP (Step 3): 每次请求独立
class Session {
    void handle() {
        ChatContext ctx;  // ← 临时
        process(input, ctx);
        // ctx 销毁
    }
};

// WebSocket (Step 4): 连接期间保持
class WebSocketSession {
    ChatContext ctx_;  // ← 持久
    
    void on_message(string msg) {
        process(msg, ctx_);  // 始终使用同一个 ctx_
    }
};
```

**这就是 Agent Loop 需要的状态机。**
