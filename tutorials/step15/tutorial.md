# Step 15: IM 平台接入 — 飞书/钉钉/企微/Telegram

> 目标：让你的 Agent 连接真实世界，支持主流 IM 平台
> 
> 难度：⭐⭐⭐ (进阶)
> 
> 代码量：约 1100 行

## 本节收获

- 理解 Webhook、事件订阅、长连接等 IM 接入模式
- 掌握多平台消息格式的统一抽象
- 实现一个可同时对接多个 IM 平台的 Agent
- 处理群聊 @提及、私聊、富文本消息等场景

---

## 第一部分：IM 接入架构设计

### 1.1 为什么需要接入 IM 平台？

**当前问题：**

你的 Agent 目前只能通过 HTTP/WebSocket 接口访问，用户需要：
- 打开浏览器或 curl 命令
- 记住复杂的 URL
- 没有消息推送能力

**IM 平台的价值：**

```
用户视角：
┌─────────────────────────────────────────────┐
│  打开飞书/钉钉/微信 → 找到机器人 → 直接对话   │
│  无需安装软件，无需记住地址，随时可聊         │
└─────────────────────────────────────────────┘
```

### 1.2 主流平台接入方式对比

| 平台 | 接入方式 | 消息推送 | 特点 |
|:---|:---|:---|:---|
| **飞书** | Webhook + 事件订阅 | HTTP POST | 企业级，开放度高 |
| **钉钉** | Stream 长连接 | WebSocket | 国内普及，稳定 |
| **企微** | 回调模式 | HTTP POST | 与微信互通，生态强 |
| **Telegram** | Bot API + Webhook | HTTP POST | 国际化，简单 |

### 1.3 统一抽象层设计

**核心思路：** 不管底层是哪个平台，上层看到的是统一的消息接口。

```cpp
// 统一消息结构
struct IMMessage {
    std::string platform;      // "feishu", "dingtalk", "wecom", "telegram"
    std::string message_id;    // 平台消息 ID
    std::string user_id;       // 发送者 ID
    std::string user_name;     // 发送者昵称
    std::string chat_id;       // 群聊/私聊 ID
    bool is_group;             // 是否群聊
    std::string content;       // 消息内容（纯文本）
    std::vector<Attachment> attachments;  // 富媒体
    std::chrono::timestamp timestamp;
};

// 统一发送接口
struct IMReply {
    std::string content;
    std::vector<Attachment> attachments;
    bool markdown = false;     // 是否使用 Markdown 格式
};

// 平台适配器接口
class IMAdapter {
public:
    virtual ~IMAdapter() = default;
    virtual std::string name() const = 0;
    virtual Task<void> start() = 0;                    // 启动监听
    virtual Task<void> stop() = 0;                     // 停止
    virtual Task<void> send(const IMReply& reply) = 0; // 发送消息
    
    // 设置消息回调
    void on_message(std::function<Task<void>(const IMMessage&)> cb) {
        message_callback_ = std::move(cb);
    }
    
protected:
    std::function<Task<void>(const IMMessage&)> message_callback_;
};
```

---

## 第二部分：飞书接入

### 2.1 飞书机器人创建

1. 进入飞书开放平台 → 创建企业自建应用
2. 添加 "机器人" 能力
3. 获取 **App ID** 和 **App Secret**
4. 配置事件订阅 URL

### 2.2 飞书适配器实现

```cpp
class FeishuAdapter : public IMAdapter {
public:
    FeishuAdapter(asio::io_context& io, 
                  const Config& config,
                  const std::shared_ptr<Router>& router);
    
    std::string name() const override { return "feishu"; }
    
    Task<void> start() override {
        // 1. 启动 HTTP 服务器接收飞书事件推送
        co_await start_http_server();
        
        // 2. 获取 tenant_access_token
        co_await refresh_token();
        
        // 3. 启动定时刷新 token
        token_refresh_timer_.start();
    }
    
    Task<void> send(const IMReply& reply) override {
        // 调用飞书消息发送 API
        auto req = build_send_request(reply);
        co_await http_client_.post(
            "https://open.feishu.cn/open-apis/im/v1/messages",
            req
        );
    }

private:
    // 处理飞书推送的事件
    Task<void> handle_event(const json& event) {
        if (event["header"]["event_type"] == "im.message.receive_v1") {
            auto msg = parse_message(event["event"]["message"]);
            
            // 转换为统一消息格式
            IMMessage im_msg{
                .platform = "feishu",
                .message_id = msg["message_id"],
                .user_id = msg["sender"]["sender_id"]["open_id"],
                .user_name = msg["sender"]["sender_id"]["open_id"], // 需要额外查询
                .chat_id = msg["chat_id"],
                .is_group = msg["chat_type"] == "group",
                .content = extract_text_content(msg),
            };
            
            // 调用上层回调
            if (message_callback_) {
                co_await message_callback_(im_msg);
            }
        }
    }
    
    // 飞书消息内容解析（处理富文本）
    std::string extract_text_content(const json& msg) {
        auto content_type = msg["content"];
        if (content_type.is_string()) {
            // 纯文本或 JSON 字符串
            auto content = json::parse(content_type.get<std::string>());
            return content.value("text", "");
        }
        return "";
    }
};
```

### 2.3 飞书特殊处理

**@提及识别：**

```cpp
// 检查消息是否 @ 了机器人
bool is_mentioned(const json& msg, const std::string& bot_open_id) {
    auto mentions = msg.value("mentions", json::array());
    for (const auto& m : mentions) {
        if (m["id"]["open_id"] == bot_open_id) {
            return true;
        }
    }
    return false;
}
```

---

## 第三部分：钉钉接入

### 3.1 钉钉机器人类型

- **企业内部机器人**：需要企业权限，功能最全
- **群机器人**：Webhook 方式，最简单

### 3.2 钉钉 Stream 模式

钉钉推荐使用 WebSocket 长连接（Stream 模式）接收消息：

```cpp
class DingtalkAdapter : public IMAdapter {
public:
    Task<void> start() override {
        // 钉钉使用 WebSocket 长连接
        ws_client_.on_message([this](const std::string& data) {
            auto msg = json::parse(data);
            handle_stream_message(msg);
        });
        
        // 连接钉钉 Stream 服务器
        auto endpoint = co_await get_stream_endpoint();
        co_await ws_client_.connect(endpoint);
    }
    
    Task<void> send(const IMReply& reply) override {
        // 钉钉支持多种消息类型
        json payload = {
            {"msgtype", reply.markdown ? "markdown" : "text"},
            {"text", {{"content", reply.content}}}
        };
        
        // 发送到钉钉机器人 Webhook
        co_await http_client_.post(webhook_url_, payload);
    }

private:
    // Stream 消息处理
    void handle_stream_message(const json& msg) {
        if (msg["header"]["event_type"] == "chat.group.message.receive") {
            IMMessage im_msg{
                .platform = "dingtalk",
                .message_id = msg["message_id"],
                .user_id = msg["sender_staff_id"],
                .chat_id = msg["conversation_id"],
                .is_group = true,
                .content = msg["text"]["content"],
            };
            
            // 钉钉需要检查是否被 @
            if (is_bot_mentioned(im_msg.content)) {
                message_callback_(im_msg);
            }
        }
    }
};
```

---

## 第四部分：多平台路由

### 4.1 平台管理器

```cpp
class IMPlatformManager {
public:
    void register_adapter(std::shared_ptr<IMAdapter> adapter) {
        adapters_[adapter->name()] = std::move(adapter);
    }
    
    Task<void> start_all() {
        std::vector<Task<void>> tasks;
        for (auto& [name, adapter] : adapters_) {
            tasks.push_back(adapter->start());
        }
        co_await asyncio::gather(tasks);
    }
    
    // 发送到指定平台
    Task<void> send_to(const std::string& platform, 
                      const std::string& chat_id,
                      const IMReply& reply) {
        auto it = adapters_.find(platform);
        if (it != adapters_.end()) {
            co_await it->second->send(reply);
        }
    }
    
    // 广播到所有平台
    Task<void> broadcast(const IMReply& reply) {
        for (auto& [name, adapter] : adapters_) {
            co_await adapter->send(reply);
        }
    }

private:
    std::unordered_map<std::string, std::shared_ptr<IMAdapter>> adapters_;
};
```

### 4.2 与 Agent 核心集成

```cpp
// 在 main.cpp 中
int main() {
    // ... 原有初始化 ...
    
    // 创建 IM 平台管理器
    IMPlatformManager im_manager;
    
    // 注册飞书适配器
    auto feishu = std::make_shared<FeishuAdapter>(io, config);
    im_manager.register_adapter(feishu);
    
    // 注册钉钉适配器
    auto dingtalk = std::make_shared<DingtalkAdapter>(io, config);
    im_manager.register_adapter(dingtalk);
    
    // 设置消息处理回调（连接到 Agent）
    feishu->on_message([&agent_engine](const IMMessage& msg) -> Task<void> {
        // 构建 Agent 输入
        AgentInput input{
            .session_id = msg.chat_id,
            .user_id = msg.user_id,
            .message = msg.content,
            .context = load_context(msg.chat_id),  // 加载历史上下文
        };
        
        // 调用 Agent 处理
        auto response = co_await agent_engine.process(input);
        
        // 发送回复
        IMReply reply{.content = response.content};
        co_await feishu->send(reply);
        
        // 保存上下文
        save_context(msg.chat_id, response.new_context);
    });
    
    // 启动所有平台
    co_await im_manager.start_all();
    
    io.run();
}
```

---

## 第五部分：实战配置

### 5.1 配置文件

```yaml
# config/im.yaml
im:
  platforms:
    feishu:
      enabled: true
      app_id: "cli_xxxxx"
      app_secret: "${FEISHU_SECRET}"  # 从环境变量读取
      encrypt_key: "xxxx"
      verification_token: "xxxx"
      webhook_port: 8081
      
    dingtalk:
      enabled: true
      app_key: "dingxxxxx"
      app_secret: "${DINGTALK_SECRET}"
      
    telegram:
      enabled: false
      bot_token: "${TG_BOT_TOKEN}"
      webhook_url: "https://your-domain.com/webhook/telegram"
```

### 5.2 环境变量设置

```bash
# .env
export FEISHU_SECRET="your-feishu-app-secret"
export DINGTALK_SECRET="your-dingtalk-app-secret"
export TG_BOT_TOKEN="your-telegram-bot-token"
```

---

## 运行测试

### 飞书测试

1. 在飞书群中添加你的机器人
2. @机器人发送消息：
   ```
   @你的机器人 你好，帮我查一下今天的天气
   ```
3. 机器人应该回复处理结果

### 钉钉测试

1. 在钉钉群中配置机器人 Webhook
2. 发送消息并 @机器人
3. 观察机器人响应

---

## 常见问题

### Q: 飞书事件订阅验证失败

**原因：** 飞书要求验证 URL 的合法性

**解决：**
```cpp
// 在 HTTP handler 中处理 challenge
if (body.contains("challenge")) {
    json response = {{"challenge", body["challenge"]}};
    res.set_body(response.dump());
}
```

### Q: 钉钉收不到消息

**原因：** 需要在钉钉开放平台配置 IP 白名单

**解决：** 在钉钉开放平台 → 应用详情 → 服务器出口 IP 中添加你的服务器 IP

### Q: 多平台消息重复处理

**解决：** 使用 message_id 去重

```cpp
class MessageDeduplicator {
public:
    bool is_duplicate(const std::string& msg_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (seen_.count(msg_id)) return true;
        seen_.insert(msg_id);
        if (seen_.size() > 10000) seen_.clear();  // 定期清理
        return false;
    }
private:
    std::unordered_set<std::string> seen_;
    std::mutex mutex_;
};
```

---

## 下一步

→ **Step 16: 期中实战案例**

我们将综合运用前面所学，构建一个完整的**智能客服机器人**，支持：
- 多轮对话上下文
- 知识库检索回答
- 工单创建工具
- 飞书/钉钉双平台接入
