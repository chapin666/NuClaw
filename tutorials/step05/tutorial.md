# Step 5: Agent 核心 — LLM 接入与工具调用

> 目标：接入真实 AI 能力，实现能理解、会思考、可调用的 Agent
> 
> 难度：⭐⭐⭐ | 代码量：~450行 | 预计：3-4小时

---

## 问题引入

### Step 4 的问题

服务器只能做简单的消息转发：

```
用户: 今天北京天气怎么样？

方案 1 - 规则匹配：
  if (包含"天气") return "晴天";  ← 硬编码，数据不真实
  
方案 2 - 需要实时数据：
  必须调用天气 API
  但用户可能问："适合出门吗？"、"外面如何？"
  规则无法覆盖所有问法！
```

**我们需要：**
1. **语义理解** — 不只是关键词匹配
2. **工具调用** — 调用外部 API 获取真实数据
3. **Agent Loop** — 理解 → 决策 → 执行 → 回复

---

## 解决方案

### LLM（大语言模型）

**核心能力：**
- **理解语义**：理解意图，不只是匹配关键词
- **推理规划**：分解复杂任务，制定执行步骤
- **知识整合**：整合信息生成合理回复

### Function Calling 机制

```
用户: 今天北京天气怎么样？

Step 1: LLM 理解意图
  "用户想了解北京今天的天气"

Step 2: LLM 判断需要调用工具
  "我没有实时天气数据，需要调用 get_weather"

Step 3: LLM 生成工具调用
  { "tool": "get_weather", "arguments": {"city": "北京"} }

Step 4: 服务器执行工具
  result = get_weather(city="北京")
  // 返回："晴天，25°C，空气质量良好"

Step 5: LLM 整合结果生成回复
  "北京今天晴天，25°C，空气质量良好，适合出门！"
```

### Agent Loop

```
┌─────────────────────────────────────────────────────────────┐
│                        Agent Loop                            │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│   ┌────────┐    ┌────────┐    ┌────────┐    ┌────────┐    │
│   │  感知  │───▶│  理解  │───▶│  决策  │───▶│  执行  │    │
│   │        │    │        │    │        │    │        │    │
│   │用户输入│    │LLM解析 │    │调用工具│    │获取数据│    │
│   └────────┘    └────┬───┘    └───┬────┘    └───┬────┘    │
│                      │            │             │         │
│                      │ No         │ Yes         │         │
│                      ▼            ▼             ▼         │
│                 ┌────────┐    ┌─────────────────────┐     │
│                 │直接回复│    │  整合结果生成回复    │     │
│                 └───┬────┘    └──────────┬──────────┘     │
│                     │                    │                │
│                     └────────────────────┘                │
│                                  │                        │
│                                  ▼                        │
│                           返回给用户                       │
└─────────────────────────────────────────────────────────────┘
```

---

## 核心代码

### HTTP 客户端

```cpp
class HttpClient {
public:
    // 同步 POST（简化版）
    string post(const string& host, const string& target, 
                const string& body, const map<string,string>& headers) {
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
};
```

### 工具定义

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

### LLM 客户端

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

    HttpClient& http_;
    string api_key_;
};
```

### Agent Session

```cpp
class AgentSession : public enable_shared_from_this<AgentSession> {
public:
    AgentSession(tcp::socket socket, LLMClient& llm)
        : ws_(move(socket)), llm_(llm) {}

    void on_message(const string& message) {
        // 解析用户消息
        json j = json::parse(message);
        string content = j.value("content", "");
        
        // 添加到历史
        history_.push_back({
            {"role", "user"},
            {"content", content}
        });
        
        // 处理
        process();
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
            return get_weather(args["city"]);
        }
        return "Unknown tool";
    }
    
    void add_tool_call(const string& name, 
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
                        {"name", name},
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
    vector<json> history_;
};
```

---

## 架构图

### Step 4 vs Step 5 对比

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
│   LLM Client    │  ← 理解意图
│  - 语义分析     │
│  - 决策调用     │
└────────┬────────┘
         │
    ┌────┴────┐
    │         │
无需工具    需要工具
    │         │
    ▼         ▼
直接回复  ┌──────────┐
    │     │ Tool Call│ ← get_weather
    │     └────┬─────┘
    │          │
    │     ┌────┴────┐
    │     │Weather  │ ← 调用天气 API
    │     │  API     │
    │     └────┬─────┘
    │          │
    │     ┌────┴────┐
    │     │ LLM整合 │ ← 生成自然语言回复
    │     └────┬─────┘
    │          │
    └────┬─────┘
         │
         ▼
     返回用户
```

---

## 交互示例

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

## 完整代码获取

由于篇幅限制，完整代码请参考 GitHub 仓库：

```bash
git clone https://github.com/chapin666/NuClaw.git
cd NuClaw/src/step05
```

---

## 本章总结

- ✅ 接入 LLM 实现语义理解
- ✅ Function Calling（工具调用）
- ✅ Agent Loop 完整流程

---

## 课后思考

我们的 Agent 现在能对话和调用工具了，但还有几个问题：

### 问题 1：对话历史无限增长

```
对话 100 轮后，历史消息变得非常长
每次调用 LLM 都要带上全部历史
Token 费用越来越高...
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

这些问题的解决方案是什么？

<details>
<summary>点击查看后续章节 💡</summary>

**Step 6: 会话管理** — 用户隔离和上下文管理

**Step 7: 短期记忆** — 窗口管理和摘要生成

**Step 8: 长期记忆** — 向量数据库和 RAG

**Step 9: 多 Agent 协作** — 复杂任务分解

最终构建一个真正有"记忆"、能"思考"的 Agent！

</details>

---

**恭喜完成 Step 1-5！** 你已经掌握了构建 AI Agent 的核心技术：
- 异步 I/O 高性能服务器
- HTTP/WebSocket 通信
- JSON 数据交互
- LLM 接入和工具调用

下一章开始，我们将让 Agent 拥有"记忆"。
