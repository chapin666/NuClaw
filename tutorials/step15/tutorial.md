# Step 15: IM 平台接入 —— 让 Agent 走进真实世界

> 目标：实现飞书/钉钉/企微/Telegram Bot 接入，让 Agent 能在真实 IM 平台运行
> 
003e 难度：⭐⭐⭐⭐ | 代码量：约 1100 行 | 预计学习时间：4-5 小时

---

## 一、为什么要接入 IM 平台？

### 1.1 现状问题

目前 Agent 只能通过 HTTP API 或 WebSocket 交互：

```
用户 ──▶ curl/postman ──▶ Agent Server ──▶ LLM API
      
问题：
• 普通用户不会用 curl
• 没有消息推送能力
• 缺乏富媒体交互（图片、卡片、@提及）
• 无法融入用户日常工作流
```

### 1.2 IM 平台的价值

```
接入 IM 平台后：

飞书/钉钉/企微/Telegram
         │
         │ Webhook / Bot API
         ▼
    ┌─────────┐
    │  Bot    │ ◀── 用户 @Bot 提问
    │ Adapter │ ──▶ 推送消息给用户
    └────┬────┘
         │
         ▼
    ┌─────────┐
    │  Agent  │
    │  Core   │
    └─────────┘

价值：
• 用户零门槛使用（在熟悉的 IM 里聊天）
• 主动推送能力（任务完成通知、日报）
• 富媒体交互（图片、文件、卡片）
• 群聊协作（@Bot 提问，全员可见）
```

---

## 二、IM 平台架构设计

### 2.1 通用架构

```
┌─────────────────────────────────────────────────────────────────┐
│                     IM 平台接入架构                              │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐           │
│  │  飞书   │  │  钉钉   │  │  企微   │  │ Telegram│           │
│  │ Feishu  │  │ DingTalk│  │ WeCom   │  │         │           │
│  └────┬────┘  └────┬────┘  └────┬────┘  └────┬────┘           │
│       │            │            │            │                  │
│       └────────────┴────────────┴────────────┘                  │
│                          │                                       │
│                    HTTP / WebSocket                             │
│                          │                                       │
│       ┌──────────────────┼──────────────────┐                   │
│       │                  │                  │                   │
│       ▼                  ▼                  ▼                   │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐              │
│  │FeishuAdapter│  │DingAdapter  │  │Telegram     │              │
│  │             │  │             │  │Adapter      │              │
│  │• 验签       │  │• 验签       │  │• Webhook    │              │
│  │• 消息解析   │  │• 消息解析   │  │• 长轮询     │              │
│  │• 消息发送   │  │• 消息发送   │  │• 消息发送   │              │
│  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘              │
│         │                │                │                     │
│         └────────────────┼────────────────┘                     │
│                          │                                       │
│                    ┌─────┴─────┐                                 │
│                    │IMAdapter  │                                 │
│                    │Interface  │                                 │
│                    └─────┬─────┘                                 │
│                          │                                       │
│                          ▼                                       │
│                    ┌─────────────┐                               │
│                    │  Agent Core │                               │
│                    │             │                               │
│                    │• Session    │                               │
│                    │• Memory     │                               │
│                    │• Tools      │                               │
│                    └─────────────┘                               │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 2.2 核心接口设计

```cpp
// 通用消息结构
struct IMMessage {
    std::string platform;       // feishu/dingtalk/wecom/telegram
    std::string message_id;     // 平台消息 ID
    std::string chat_id;        // 会话 ID（单聊/群聊）
    std::string user_id;        // 发送者 ID
    std::string user_name;      // 发送者昵称
    std::string content;        // 文本内容
    std::vector<Attachment> attachments;  // 附件
    bool is_group = false;      // 是否群聊
    bool is_at_me = false;      // 是否 @Bot
    std::chrono::timestamp timestamp;
};

// 回复消息结构
struct IMReply {
    enum Type { TEXT, MARKDOWN, CARD, IMAGE } type = TEXT;
    std::string content;
    json card_data;             // 卡片数据
    std::vector<ActionButton> buttons;  // 交互按钮
};

// IM 适配器接口
class IMAdapter {
public:
    virtual ~IMAdapter() = default;
    
    // 启动/停止
    virtual void start() = 0;
    virtual void stop() = 0;
    
    // 发送消息
    virtual void send_message(const std::string& chat_id,
                              const IMReply& reply,
                              std::function<void(bool)> callback) = 0;
    
    // 获取平台名称
    virtual std::string get_platform() const = 0;
    
    // 设置消息处理器
    void on_message(std::function<void(const IMMessage&)> handler) {
        message_handler_ = handler;
    }

protected:
    std::function<void(const IMMessage&)> message_handler_;
};
```

---

## 三、飞书 Bot 接入

### 3.1 飞书 Bot 工作原理

```
┌─────────────────────────────────────────────────────────────┐
│                    飞书 Bot 交互流程                          │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  用户发送消息                                                 │
│       │                                                      │
│       ▼                                                      │
│  ┌─────────┐                                                 │
│  │ 飞书服务器 │                                               │
│  └────┬────┘                                                 │
│       │ 推送事件（HTTP POST）                                  │
│       │ 签名验证: X-Lark-Signature                            │
│       ▼                                                      │
│  ┌─────────┐     解密          ┌─────────┐                   │
│  │ 你的服务器 │ ─────────────▶ │ 事件数据  │                   │
│  │  :8080   │                 │         │                   │
│  └────┬────┘                 └─────────┘                   │
│       │                                                      │
│       │ 响应 200 OK（3秒内必须返回）                           │
│       ▼                                                      │
│  ┌─────────┐                                                 │
│  │ 飞书服务器 │                                               │
│  └─────────┘                                                 │
│                                                              │
│  主动发送消息：                                               │
│  你的服务器 ──HTTP POST──▶ 飞书 API ──▶ 用户                  │
│  （需 access_token）                                          │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

### 3.2 飞书适配器实现

```cpp
class FeishuAdapter : public IMAdapter,
                      public std::enable_shared_from_this<FeishuAdapter> {
public:
    FeishuAdapter(asio::io_context& io,
                  const std::string& app_id,
                  const std::string& app_secret,
                  const std::string& encrypt_key = "")
        : io_(io), app_id_(app_id), app_secret_(app_secret),
          encrypt_key_(encrypt_key), http_client_(io) {}
    
    void start() override {
        // 获取 access_token（定时刷新）
        refresh_access_token();
        
        // 启动 HTTP 服务器接收飞书事件
        start_event_server(8080);
    }
    
    void stop() override {
        // 清理资源
    }
    
    std::string get_platform() const override { return "feishu"; }

private:
    // 验证飞书请求签名
    bool verify_signature(const std::string& signature,
                          const std::string& timestamp,
                          const std::string& nonce,
                          const std::string& body) {
        // 飞书签名算法：
        // signature = HMAC-SHA256(key=encrypt_key, 
        //                         message=timestamp + nonce + body)
        std::string message = timestamp + nonce + body;
        std::string computed = hmac_sha256(encrypt_key_, message);
        return signature == computed;
    }
    
    // 解密事件数据（AES-CBC）
    std::string decrypt_event(const std::string& encrypted_data) {
        if (encrypt_key_.empty()) return encrypted_data;
        
        // AES-256-CBC 解密
        return aes_decrypt(encrypted_data, derive_key(encrypt_key_));
    }
    
    // 处理飞书事件
    void handle_event(const json& event) {
        std::string event_type = event["header"]["event_type"];
        
        if (event_type == "im.message.receive_v1") {
            // 收到消息
            auto message = parse_message(event["event"]["message"]);
            
            if (message_handler_) {
                message_handler_(message);
            }
        }
        // 其他事件：群组加入、卡片交互等
    }
    
    // 解析飞书消息格式
    IMMessage parse_message(const json& msg) {
        IMMessage result;
        result.platform = "feishu";
        result.message_id = msg["message_id"];
        result.chat_id = msg["chat_id"];
        result.user_id = msg["sender"]["sender_id"]["open_id"];
        
        // 解析消息内容（文本、图片、富文本等）
        std::string msg_type = msg["msg_type"];
        if (msg_type == "text") {
            // 文本内容在 JSON 字符串中，需要二次解析
            json content = json::parse(msg["content"].get<std::string>());
            result.content = content["text"];
        }
        
        // 检查是否 @Bot
        if (msg.contains("mentions")) {
            for (const auto& mention : msg["mentions"]) {
                if (mention["key"].get<std::string>() == "@_user_1") {
                    result.is_at_me = true;
                    break;
                }
            }
        }
        
        return result;
    }

public:
    // 发送消息
    void send_message(const std::string& chat_id,
                      const IMReply& reply,
                      std::function<void(bool)> callback) override {
        
        json payload;
        payload["receive_id"] = chat_id;
        
        switch (reply.type) {
            case IMReply::TEXT:
                payload["msg_type"] = "text";
                payload["content"] = json{
                    {"text", reply.content}
                }.dump();
                break;
                
            case IMReply::MARKDOWN:
                payload["msg_type"] = "interactive";
                payload["content"] = build_markdown_card(reply.content);
                break;
                
            case IMReply::CARD:
                payload["msg_type"] = "interactive";
                payload["content"] = reply.card_data.dump();
                break;
        }
        
        // 调用飞书 API
        std::string url = "https://open.feishu.cn/open-apis/im/v1/messages?" +
                         "receive_id_type=chat_id";
        
        http_client_.post(url, access_token_, payload.dump(),
            [callback](HttpResponse resp) {
                callback(resp.success && resp.status_code == 200);
            }
        );
    }

private:
    asio::io_context& io_;
    std::string app_id_;
    std::string app_secret_;
    std::string encrypt_key_;
    std::string access_token_;
    HttpClient http_client_;
};
```

---

## 四、多平台管理器

```cpp
class IMPlatformManager {
public:
    void register_adapter(std::shared_ptr<IMAdapter> adapter) {
        adapters_[adapter->get_platform()] = adapter;
        
        // 设置消息处理回调
        adapter->on_message(
            [this, adapter](const IMMessage& msg) {
                handle_incoming_message(adapter->get_platform(), msg);
            }
        );
    }
    
    void start_all() {
        for (auto& [platform, adapter] : adapters_) {
            adapter->start();
        }
    }
    
    // 发送消息到指定平台
    void send(const std::string& platform,
              const std::string& chat_id,
              const IMReply& reply,
              std::function<void(bool)> callback) {
        auto it = adapters_.find(platform);
        if (it != adapters_.end()) {
            it->second->send_message(chat_id, reply, callback);
        } else {
            callback(false);
        }
    }
    
    // 广播到所有平台
    void broadcast(const IMReply& reply) {
        for (auto& [platform, adapter] : adapters_) {
            // 获取该平台的默认聊天 ID
            auto chat_id = get_default_chat_id(platform);
            adapter->send_message(chat_id, reply, [](bool) {});
        }
    }

private:
    void handle_incoming_message(const std::string& platform,
                                  const IMMessage& msg) {
        // 构建统一会话 ID
        std::string session_id = platform + ":" + msg.chat_id;
        
        // 调用 Agent 处理
        agent_core_.process(session_id, msg.user_id, msg.content,
            [this, platform, msg](const std::string& response) {
                // 发送回复
                IMReply reply{.type = IMReply::TEXT, .content = response};
                send(platform, msg.chat_id, reply, [](bool) {});
            }
        );
    }

    std::map<std::string, std::shared_ptr<IMAdapter>> adapters_;
    AgentCore agent_core_;
};
```

---

## 五、配置示例

```yaml
# im_config.yaml
platforms:
  feishu:
    enabled: true
    app_id: "${FEISHU_APP_ID}"
    app_secret: "${FEISHU_APP_SECRET}"
    encrypt_key: "${FEISHU_ENCRYPT_KEY}"  # 可选
    event_port: 8080
    
  dingtalk:
    enabled: false
    app_key: "${DING_APP_KEY}"
    app_secret: "${DING_APP_SECRET}"
    
  telegram:
    enabled: true
    bot_token: "${TG_BOT_TOKEN}"
    webhook_url: "https://your-domain.com/webhook/telegram"
    use_polling: false  # false=webhook, true=长轮询
```

---

## 六、本章小结

**核心收获：**

1. **多平台架构**：统一接口适配不同 IM 平台
2. **飞书 Bot**：事件订阅、签名验证、消息加解密
3. **消息抽象**：统一消息格式，屏蔽平台差异

---

## 七、引出的问题

### 7.1 状态管理问题

当前 Agent 是无状态的，每次对话都是新的开始：

```
用户: 你好，我叫小明
Bot: 你好小明！

用户: 我叫什么？
Bot: 抱歉，我不知道  ← 上下文丢失了！
```

**需要：** Session 状态管理、短期记忆、长期记忆。

### 7.2 情感与个性化

当前回复机械，缺乏个性：

```
用户: 我好难过
Bot: 我理解你的感受  ← 没有情感温度
```

**需要：** 情感计算、人设定义、个性化回复。

---

**下一章预告（Step 16）：**

我们将实现**Agent 状态与记忆系统**：
- 情感状态机（心情、能量、好感度）
- 短期记忆（对话上下文）
- 长期记忆（用户画像、历史记录）
- 记忆检索与衰减

Agent 已经接入 IM 平台，接下来要让它"记得"用户，"有温度"。
