# Step 5: Agent 核心 — LLM 接入与工具调用

> 目标：接入真实 AI 能力，实现能理解、会思考、可调用的 Agent
> 
> 难度：⭐⭐⭐ (中等)
> 
> 代码量：约 450 行（较 Step 4 新增约 100 行）
> 
> 预计学习时间：3-4 小时

---

## 问题引入

**Step 4 的问题：**

我们的服务器只能做简单的消息转发，无法智能回答：

```
用户: 今天北京天气怎么样？

方案 1 - 规则匹配（Step 3）：
  if (包含"天气") return "晴天";  ← 硬编码，数据不真实

方案 2 - 需要实时数据：
  必须调用天气 API 获取真实数据
  但用户可能问："适合出门吗？"、"外面如何？"、"要带伞吗？"
  这些意思相同，但规则无法覆盖所有问法！
```

**本章目标：**
1. 接入 LLM（大语言模型），实现**语义理解**（不只是关键词匹配）
2. 实现 **Function Calling**，让 LLM 能够调用外部工具
3. 完成 **Agent Loop**：理解 → 决策 → 执行 → 回复

---

## 解决方案

### 什么是 LLM？

**LLM（Large Language Model）** 是基于深度学习的自然语言处理模型：

**核心能力：**
- **理解语义**：不只是匹配关键词，而是理解意图
- **推理规划**：分解复杂任务，制定执行步骤
- **知识整合**：整合信息生成合理回复

**代表模型：**
| 模型 | 厂商 | 特点 |
|:---|:---|:---|
| GPT-4 | OpenAI | 推理能力强，Function Calling 支持好 |
| Claude | Anthropic | 长文本处理优秀 |
| 文心一言 | 百度 | 中文理解好 |
| 通义千问 | 阿里 | 国内可用，中文优化 |

### Function Calling 机制

Function Calling 让 LLM 能够**调用外部工具**获取数据：

```
用户: 今天北京天气怎么样？

Step 1: LLM 理解意图
  "用户想了解北京今天的天气"

Step 2: LLM 判断需要调用工具
  "我没有实时天气数据，需要调用 get_weather 工具"

Step 3: LLM 生成工具调用
  {
    "tool": "get_weather",
    "arguments": {"city": "北京"}
  }

Step 4: 服务器执行工具
  result = get_weather(city="北京")
  // 返回："晴天，25°C，空气质量良好"

Step 5: LLM 整合结果生成回复
  "北京今天晴天，25°C，空气质量良好，适合出门！"
```

### Agent Loop

完整的 Agent 工作流程：

```
┌─────────────────────────────────────────────────────────────────┐
│                         Agent Loop                              │
│                                                                 │
│   ┌─────────┐    ┌─────────┐    ┌─────────┐    ┌─────────┐    │
│   │  感知   │───▶│  理解   │───▶│  决策   │───▶│  执行   │    │
│   │         │    │         │    │         │    │         │    │
│   │用户输入 │    │LLM解析  │    │是否调用 │    │调用工具 │    │
│   │         │    │意图     │    │工具？   │    │获取数据 │    │
│   └─────────┘    └─────────┘    └────┬────┘    └────┬────┘    │
│        ▲                             │              │         │
│        │                             │ No           │         │
│        │                             ▼              ▼         │
│        │                        ┌─────────────────────────┐    │
│        │                        │        直接回复         │    │
│        │                        └────────────┬────────────┘    │
│        │                                     │                 │
│        │                        ┌────────────┘                 │
│        │                        │ Yes                          │
│        │                        ▼                              │
│        │            ┌──────────────────────┐                   │
│        │            │  生成最终回复        │                   │
│        │            │  (整合工具结果)      │                   │
│        │            └──────────┬───────────┘                   │
│        │                       │                              │
│        └───────────────────────┘                              │
│                              返回给用户                        │
└─────────────────────────────────────────────────────────────────┘
```

---

## 核心概念详解

### 提示工程（Prompt Engineering）

提示是 LLM 的输入，好的提示能让 LLM 表现更好：

```
系统提示（System Prompt）：
─────────────────────────
你是一个天气助手，帮助用户查询天气信息。

你可以使用以下工具：
1. get_weather(city: string) - 获取指定城市天气

回复格式：
- 如果有工具可用，请使用工具
- 工具返回后，用自然语言整合回复

用户输入：今天北京天气怎么样？
─────────────────────────

LLM 回复：
我会帮你查询北京天气。

工具调用：get_weather(city="北京")
```

### 工具定义格式

告诉 LLM 有哪些工具可用：

```json
{
    "type": "function",
    "function": {
        "name": "get_weather",
        "description": "获取指定城市的当前天气",
        "parameters": {
            "type": "object",
            "properties": {
                "city": {
                    "type": "string",
                    "description": "城市名称，如北京、上海"
                }
            },
            "required": ["city"]
        }
    }
}
```

---

## 代码对比

### Step 4 vs Step 5 架构对比

**Step 4 架构（消息转发）：**
```
用户消息 ──▶ 服务器 ──▶ 广播给所有客户端
     ↑                        ↓
     └────────────────────────┘
           纯转发，无处理
```

**Step 5 架构（智能 Agent）：**
```
用户消息
    │
    ▼
┌─────────────────┐
│   LLM Client    │
│  - 理解意图     │
│  - 决策是否调用 │
└────────┬────────┘
         │
         ├──▶ 无需工具 ──▶ 直接回复
         │
         └──▶ 需要工具
                │
                ▼
         ┌──────────────┐
         │  Tool Call   │
         │ get_weather  │
         └──────┬───────┘
                │
                ▼
         ┌──────────────┐
         │ Weather API  │
         └──────┬───────┘
                │
                ▼
         ┌──────────────┐
         │  LLM 整合    │
         │ 生成自然回复 │
         └──────┬───────┘
                │
                ▼
            返回用户
```

### 代码修改详解

**Step 4 的关键代码：**
```cpp
// 简单消息转发
void on_message(const std::string& message) {
    json j = json::parse(message);
    std::string content = j.value("content", "");
    // 直接广播，没有智能处理
    manager_.broadcast(content, nullptr);
}
```

**Step 5 的修改：**

```cpp
// 新增：异步 HTTP 客户端
class HttpClient {
public:
    HttpClient(asio::io_context& io) : resolver_(io) {}
    
    // 同步 POST（简化版，生产环境应使用异步）
    std::string post(const std::string& host, 
                     const std::string& target, 
                     const std::string& body,
                     const std::map<std::string, std::string>& headers = {}) {
        tcp::resolver resolver(resolver_.get_executor());
        tcp::socket socket(resolver_.get_executor());
        
        auto results = resolver.resolve(host, "443");
        socket.connect(*results.begin());
        
        // 构造 HTTP 请求
        std::string request = "POST " + target + " HTTP/1.1\r\n";
        request += "Host: " + host + "\r\n";
        request += "Content-Type: application/json\r\n";
        for (const auto& [k, v] : headers) {
            request += k + ": " + v + "\r\n";
        }
        request += "Content-Length: " + std::to_string(body.size()) + "\r\n";
        request += "\r\n" + body;
        
        socket.write_some(asio::buffer(request));
        
        char response[4096];
        size_t len = socket.read_some(asio::buffer(response));
        
        std::string resp_str(response, len);
        // 提取 body（跳过 HTTP 头部）
        auto pos = resp_str.find("\r\n\r\n");
        if (pos != std::string::npos) {
            return resp_str.substr(pos + 4);
        }
        return resp_str;
    }

private:
    tcp::resolver resolver_;
};

// 新增：工具定义
struct Tool {
    std::string name;
    std::string description;
    json parameters;
};

// 新增：LLM 响应
struct LLMResponse {
    std::string content;
    bool has_tool_call = false;
    std::string tool_name;
    json tool_args;
};

// 新增：LLM 客户端
class LLMClient {
public:
    LLMClient(HttpClient& http, const std::string& api_key)
        : http_(http), api_key_(api_key) {}
    
    LLMResponse chat(const std::vector<json>& messages, 
                    const std::vector<Tool>& tools) {
        // 构造 OpenAI API 请求
        json request;
        request["model"] = "gpt-3.5-turbo";
        request["messages"] = messages;
        
        if (!tools.empty()) {
            json tools_json = json::array();
            for (const auto& tool : tools) {
                tools_json.push_back({
                    {"type", "function"},
                    {"function", {
                        {"name", tool.name},
                        {"description", tool.description},
                        {"parameters", tool.parameters}
                    }}
                });
            }
            request["tools"] = tools_json;
            request["tool_choice"] = "auto";
        }
        
        // 发送请求
        std::map<std::string, std::string> headers = {
            {"Authorization", "Bearer " + api_key_}
        };
        
        std::string response = http_.post(
            "api.openai.com", 
            "/v1/chat/completions", 
            request.dump(),
            headers
        );
        
        return parse_response(response);
    }

private:
    LLMResponse parse_response(const std::string& response) {
        try {
            json j = json::parse(response);
            LLMResponse result;
            
            auto& message = j["choices"][0]["message"];
            result.content = message.value("content", "");
            
            // 检查是否有工具调用
            if (message.contains("tool_calls")) {
                auto& tool_call = message["tool_calls"][0];
                result.has_tool_call = true;
                result.tool_name = tool_call["function"]["name"];
                result.tool_args = json::parse(
                    tool_call["function"]["arguments"].get<std::string>()
                );
            }
            
            return result;
        } catch (...) {
            return LLMResponse{.content = "Error parsing response"};
        }
    }

    HttpClient& http_;
    std::string api_key_;
};

// 新增：天气工具
class WeatherTool {
public:
    std::string get_weather(const std::string& city) {
        // 模拟天气数据（真实场景应调用天气 API）
        static std::map<std::string, std::string> data = {
            {"北京", "晴天，25°C，空气质量良好"},
            {"上海", "多云，22°C，微风"},
            {"广州", "小雨，28°C，湿度较高"}
        };
        
        auto it = data.find(city);
        if (it != data.end()) {
            return it->second;
        }
        return "未知城市";
    }
};

// 修改：智能 Agent Session
class AgentSession : public std::enable_shared_from_this<AgentSession> {
public:
    AgentSession(tcp::socket socket, LLMClient& llm, WeatherTool& weather)
        : ws_(std::move(socket)), llm_(llm), weather_(weather) {}

    void start() {
        do_accept();
    }

private:
    void do_accept() {
        // WebSocket 握手（同 Step 4）
        // ...
    }
    
    void on_message(const std::string& message) {
        try {
            json j = json::parse(message);
            std::string content = j.value("content", "");
            
            // 添加到对话历史
            history_.push_back({
                {"role", "user"},
                {"content", content}
            });
            
            // 处理
            process();
        } catch (...) {
            send("{\"error\":\"invalid message\"}");
        }
    }
    
    void process() {
        // 定义可用工具
        std::vector<Tool> tools = {
            {
                "get_weather",
                "获取指定城市的天气信息",
                {
                    {"type", "object"},
                    {"properties", {
                        {"city", {
                            {"type", "string"},
                            {"description", "城市名称，如北京、上海"}
                        }}
                    }},
                    {"required", json::array({"city"})}
                }
            }
        };
        
        // 调用 LLM
        LLMResponse response = llm_.chat(history_, tools);
        
        if (response.has_tool_call) {
            // 执行工具
            std::string tool_result;
            if (response.tool_name == "get_weather") {
                std::string city = response.tool_args["city"];
                tool_result = weather_.get_weather(city);
            }
            
            // 将工具结果添加到历史
            history_.push_back({
                {"role", "assistant"},
                {"content", nullptr},
                {"tool_calls", json::array({
                    {
                        {"id", "call_1"},
                        {"type", "function"},
                        {"function", {
                            {"name", response.tool_name},
                            {"arguments", response.tool_args.dump()}
                        }}
                    }
                })}
            });
            
            history_.push_back({
                {"role", "tool"},
                {"tool_call_id", "call_1"},
                {"content", tool_result}
            });
            
            // 再次调用 LLM 生成最终回复
            LLMResponse final_response = llm_.chat(history_, tools);
            send_reply(final_response.content);
        } else {
            send_reply(response.content);
        }
    }
    
    void send_reply(const std::string& content) {
        history_.push_back({
            {"role", "assistant"},
            {"content", content}
        });
        
        json reply;
        reply["type"] = "message";
        reply["content"] = content;
        send(reply.dump());
    }

    beast::websocket::stream<tcp::socket> ws_;
    LLMClient& llm_;
    WeatherTool& weather_;
    std::vector<json> history_;
};
```

---

## 完整源码（简化版）

由于篇幅限制，这里展示核心逻辑：

```cpp
// 主要组件：
// 1. HttpClient - HTTP 客户端
// 2. LLMClient - LLM API 封装
// 3. WeatherTool - 天气工具
// 4. AgentSession - 智能会话

// 完整代码参考 GitHub 仓库
// https://github.com/chapin666/NuClaw

// 核心流程伪代码：
void agent_loop() {
    // 1. 接收用户输入
    user_input = receive_message();
    
    // 2. 调用 LLM
    llm_response = llm.chat(history, available_tools);
    
    // 3. 检查是否需要调用工具
    if (llm_response.has_tool_call) {
        // 执行工具
        result = execute_tool(llm_response.tool_name, 
                             llm_response.tool_args);
        
        // 将结果返回 LLM
        history.add_tool_result(result);
        
        // 再次调用 LLM 生成最终回复
        final_response = llm.chat(history, tools);
        send(final_response.content);
    } else {
        // 直接回复
        send(llm_response.content);
    }
}
```

---

## 交互示例

```
用户: 你好！
Agent: 你好！很高兴见到你。有什么我可以帮助你的吗？

用户: 今天北京天气怎么样？
Agent: [思考] 用户问天气，需要调用 get_weather 工具
Agent: [调用] get_weather(city="北京")
Agent: [结果] 晴天，25°C，空气质量良好
Agent: [回复] 北京今天晴天，25°C，空气质量良好，适合出门！

用户: 那上海呢？
Agent: [思考] 用户问"那"，结合上文知道是问天气
Agent: [调用] get_weather(city="上海")
Agent: [结果] 多云，22°C，微风
Agent: [回复] 上海今天多云，22°C，有微风，也很舒适。
```

---

## 本章总结

- ✅ 解决了 Step 4 的"不够智能"问题
- ✅ 接入 LLM 实现语义理解
- ✅ 实现 Function Calling（工具调用）
- ✅ 掌握 Agent Loop 完整流程
- ✅ 代码从 350 行扩展到 450 行

---

## 课后思考

我们的 Agent 现在已经能对话和调用工具了，但还有几个明显问题：

### 问题 1：对话历史无限增长

```
对话 100 轮后，历史消息变得非常长
每次调用 LLM 都要带上全部历史
Token 费用越来越高...
Token 限制（如 4096）可能超出
```

### 问题 2：没有长期记忆

```
用户: 我叫小明
Agent: 你好小明！
...
[关闭连接，重新连接]
用户: 我叫什么？
Agent: 抱歉，我不知道...  ← 记忆丢失了！
```

### 问题 3：没有用户隔离

```
多个用户同时连接
他们共享同一个对话历史？
A 用户的消息被 B 用户看到？
```

### 问题 4：上下文太长怎么办？

```
用户上传了一篇 10 万字的文档
LLM 无法处理这么长的上下文
如何解决这个问题？
```

这些问题的解决方案是什么？

<details>
<summary>点击查看后续章节 💡</summary>

**后续章节预告：**

**Step 6: 会话管理** — 用户隔离和上下文管理
- Session ID 机制
- 每个用户独立的历史

**Step 7: 短期记忆** — 窗口管理和摘要生成
- 滑动窗口：只保留最近 N 轮
- 对话摘要：压缩历史信息

**Step 8: 长期记忆** — 向量数据库和 RAG
- 向量存储：语义记忆检索
- RAG：检索增强生成

**Step 9: 多 Agent 协作** — 复杂任务分解
- 多 Agent 协作架构
- 任务分配和结果汇总

最终构建一个真正有"记忆"、能"思考"的 Agent！

</details>

---

**恭喜完成 Step 1-5！** 你已经掌握了构建 AI Agent 的核心技术：
- 异步 I/O 高性能服务器
- HTTP/WebSocket 通信
- JSON 数据交互
- LLM 接入和工具调用

下一章开始，我们将让 Agent 拥有"记忆"。
