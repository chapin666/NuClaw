# Step 5: Agent 核心 — LLM 接入与工具调用

> 目标：接入真实 AI 能力，实现能理解、会思考、可调用的 Agent
> 
> 难度：⭐⭐⭐ | 代码量：约 450 行 | 预计学习时间：3-4 小时

---

## 一、问题引入

### 1.1 Step 4 的问题

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

### 1.2 本章目标

1. 接入 LLM（大语言模型），实现**语义理解**（不只是关键词匹配）
2. 实现 **Function Calling**，让 LLM 能够调用外部工具
3. 完成 **Agent Loop**：理解 → 决策 → 执行 → 回复

---

## 二、LLM 基础概念

### 2.1 什么是 LLM？

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

### 2.2 OpenAI API 格式

**请求格式：**
```json
{
    "model": "gpt-3.5-turbo",
    "messages": [
        {"role": "system", "content": "你是一个天气助手"},
        {"role": "user", "content": "今天北京天气怎么样？"}
    ],
    "tools": [...]
}
```

**响应格式：**
```json
{
    "choices": [{
        "message": {
            "role": "assistant",
            "content": "我会帮你查询北京天气",
            "tool_calls": [{
                "id": "call_xxx",
                "type": "function",
                "function": {
                    "name": "get_weather",
                    "arguments": "{\"city\": \"北京\"}"
                }
            }]
        }
    }]
}
```

**角色说明：**
| 角色 | 作用 |
|:---|:---|
| `system` | 设置系统提示，定义 Agent 的行为方式 |
| `user` | 用户的输入 |
| `assistant` | AI 的回复 |
| `tool` | 工具执行结果 |

---

## 三、Function Calling 机制

### 3.1 为什么需要 Function Calling？

LLM 有知识截止日期，无法获取实时数据：
- 天气、股票、新闻等实时信息
- 用户个人数据（日历、邮件）
- 执行操作（发送邮件、创建日历事件）

**解决方案：** 让 LLM 能够调用外部工具（函数）获取数据或执行操作。

### 3.2 Function Calling 流程

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

### 3.3 工具定义格式

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

**关键字段：**
| 字段 | 说明 |
|:---|:---|
| `name` | 工具名称，字母数字下划线 |
| `description` | 工具功能描述，LLM 用它来决定是否调用 |
| `parameters` | JSON Schema 定义参数 |
| `required` | 必需参数列表 |

---

## 四、Agent Loop 详解

### 4.1 什么是 Agent Loop？

完整的 Agent 工作流程：

```
┌─────────────────────────────────────────────────────────────────┐
│                         Agent Loop                              │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│   ┌─────────┐    ┌─────────┐    ┌─────────┐    ┌─────────┐    │
│   │  感知   │───▶│  理解   │───▶│  决策   │───▶│  执行   │    │
│   │         │    │         │    │         │    │         │    │
│   │用户输入 │    │LLM解析  │    │是否调用 │    │调用工具 │    │
│   │         │    │意图     │    │工具？   │    │获取数据 │    │
│   └─────────┘    └────┬────┘    └───┬─────┘    └────┬────┘    │
│                       │             │               │         │
│                       │ No          │ Yes           │         │
│                       ▼             ▼               ▼         │
│                  ┌────────┐    ┌─────────────────────────┐    │
│                  │直接回复│    │  整合结果生成回复        │    │
│                  └───┬────┘    └──────────┬──────────────┘    │
│                      │                    │                   │
│                      └────────────────────┘                   │
│                                   │                           │
│                                   ▼                           │
│                            返回给用户                          │
└─────────────────────────────────────────────────────────────────┘
```

### 4.2 对话历史管理

```cpp
class AgentSession {
    std::vector<json> history_;  // 对话历史
    
    void on_message(const string& content) {
        // 添加用户消息
        history_.push_back({
            {"role", "user"},
            {"content", content}
        });
        
        // 调用 LLM
        process();
    }
    
    void add_tool_result(const string& tool_name, 
                         const json& args,
                         const string& result) {
        // 添加 assistant 的工具调用
        history_.push_back({
            {"role", "assistant"},
            {"content", nullptr},
            {"tool_calls", json::array({
                {
                    {"id", "call_1"},
                    {"type", "function"},
                    {"function", {
                        {"name", tool_name},
                        {"arguments", args.dump()}
                    }}
                }
            })}
        });
        
        // 添加 tool 的返回结果
        history_.push_back({
            {"role", "tool"},
            {"tool_call_id", "call_1"},
            {"content", result}
        });
    }
};
```

---

## 五、代码结构详解

### 5.1 Step 4 vs Step 5 架构对比

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
│  - 决策调用     │
└────────┬────────┘
         │
    ┌────┴────┐
    │         │
无需工具    需要工具
    │         │
    ▼         ▼
直接回复  ┌──────────┐
    │     │ Tool Call│
    │     └────┬─────┘
    │          │
    │     ┌────┴────┐
    │     │Weather  │
    │     │  API     │
    │     └────┬─────┘
    │          │
    │     ┌────┴────┐
    │     │ LLM整合 │
    │     └────┬─────┘
    │          │
    └────┬─────┘
         │
         ▼
     返回用户
```

### 5.2 HTTP 客户端

```cpp
class HttpClient {
public:
    // 同步 POST（简化版）
    string post(const string& host, 
                const string& target, 
                const string& body,
                const map<string,string>& headers) {
        tcp::resolver resolver(io_);
        tcp::socket socket(io_);
        
        auto results = resolver.resolve(host, "443");
        socket.connect(*results.begin());
        
        // 构造 HTTP 请求
        string request = "POST " + target + " HTTP/1.1\r\n";
        request += "Host: " + host + "\r\n";
        for (const auto& [k, v] : headers) {
            request += k + ": " + v + "\r\n";
        }
        request += "Content-Length: " + to_string(body.size()) + "\r\n";
        request += "\r\n" + body;
        
        socket.write_some(buffer(request));
        
        char response[4096];
        size_t len = socket.read_some(buffer(response));
        
        return extract_body(string(response, len));
    }

private:
    io_context io_;
    
    string extract_body(const string& raw) {
        auto pos = raw.find("\r\n\r\n");
        if (pos != string::npos) {
            return raw.substr(pos + 4);
        }
        return raw;
    }
};
```

### 5.3 工具定义

```cpp
struct Tool {
    string name;
    string description;
    json parameters;
};

// 天气工具定义
Tool weather_tool = {
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
        {"required", {"city"}}
    }
};
```

### 5.4 LLM 客户端

```cpp
class LLMClient {
public:
    LLMClient(HttpClient& http, const string& api_key)
        : http_(http), api_key_(api_key) {}
    
    struct Response {
        string content;
        bool has_tool_call = false;
        string tool_name;
        json tool_args;
    };
    
    Response chat(const vector<json>& messages, 
                  const vector<Tool>& tools) {
        // 构造 OpenAI API 请求
        json request = {
            {"model", "gpt-3.5-turbo"},
            {"messages", messages}
        };
        
        if (!tools.empty()) {
            request["tools"] = format_tools(tools);
            request["tool_choice"] = "auto";
        }
        
        map<string,string> headers = {
            {"Authorization", "Bearer " + api_key_}
        };
        
        string resp = http_.post("api.openai.com", 
                                  "/v1/chat/completions", 
                                  request.dump(), headers);
        
        return parse_response(resp);
    }

private:
    Response parse_response(const string& resp) {
        json j = json::parse(resp);
        Response result;
        
        auto& message = j["choices"][0]["message"];
        result.content = message.value("content", "");
        
        // 检查工具调用
        if (message.contains("tool_calls")) {
            auto& tc = message["tool_calls"][0];
            result.has_tool_call = true;
            result.tool_name = tc["function"]["name"];
            result.tool_args = json::parse(
                tc["function"]["arguments"].get<string>()
            );
        }
        
        return result;
    }
    
    json format_tools(const vector<Tool>& tools) {
        json result = json::array();
        for (const auto& t : tools) {
            result.push_back({
                {"type", "function"},
                {"function", {
                    {"name", t.name},
                    {"description", t.description},
                    {"parameters", t.parameters}
                }}
            });
        }
        return result;
    }

    HttpClient& http_;
    string api_key_;
};
```

### 5.5 天气工具实现

```cpp
class WeatherTool {
public:
    string get_weather(const string& city) {
        // 模拟天气数据（真实场景应调用天气 API）
        static map<string, string> data = {
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
```

### 5.6 Agent Session

```cpp
class AgentSession : public enable_shared_from_this<AgentSession> {
public:
    AgentSession(tcp::socket socket, LLMClient& llm, WeatherTool& weather)
        : ws_(move(socket)), llm_(llm), weather_(weather) {}

    void on_message(const string& message) {
        try {
            json j = json::parse(message);
            string content = j.value("content", "");
            
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

private:
    void process() {
        // 定义可用工具
        vector<Tool> tools = {weather_tool};
        
        // 调用 LLM
        auto response = llm_.chat(history_, tools);
        
        if (response.has_tool_call) {
            // 执行工具
            string result = execute_tool(
                response.tool_name, 
                response.tool_args
            );
            
            // 添加到历史
            add_tool_call(response.tool_name, 
                         response.tool_args, result);
            
            // 再次调用 LLM 生成最终回复
            auto final_resp = llm_.chat(history_, tools);
            send_reply(final_resp.content);
        } else {
            send_reply(response.content);
        }
    }
    
    string execute_tool(const string& name, const json& args) {
        if (name == "get_weather") {
            return weather_.get_weather(args["city"]);
        }
        return "Unknown tool";
    }
    
    void add_tool_call(const string& name, 
                       const json& args, 
                       const string& result) {
        history_.push_back({
            {"role", "assistant"},
            {"content", nullptr},
            {"tool_calls", json::array({
                {
                    {"id", "call_1"},
                    {"type", "function"},
                    {"function", {
                        {"name", name},
                        {"arguments", args.dump()}
                    }}
                }
            })}
        });
        
        history_.push_back({
            {"role", "tool"},
            {"tool_call_id", "call_1"},
            {"content", result}
        });
    }
    
    void send_reply(const string& content) {
        history_.push_back({
            {"role", "assistant"},
            {"content", content}
        });
        
        json reply = {
            {"type", "message"},
            {"content", content}
        };
        send(reply.dump());
    }

    websocket::stream<tcp::socket> ws_;
    LLMClient& llm_;
    WeatherTool& weather_;
    vector<json> history_;
};
```

---

## 六、交互示例

```
用户: 你好！

Agent: 你好！很高兴见到你。有什么我可以帮助你的吗？

─────────────────────────────────────────

用户: 今天北京天气怎么样？

Agent: [思考] 用户问天气，需要调用 get_weather 工具
Agent: [调用] get_weather(city="北京")
Agent: [结果] 晴天，25°C，空气质量良好
Agent: [回复] 北京今天晴天，25°C，空气质量良好，适合出门！

─────────────────────────────────────────

用户: 那上海呢？

Agent: [思考] 用户问"那"，结合上文知道是问天气
Agent: [调用] get_weather(city="上海")
Agent: [结果] 多云，22°C，微风
Agent: [回复] 上海今天多云，22°C，有微风，也很舒适。
```

---

## 七、本章总结

- ✅ 接入 LLM 实现语义理解
- ✅ Function Calling（工具调用）
- ✅ Agent Loop 完整流程
- ✅ 代码从 350 行扩展到 450 行

---

## 八、课后思考

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
