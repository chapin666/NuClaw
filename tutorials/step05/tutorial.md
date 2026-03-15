# Step 5: Agent 核心 —— LLM 接入与工具调用

> 目标：接入大语言模型（LLM），实现真正的语义理解和工具调用能力
> 
003e 难度：⭐⭐⭐⭐ | 代码量：约 450 行 | 预计学习时间：4-5 小时

---

## 一、为什么需要 LLM？

### 1.1 规则 AI 的天花板

Step 4 的 WebSocket + 规则 AI 能处理固定场景，但遇到复杂语义就失效：

```
规则 AI 能处理的：                      规则 AI 无法处理的：
────────────────────────────────────────────────────────────────
"北京天气"        → 命中关键词          "我想去一个不太热、
                                          人不多、有美食的海滨城市"
"几点了"          → 命中关键词          （需要综合多个条件推理）
                                          
"讲个笑话"        → 命中关键词          "帮我分析一下这份销售数据
                                          的问题"
                                          （需要理解表格内容）
                                          
"提醒我倒垃圾"    → 命中关键词          "用 Python 写个快速排序
                                          并解释时间复杂度"
                                          （需要代码生成+知识）
```

### 1.2 LLM 的核心能力

**LLM（Large Language Model）** 是基于深度学习的语言模型：

```
┌─────────────────────────────────────────────────────────────┐
│                      LLM 核心能力                            │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  1. 语义理解                                                  │
│     理解自然语言的深层含义，不只是关键词匹配                      │
│                                                             │
│  2. 推理规划                                                  │
│     将复杂任务分解为步骤，制定执行计划                          │
│                                                             │
│  3. 知识整合                                                  │
│     利用训练时学到的知识回答问题                               │
│                                                             │
│  4. 生成能力                                                  │
│     生成流畅、符合语境的文本（代码、文章、对话）                  │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

**主流 LLM 对比：**

| 模型 | 厂商 | 上下文长度 | 特点 |
|:---|:---|:---|:---|
| GPT-4 | OpenAI | 8K/128K | 推理强，Function Calling 好 |
| Claude 3 | Anthropic | 200K | 长文本处理优秀 |
| Kimi | Moonshot | 200K | 中文场景友好 |
| Llama 3 | Meta | 8K | 开源可本地部署 |

---

## 二、LLM API 基础

### 2.1 Chat Completions API

OpenAI 风格的 Chat API：

```cpp
// 请求结构
struct ChatRequest {
    std::string model = "gpt-4";  // 模型名称
    std::vector<Message> messages;  // 对话历史
    std::vector<Tool> tools;        // 可用工具（可选）
    float temperature = 0.7;         // 随机性（0-2）
    int max_tokens = 2000;          // 最大输出长度
};

struct Message {
    std::string role;      // system / user / assistant / tool
    std::string content;   // 内容
    std::string name;      // tool 调用时的工具名
};
```

**角色说明：**

| 角色 | 作用 | 示例 |
|:---|:---|:---|
| `system` | 设定 Agent 身份和行为 | "你是一个有帮助的助手" |
| `user` | 用户输入 | "北京天气怎么样？" |
| `assistant` | LLM 回复 | "北京今天晴朗..." |
| `tool` | 工具执行结果 | `{"temperature": 25}` |

### 2.2 HTTP 请求示例

```http
POST /v1/chat/completions HTTP/1.1
Host: api.openai.com
Authorization: Bearer sk-xxxxxxxxxxxx
Content-Type: application/json

{
    "model": "gpt-4",
    "messages": [
        {"role": "system", "content": "你是一个天气助手"},
        {"role": "user", "content": "北京天气怎么样？"}
    ],
    "temperature": 0.7
}
```

**响应：**

```json
{
    "id": "chatcmpl-xxx",
    "choices": [{
        "message": {
            "role": "assistant",
            "content": "北京今天天气晴朗，气温25°C..."
        },
        "finish_reason": "stop"
    }],
    "usage": {
        "prompt_tokens": 25,
        "completion_tokens": 15,
        "total_tokens": 40
    }
}
```

---

## 三、LLM 客户端实现

### 3.1 异步 HTTP 客户端

```cpp
#include <boost/beast.hpp>
#include <boost/asio/ssl.hpp>

namespace beast = boost::beast;
namespace http = beast::http;
using tcp = asio::ip::tcp;

class LLMClient {
public:
    LLMClient(asio::io_context& io, const std::string& api_key)
        : io_(io), api_key_(api_key) {}
    
    // 异步调用 LLM
    // callback 签名：void(error_code, std::string reply)
    template<typename Callback>
    void chat(const std::vector<Message>& messages, 
              Callback callback);

private:
    asio::io_context& io_;
    std::string api_key_;
};
```

### 3.2 核心实现

```cpp
template<typename Callback>
void LLMClient::chat(const std::vector<Message>& messages,
                     Callback callback) {
    
    // 1. 构造 JSON 请求体
    json request_body = {
        {"model", "gpt-4"},
        {"messages", json::array()},
        {"temperature", 0.7}
    };
    
    for (const auto& msg : messages) {
        request_body["messages"].push_back({
            {"role", msg.role},
            {"content", msg.content}
        });
    }
    
    std::string body = request_body.dump();
    
    // 2. 构造 HTTP 请求
    http::request<http::string_body> req{
        http::verb::post, "/v1/chat/completions", 11
    };
    
    req.set(http::field::host, "api.openai.com");
    req.set(http::field::authorization, "Bearer " + api_key_);
    req.set(http::field::content_type, "application/json");
    req.content_length(body.size());
    req.body() = body;
    
    // 3. 建立 SSL 连接并发送
    auto resolver = std::make_shared<tcp::resolver>(io_);
    auto ssl_ctx = std::make_shared<asio::ssl::context>(
        asio::ssl::context::tlsv12_client
    );
    auto stream = std::make_shared<beast::ssl_stream<beast::tcp_stream>>(
        io_, *ssl_ctx
    );
    
    // 解析域名
    resolver->async_resolve(
        "api.openai.com", "https",
        [this, stream, req, callback](auto ec, auto results) mutable {
            if (ec) {
                callback(ec, "");
                return;
            }
            
            // 连接
            beast::get_lowest_layer(*stream).async_connect(
                results,
                [this, stream, req, callback](auto ec) mutable {
                    if (ec) {
                        callback(ec, "");
                        return;
                    }
                    
                    // SSL 握手
                    stream->async_handshake(
                        asio::ssl::stream_base::client,
                        [this, stream, req, callback](auto ec) mutable {
                            if (ec) {
                                callback(ec, "");
                                return;
                            }
                            
                            // 发送请求
                            http::async_write(*stream, req,
                                [this, stream, callback](auto ec, size_t) mutable {
                                    if (ec) {
                                        callback(ec, "");
                                        return;
                                    }
                                    
                                    // 读取响应
                                    auto res = std::make_shared<
                                        http::response<http::string_body>
                                    >();
                                    
                                    http::async_read(*stream, buffer_, *res,
                                        [res, callback](auto ec, size_t) {
                                            if (ec) {
                                                callback(ec, "");
                                                return;
                                            }
                                            
                                            // 解析 JSON 响应
                                            try {
                                                json j = json::parse(res->body());
                                                std::string reply = j["choices"][0]
                                                    ["message"]["content"];
                                                callback(std::error_code(), reply);
                                            } catch (...) {
                                                callback(
                                                    std::make_error_code(
                                                        std::errc::bad_message
                                                    ), 
                                                    ""
                                                );
                                            }
                                        }
                                    );
                                }
                            );
                        }
                    );
                }
            );
        }
    );
}
```

---

## 四、工具调用（Function Calling）

### 4.1 为什么需要工具？

LLM 有知识截止时间，无法获取实时信息：

```
用户: "今天北京天气怎么样？"

LLM（无工具）: 
"抱歉，我无法获取实时天气信息。"

LLM（有工具）:
1. 识别需要调用天气工具
2. 调用 get_weather(location="北京")
3. 获取结果：{"temperature": 25, "condition": "晴朗"}
4. 生成回复："北京今天天气晴朗，气温25°C..."
```

### 4.2 工具定义

```cpp
struct Tool {
    std::string name;           // 工具名称
    std::string description;    // 功能描述（LLM 用）
    json parameters;            // 参数 JSON Schema
    std::function<json(const json&)> execute;  // 执行函数
};

// 天气工具示例
Tool weather_tool = {
    .name = "get_weather",
    .description = "获取指定城市的当前天气信息",
    .parameters = {
        {"type", "object"},
        {"properties", {
            {"location", {
                {"type", "string"},
                {"description", "城市名称，如北京、上海"}
            }},
            {"date", {
                {"type", "string"},
                {"description", "日期，如今天、明天"}
            }}
        }},
        {"required", {"location"}}
    },
    .execute = [](const json& params) -> json {
        std::string location = params.value("location", "");
        // 实际调用天气 API
        return {
            {"location", location},
            {"temperature", 25},
            {"condition", "晴朗"}
        };
    }
};
```

### 4.3 Agent Loop 实现

```cpp
class Agent {
public:
    Agent(LLMClient& llm) : llm_(llm) {}
    
    void register_tool(const Tool& tool) {
        tools_[tool.name] = tool;
    }
    
    // 处理用户输入
    template<typename Callback>
    void process(const std::string& user_input, Callback callback) {
        // 1. 构建系统提示
        std::vector<Message> messages = {
            {"system", build_system_prompt()},
            {"user", user_input}
        };
        
        // 2. 调用 LLM
        call_llm(messages, callback);
    }

private:
    void call_llm(std::vector<Message> messages, auto callback) {
        llm_.chat(messages, [this, messages, callback](auto ec, std::string reply) {
            if (ec) {
                callback("抱歉，服务暂时不可用");
                return;
            }
            
            // 3. 检查是否需要调用工具
            // LLM 可能返回类似：{"tool": "get_weather", "arguments": {...}}
            auto tool_call = parse_tool_call(reply);
            
            if (tool_call) {
                // 4. 执行工具
                std::string tool_name = tool_call->first;
                json arguments = tool_call->second;
                
                if (tools_.count(tool_name)) {
                    json result = tools_[tool_name].execute(arguments);
                    
                    // 5. 将工具结果加入对话历史
                    messages.push_back({"assistant", reply});
                    messages.push_back({
                        "tool", 
                        result.dump(),
                        tool_name
                    });
                    
                    // 6. 再次调用 LLM，让它基于工具结果生成回复
                    call_llm(messages, callback);
                } else {
                    callback("抱歉，我没有这个工具");
                }
            } else {
                // 直接返回 LLM 的回复
                callback(reply);
            }
        });
    }
    
    std::string build_system_prompt() {
        std::string prompt = 
            "你是一个智能助手。你可以使用以下工具：\n\n";
        
        for (const auto& [name, tool] : tools_) {
            prompt += "工具: " + name + "\n";
            prompt += "描述: " + tool.description + "\n";
            prompt += "参数: " + tool.parameters.dump() + "\n\n";
        }
        
        prompt += "如果需要使用工具，请按以下格式回复：\n";
        prompt += "TOOL_CALL: {\"tool\": \"工具名\", \"arguments\": {...}}\n";
        
        return prompt;
    }

    LLMClient& llm_;
    std::map<std::string, Tool> tools_;
};
```

### 4.4 Agent Loop 流程图

```
用户输入
    │
    ▼
┌─────────────────┐
│ 1. 理解意图     │  ← LLM 分析用户需求
│  (LLM 调用)     │
└────────┬────────┘
         │
    ┌────┴────┐
    │         │
    ▼         ▼
直接回答    需要工具
    │         │
    │         ▼
    │    ┌─────────────────┐
    │    │ 2. 参数提取      │  ← LLM 提取工具参数
    │    │  (LLM/解析)      │
    │    └────────┬────────┘
    │             │
    │             ▼
    │    ┌─────────────────┐
    │    │ 3. 执行工具      │  ← 调用外部 API/函数
    │    │  (本地执行)      │
    │    └────────┬────────┘
    │             │
    │             ▼
    │    ┌─────────────────┐
    │    │ 4. 整合回复      │  ← LLM 基于结果生成回复
    │    │  (LLM 调用)      │
    │    └────────┬────────┘
    │             │
    └──────┬──────┘
           │
           ▼
    ┌─────────────────┐
    │ 5. 返回用户      │
    └─────────────────┘
```

---

## 五、流式输出（Streaming）

### 5.1 为什么需要流式？

LLM 生成回复需要时间（几秒内），用户等待体验差：

```
非流式：
用户: "写一段Python代码"
    │ 等待 5 秒
    ▼
Agent: [一次性显示全部代码]

流式：
用户: "写一段Python代码"
    │
    ▼
Agent: def
Agent: def quick
Agent: def quick_sort(
Agent: def quick_sort(arr):
Agent: def quick_sort(arr):
    if
... 逐字显示，体验更好
```

### 5.2 SSE（Server-Sent Events）

OpenAI 使用 SSE 协议实现流式输出：

```
HTTP 响应头：
Content-Type: text/event-stream

数据格式：
data: {"choices":[{"delta":{"content":"def"}}]}

data: {"choices":[{"delta":{"content":" quick"}}]}

data: {"choices":[{"delta":{"content":"_sort"}}]}

data: [DONE]
```

### 5.3 流式处理实现

```cpp
void LLMClient::chat_stream(
    const std::vector<Message>& messages,
    std::function<void(std::string)> on_chunk,
    std::function<void(error_code)> on_complete) {
    
    // 设置 stream: true
    json request_body = {
        {"model", "gpt-4"},
        {"messages", messages},
        {"stream", true}  // 启用流式
    };
    
    // 发送请求...
    
    // 读取 SSE 数据
    beast::flat_buffer buffer;
    
    auto do_read = [this, &buffer, on_chunk, on_complete](auto self) mutable {
        http::async_read_some(*stream_, buffer, 
            [self, &buffer, on_chunk, on_complete](auto ec, size_t) mutable {
                if (ec == http::error::end_of_stream) {
                    on_complete(std::error_code());
                    return;
                }
                if (ec) {
                    on_complete(ec);
                    return;
                }
                
                // 解析 SSE 数据
                auto data = buffer.data();
                std::string chunk(static_cast<const char*>(data.data()), 
                                 data.size());
                buffer.consume(data.size());
                
                // 解析 data: 行
                std::istringstream stream(chunk);
                std::string line;
                while (std::getline(stream, line)) {
                    if (line.find("data: ") == 0) {
                        std::string json_str = line.substr(6);
                        
                        if (json_str == "[DONE]") {
                            on_complete(std::error_code());
                            return;
                        }
                        
                        try {
                            json j = json::parse(json_str);
                            if (j.contains("choices") && 
                                !j["choices"].empty() &&
                                j["choices"][0].contains("delta")) {
                                
                                auto delta = j["choices"][0]["delta"];
                                if (delta.contains("content")) {
                                    on_chunk(delta["content"]);
                                }
                            }
                        } catch (...) {
                            // 忽略解析错误
                        }
                    }
                }
                
                // 继续读取
                self(self);
            }
        );
    };
    
    do_read(do_read);
}
```

---

## 六、本章小结

**核心收获：**

1. **LLM API**：
   - Chat Completions 接口
   - 消息角色（system/user/assistant/tool）
   - Token 计费概念

2. **工具调用（Function Calling）**：
   - 工具定义（名称、描述、参数 Schema）
   - Agent Loop：理解 → 决策 → 执行 → 回复
   - 多轮对话与工具结果整合

3. **流式输出**：
   - SSE 协议
   - 实时推送生成内容
   - 提升用户体验

---

## 七、引出的问题

### 7.1 工具管理问题

目前的工具是硬编码注册：

```cpp
agent.register_tool(weather_tool);
agent.register_tool(calculator_tool);
// ... 每个工具都要手动注册
```

**问题：**
- 新增工具需要修改代码、重新编译
- 工具之间可能有依赖关系
- 无法动态加载工具

**需要：** 工具注册表模式、依赖注入。

### 7.2 并发问题

如果多个用户同时请求：

```
User A ──▶ Agent ──▶ LLM API ──▶ 回复 A
User B ──▶ Agent ──▶ LLM API ──▶ 回复 B
                ↑
           会不会串扰？
```

**问题：** Agent 状态管理、并发安全、API 限流。

### 7.3 安全问题

工具执行可能存在风险：

```cpp
.execute = [](const json& params) {
    std::string cmd = params["command"];
    system(cmd.c_str());  // 危险！任意代码执行
};
```

**问题：** 如何防止恶意调用？需要沙箱机制。

---

**后续章节预告：**

- **Step 6**: 工具调用接口设计 —— 标准化工具定义
- **Step 7**: 异步工具执行 —— 并发控制、超时机制
- **Step 8**: 安全沙箱 —— SSRF 防护、路径限制、审计
- **Step 9**: 工具注册表 —— 注册表模式、依赖注入

现在 Agent 有了"大脑"（LLM）和"手脚"（工具），接下来要让工具系统更完善、更安全。
