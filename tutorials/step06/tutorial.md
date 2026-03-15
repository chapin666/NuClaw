# Step 6: 会话管理与用户隔离

> 目标：实现多用户支持，每个用户独立会话和上下文
> 
> 难度：⭐⭐⭐ | 代码量：约 500 行 | 预计学习时间：2-3 小时

---

## 一、问题引入

### 1.1 Step 5 的问题

我们的 Agent 目前存在几个严重问题：

**问题 1：没有用户隔离**
```
用户A连接 ──▶ 服务器 ──▶ 使用同一个对话历史
用户B连接 ──▶ 服务器 ──▶ 也使用同一个对话历史

结果：
- 用户A的消息被用户B看到
- 用户A的个人信息被用户B获取
- 完全无法支持多用户
```

**问题 2：连接断开后记忆丢失**
```
用户: 我叫小明
Agent: 你好小明！
[用户断开连接]
[用户重新连接]
用户: 我叫什么？
Agent: 抱歉，我不知道你是谁...  ← 记忆丢失了！
```

**问题 3：对话历史无限增长**
```
对话 100 轮后，history_ 向量变得非常庞大
每次调用 LLM 都要带上全部历史
Token 费用飙升...
可能超出模型上下文限制（4096/8192/128k tokens）
```

### 1.2 本章目标

1. **用户隔离**：每个用户独立的 Session ID
2. **会话持久化**：连接断开后会话不丢失
3. **会话恢复**：用户可以恢复之前的对话
4. **会话过期**：长时间不活动的会话自动清理

---

## 二、核心概念

### 2.1 Session ID 机制

**什么是 Session ID？**

Session ID 是标识用户会话的唯一标识符：
```
用户连接 ──▶ 生成 Session ID ──▶ 创建独立会话
                │
                ▼
        session_map_[session_id] = session_data
```

**Session ID 生成方式：**
- UUID（通用唯一标识符）：`550e8400-e29b-41d4-a716-446655440000`
- 时间戳 + 随机数：`1704067200_abc123`
- 用户ID（如果有认证系统）：`user_12345`

### 2.2 会话存储架构

```
┌─────────────────────────────────────────────────────────────┐
│                      SessionManager                          │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│   sessions_                                                 │
│   ┌─────────────┐    ┌─────────────┐    ┌─────────────┐    │
│   │ Session ID  │    │ Session ID  │    │ Session ID  │    │
│   │  "abc123"   │    │  "def456"   │    │  "ghi789"   │    │
│   └──────┬──────┘    └──────┬──────┘    └──────┬──────┘    │
│          │                  │                  │           │
│          ▼                  ▼                  ▼           │
│   ┌─────────────┐    ┌─────────────┐    ┌─────────────┐    │
│   │  Session    │    │  Session    │    │  Session    │    │
│   │  Data       │    │  Data       │    │  Data       │    │
│   │  • history  │    │  • history  │    │  • history  │    │
│   │  • user_id  │    │  • user_id  │    │  • user_id  │    │
│   │  • last_activity│ • last_activity│  • last_activity│   │
│   └─────────────┘    └─────────────┘    └─────────────┘    │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 2.3 会话生命周期

```
        用户连接
            │
            ▼
    ┌───────────────┐
    │  查找 Session │
    │  (by ID)      │
    └───────┬───────┘
            │
    ┌───────┴───────┐
    │               │
   存在            不存在
    │               │
    ▼               ▼
┌────────┐    ┌────────────┐
│ 恢复   │    │ 创建新     │
│ 会话   │    │ Session    │
└───┬────┘    └─────┬──────┘
    │               │
    └───────┬───────┘
            │
            ▼
    ┌───────────────┐
    │  WebSocket    │
    │  通信         │
    └───────┬───────┘
            │
       用户断开
            │
            ▼
    ┌───────────────┐
    │ 保存会话      │
    │ (不删除)      │
    └───────────────┘
```

---

## 三、代码结构详解

### 3.1 Session 数据结构

```cpp
struct SessionData {
    std::string session_id;
    std::string user_id;
    std::vector<json> history;
    std::chrono::steady_clock::time_point last_activity;
    
    // 检查是否过期（默认30分钟）
    bool is_expired(std::chrono::minutes timeout = std::chrono::minutes(30)) {
        auto now = std::chrono::steady_clock::now();
        return (now - last_activity) > timeout;
    }
    
    // 更新活动时间
    void touch() {
        last_activity = std::chrono::steady_clock::now();
    }
};
```

### 3.2 SessionManager 类

```cpp
class SessionManager {
public:
    // 创建新会话
    std::string create_session(const std::string& user_id = "") {
        std::string session_id = generate_uuid();
        
        SessionData data;
        data.session_id = session_id;
        data.user_id = user_id.empty() ? session_id : user_id;
        data.touch();
        
        std::lock_guard<std::mutex> lock(mutex_);
        sessions_[session_id] = std::move(data);
        
        return session_id;
    }
    
    // 获取会话（如果不存在返回 nullptr）
    SessionData* get_session(const std::string& session_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = sessions_.find(session_id);
        if (it != sessions_.end()) {
            it->second.touch();
            return &(it->second);
        }
        return nullptr;
    }
    
    // 更新会话历史
    void update_history(const std::string& session_id, 
                        const std::vector<json>& history) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = sessions_.find(session_id);
        if (it != sessions_.end()) {
            it->second.history = history;
            it->second.touch();
        }
    }
    
    // 清理过期会话
    void cleanup_expired(std::chrono::minutes timeout = std::chrono::minutes(30)) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        for (auto it = sessions_.begin(); it != sessions_.end();) {
            if (it->second.is_expired(timeout)) {
                it = sessions_.erase(it);
            } else {
                ++it;
            }
        }
    }

private:
    std::string generate_uuid() {
        // 简化版 UUID 生成
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(0, 15);
        
        const char* chars = "0123456789abcdef";
        std::string uuid = "xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx";
        
        for (auto& c : uuid) {
            if (c == 'x') c = chars[dis(gen)];
            else if (c == 'y') c = chars[(dis(gen) & 0x03) | 0x08];
        }
        return uuid;
    }

    std::unordered_map<std::string, SessionData> sessions_;
    std::mutex mutex_;
};
```

### 3.3 WebSocket 协议扩展

客户端连接时需要提供 Session ID：

```
方式 1：URL 参数
wss://example.com/chat?session_id=abc123

方式 2：第一条消息
{
    "type": "auth",
    "session_id": "abc123"
}

方式 3：WebSocket 子协议
Sec-WebSocket-Protocol: chat, session_id=abc123
```

### 3.4 修改后的 Session 类

```cpp
class AgentSession : public enable_shared_from_this<AgentSession> {
public:
    AgentSession(tcp::socket socket, 
                 SessionManager& session_mgr,
                 LLMClient& llm)
        : ws_(move(socket)),
          session_mgr_(session_mgr),
          llm_(llm) {}

    void start() {
        // 等待客户端发送 session_id
        do_read_auth();
    }

private:
    void do_read_auth() {
        auto self = shared_from_this();
        
        ws_.async_read(buffer_,
            [this, self](error_code ec, size_t len) {
                if (!ec) {
                    handle_auth(string(buffer_.data(), len));
                }
            }
        );
    }
    
    void handle_auth(const string& message) {
        try {
            json j = json::parse(message);
            
            if (j.value("type", "") == "auth") {
                string session_id = j.value("session_id", "");
                
                if (session_id.empty()) {
                    // 创建新会话
                    session_id_ = session_mgr_.create_session();
                    session_data_ = session_mgr_.get_session(session_id_);
                    
                    // 通知客户端新的 session_id
                    send_auth_response(session_id_, true);
                } else {
                    // 恢复已有会话
                    session_data_ = session_mgr_.get_session(session_id);
                    
                    if (session_data_) {
                        session_id_ = session_id;
                        send_auth_response(session_id_, true);
                    } else {
                        // 会话不存在或已过期
                        send_auth_response("", false);
                        return;
                    }
                }
                
                // 开始正常通信
                do_read();
            }
        } catch (...) {
            send_auth_response("", false);
        }
    }
    
    void on_message(const string& message) {
        try {
            json j = json::parse(message);
            string content = j.value("content", "");
            
            // 添加到会话历史
            session_data_->history.push_back({
                {"role", "user"},
                {"content", content}
            });
            
            // 处理（使用会话历史）
            process();
        } catch (...) {
            send("{\"error\":\"invalid message\"}");
        }
    }
    
    void process() {
        vector<Tool> tools = {weather_tool};
        
        // 使用会话历史调用 LLM
        LLMResponse response = llm_.chat(session_data_->history, tools);
        
        if (response.has_tool_call) {
            string result = execute_tool(response.tool_name, 
                                        response.tool_args);
            
            add_tool_call(response.tool_name, 
                         response.tool_args, result);
            
            LLMResponse final_resp = llm_.chat(session_data_->history, tools);
            send_reply(final_resp.content);
        } else {
            send_reply(response.content);
        }
        
        // 更新会话存储
        session_mgr_.update_history(session_id_, session_data_->history);
    }

    websocket::stream<tcp::socket> ws_;
    SessionManager& session_mgr_;
    LLMClient& llm_;
    
    string session_id_;
    SessionData* session_data_ = nullptr;
    array<char, 4096> buffer_;
};
```

### 3.5 定期清理过期会话

```cpp
class Server {
public:
    Server(io_context& io, unsigned short port)
        : acceptor_(io, tcp::endpoint(tcp::v4(), port)),
          cleanup_timer_(io) {
        do_accept();
        start_cleanup_timer();
    }

private:
    void start_cleanup_timer() {
        cleanup_timer_.expires_after(chrono::minutes(5));
        cleanup_timer_.async_wait(
            [this](error_code ec) {
                if (!ec) {
                    session_mgr_.cleanup_expired();
                    start_cleanup_timer();  // 继续定时
                }
            }
        );
    }

    tcp::acceptor acceptor_;
    steady_timer cleanup_timer_;
    SessionManager session_mgr_;
};
```

---

## 四、交互流程

### 4.1 新用户连接

```
客户端                                  服务器
  │                                       │
  │── WebSocket 连接 ────────────────────▶│
  │                                       │
  │── {                                 │
  │     "type": "auth",                │
  │     "session_id": ""  ← 空表示新用户 │
  │   } ────────────────────────────────▶│
  │                                       │
  │◀── {                                │
  │      "type": "auth_response",      │
  │      "success": true,              │
  │      "session_id": "abc123"  ← 新ID │
  │    } ────────────────────────────────│
  │                                       │
  │── {                                 │
  │     "type": "chat",                │
  │     "content": "你好"               │
  │   } ────────────────────────────────▶│
  │                                       │
```

### 4.2 老用户恢复会话

```
客户端                                  服务器
  │                                       │
  │── WebSocket 连接 ────────────────────▶│
  │                                       │
  │── {                                 │
  │     "type": "auth",                │
  │     "session_id": "abc123" ← 已有ID │
  │   } ────────────────────────────────▶│
  │                                       │
  │◀── {                                │
  │      "type": "auth_response",      │
  │      "success": true,              │
  │      "session_id": "abc123",       │
  │      "restored": true  ← 恢复成功   │
  │    } ────────────────────────────────│
  │                                       │
  │── {                                 │
  │     "type": "chat",                │
  │     "content": "我叫什么？" ← 还记得 │
  │   } ────────────────────────────────▶│
  │                                       │
```

---

## 五、本章总结

- ✅ 实现了用户隔离（Session ID）
- ✅ 会话持久化（连接断开后不丢失）
- ✅ 会话恢复（重新连接后恢复历史）
- ✅ 会话过期清理（防止内存泄漏）
- ✅ 代码从 450 行扩展到 500 行

---

## 六、课后思考

会话管理解决了用户隔离问题，但对话历史仍然会无限增长：

```
用户聊天 1000 轮后：
- history 向量有 1000+ 条消息
- 每次调用 LLM 都要发送 1000 条历史
- Token 费用极高
- 可能超出模型上下文限制
```

如何限制对话历史的长度？

<details>
<summary>点击查看下一章 💡</summary>

**Step 7: 短期记忆管理**

我们将学习：
- 滑动窗口：只保留最近 N 轮对话
- 对话摘要：压缩历史信息
- Token 计数和限制

</details>
