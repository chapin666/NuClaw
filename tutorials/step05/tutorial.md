# Step 5: Agent 核心 — LLM 接入与工具调用

> 目标：接入真实 AI 能力，实现能理解、会思考的 Agent
> 
> 难度：⭐⭐⭐ (中等)
> 
> 代码量：约 450 行（较 Step 4 新增约 100 行）

---

## 问题引入

**Step 4 的问题：**

我们的服务器只能做简单的消息转发，无法智能回答：

```
用户: 今天北京天气怎么样？

方案 1 - 规则匹配（Step 3）：
  if (包含"天气") return "晴天";  ← 硬编码，不灵活

方案 2 - 需要实时数据：
  必须调用天气 API 获取真实数据
  但服务器是异步架构，如何等待 API 响应？
```

**本章目标：**
1. 接入 LLM（大语言模型），实现语义理解
2. 实现异步 HTTP 客户端调用外部 API
3. 让 LLM 能够"调用工具"获取实时数据

---

## 解决方案

### 什么是 LLM？

**LLM（Large Language Model）** 是基于深度学习的自然语言处理模型，它能：
- 理解自然语言（不只是关键词匹配）
- 推理和规划（分解复杂任务）
- 生成合理回复

**代表模型：** GPT-4、Claude、文心一言、通义千问

### Function Calling（工具调用）

现代 LLM 支持 Function Calling —— 让 AI 能够调用外部工具：

```
用户: 今天北京天气怎么样？

LLM 思考：
  1. 用户问天气
  2. 需要调用 get_weather 工具
  3. 参数：city="北京"

LLM → 服务器: 
  {
    "tool": "get_weather",
    "arguments": {"city": "北京"}
  }

服务器执行工具 → 返回结果

LLM 生成最终回复：
  "北京今天晴天，25°C，空气质量良好。"
```

### Agent Loop

```
        ┌─────────────────────────────────────┐
        │           Agent Loop                │
        │                                     │
   ┌────┴────┐    ┌─────────┐    ┌────────┐ │
   │  感知   │───▶│  理解   │───▶│  决策  │ │
   │(用户输入)│    │(LLM理解)│    │(是否调用工具)│
   └────┬────┘    └─────────┘    └───┬────┘ │
        │                            │      │
        │    ┌─────────┐    ┌────────┴───┐  │
        └───▶│  执行   │◀───│  调用工具   │  │
             │(返回结果)│    │(获取数据)   │  │
             └────┬────┘    └────────────┘  │
                  │                         │
                  └─────────────────────────┘
```

---

## 代码对比

### Step 4 的关键代码

```cpp
// 简单的消息广播
void on_message(const std::string& message) {
    json j = json::parse(message);
    std::string content = j.value("content", "");
    
    // 直接广播，没有智能处理
    manager_.broadcast(content, nullptr);
}
```

### Step 5 的修改

**主要改动：**
1. 添加异步 HTTP 客户端
2. 添加 LLM 客户端
3. 实现 Function Calling 协议
4. 添加天气工具

```cpp
// 新增：异步 HTTP 客户端
class HttpClient {
public:
    HttpClient(asio::io_context& io) : resolver_(io) {}
    
    async::Task<std::string> get(const std::string& host, 
                                   const std::string& path) {
        // 异步解析 DNS
        auto results = co_await resolver_.async_resolve(host, "http");
        
        // 连接
        tcp::socket socket(resolver_.get_executor());
        co_await asio::async_connect(socket, results);
        
        // 发送请求
        std::string request = "GET " + path + " HTTP/1.1\r\n";
        request += "Host: " + host + "\r\n";
        request += "Connection: close\r\n\r\n";
        co_await asio::async_write(socket, asio::buffer(request));
        
        // 读取响应
        char response[1024];
        size_t len = co_await socket.async_read_some(
            asio::buffer(response)
        );
        
        co_return std::string(response, len);
    }

private:
    tcp::resolver resolver_;
};

// 新增：天气工具
class WeatherTool {
public:
    WeatherTool(HttpClient& client) : client_(client) {}
    
    async::Task<std::string> get_weather(const std::string& city) {
        // 调用天气 API
        std::string response = co_await client_.get(
            "api.weather.com", 
            "/v1/current?city=" + city
        );
        
        // 解析响应（简化）
        co_return parse_weather(response);
    }

private:
    HttpClient& client_;
    
    std::string parse_weather(const std::string& response) {
        // 简化解析
        return "晴天，25°C";
    }
};

// 新增：LLM 客户端
class LLMClient {
public:
    LLMClient(HttpClient& client, const std::string& api_key)
        : client_(client), api_key_(api_key) {}
    
    // Function Calling 调用
    async::Task<LLMResponse> chat(const std::vector<Message>& history,
                                   const std::vector<Tool>& tools) {
        // 构造请求体
        json request;
        request["model"] = "gpt-3.5-turbo";
        request["messages"] = history;
        request["tools"] = tools;
        
        // 发送请求到 OpenAI API
        std::string response = co_await client_.post(
            "api.openai.com",
            "/v1/chat/completions",
            request.dump()
        );
        
        // 解析响应
        json j = json::parse(response);
        LLMResponse result;
        result.content = j["choices"][0]["message"]["content"];
        
        // 检查是否有工具调用
        if (j["choices"][0]["message"].contains("tool_calls")) {
            auto tool_call = j["choices"][0]["message"]["tool_calls"][0];
            result.tool_name = tool_call["function"]["name"];
            result.tool_args = json::parse(
                tool_call["function"]["arguments"].get<std::string>()
            );
            result.has_tool_call = true;
        }
        
        co_return result;
    }

private:
    HttpClient& client_;
    std::string api_key_;
};

// 修改：智能 Agent 会话
class AgentSession : public std::enable_shared_from_this<AgentSession> {
public:
    AgentSession(tcp::socket socket, LLMClient& llm, WeatherTool& weather)
        : socket_(std::move(socket)),
          llm_(llm),
          weather_(weather) {}

private:
    void on_message(const std::string& message) {
        // 添加到对话历史
        history_.push_back({"user", message});
        
        // 调用 LLM
        process_with_llm();
    }
    
    async::Task<void> process_with_llm() {
        // 定义可用工具
        std::vector<Tool> tools = {
            {
                "get_weather",
                "获取指定城市的天气",
                {
                    {"city", "string", "城市名称，如北京、上海"}
                }
            }
        };
        
        // 调用 LLM
        auto response = co_await llm_.chat(history_, tools);
        
        if (response.has_tool_call) {
            // LLM 要求调用工具
            std::string tool_result;
            
            if (response.tool_name == "get_weather") {
                std::string city = response.tool_args["city"];
                tool_result = co_await weather_.get_weather(city);
            }
            
            // 将工具结果返回给 LLM
            history_.push_back({"assistant", "调用工具: " + response.tool_name});
            history_.push_back({"tool", tool_result});
            
            // 再次调用 LLM 生成最终回复
            auto final_response = co_await llm_.chat(history_, tools);
            send(final_response.content);
        } else {
            // 直接回复
            send(response.content);
        }
        
        // 保存到历史
        history_.push_back({"assistant", response.content});
    }

    tcp::socket socket_;
    LLMClient& llm_;
    WeatherTool& weather_;
    std::vector<Message> history_;
};
```

---

## 文件变更清单

| 文件 | 变更类型 | 说明 |
|:---|:---|:---|
| `main.cpp` | 修改 | 添加 HttpClient、LLMClient、WeatherTool、AgentSession |
| `CMakeLists.txt` | 修改 | 添加 HTTP 客户端依赖（如 libcurl 或 Boost.Beast） |

---

## 完整源码（简化版）

```cpp
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <nlohmann/json.hpp>
#include <iostream>
#include <memory>
#include <vector>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = asio::ip::tcp;
using json = nlohmann::json;

// HTTP 客户端（使用 Boost.Beast）
class HttpClient {
public:
    HttpClient(asio::io_context& io) : resolver_(io) {}
    
    std::string post(const std::string& host, 
                     const std::string& target, 
                     const std::string& body,
                     const std::map<std::string, std::string>& headers = {}) {
        tcp::resolver resolver(resolver_.get_executor());
        beast::tcp_stream stream(resolver_.get_executor());
        
        // 解析并连接
        auto const results = resolver.resolve(host, "443");
        stream.connect(results);
        
        // 构造请求
        http::request<http::string_body> req{http::verb::post, target, 11};
        req.set(http::field::host, host);
        req.set(http::field::content_type, "application/json");
        for (const auto& [k, v] : headers) {
            req.set(k, v);
        }
        req.body() = body;
        req.prepare_payload();
        
        // 发送
        http::write(stream, req);
        
        // 接收响应
        beast::flat_buffer buffer;
        http::response<http::string_body> res;
        http::read(stream, buffer, res);
        
        // 关闭
        beast::error_code ec;
        stream.socket().shutdown(tcp::socket::shutdown_both, ec);
        
        return res.body();
    }

private:
    tcp::resolver resolver_;
};

// 工具定义
struct Tool {
    std::string name;
    std::string description;
    json parameters;
};

// LLM 响应
struct LLMResponse {
    std::string content;
    bool has_tool_call = false;
    std::string tool_name;
    json tool_args;
};

// LLM 客户端
class LLMClient {
public:
    LLMClient(HttpClient& http, const std::string& api_key)
        : http_(http), api_key_(api_key) {}
    
    LLMResponse chat(const std::vector<json>& messages, 
                    const std::vector<Tool>& tools) {
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
        }
        
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
        json j = json::parse(response);
        LLMResponse result;
        
        auto& message = j["choices"][0]["message"];
        result.content = message.value("content", "");
        
        if (message.contains("tool_calls")) {
            auto& tool_call = message["tool_calls"][0];
            result.has_tool_call = true;
            result.tool_name = tool_call["function"]["name"];
            result.tool_args = json::parse(
                tool_call["function"]["arguments"].get<std::string>()
            );
        }
        
        return result;
    }

    HttpClient& http_;
    std::string api_key_;
};

// 天气工具（模拟）
class WeatherTool {
public:
    std::string get_weather(const std::string& city) {
        // 模拟天气数据
        static std::map<std::string, std::string> weather_data = {
            {"北京", "晴天，25°C，空气质量良好"},
            {"上海", "多云，22°C，微风"},
            {"广州", "小雨，28°C，湿度较高"}
        };
        
        auto it = weather_data.find(city);
        if (it != weather_data.end()) {
            return it->second;
        }
        return "未知城市";
    }
};

// WebSocket + Agent
class AgentSession : public std::enable_shared_from_this<AgentSession> {
public:
    AgentSession(tcp::socket socket, LLMClient& llm, WeatherTool& weather)
        : ws_(std::move(socket)), llm_(llm), weather_(weather) {}

    void start() {
        // WebSocket 握手...
        do_accept();
    }

private:
    void do_accept() {
        auto self = shared_from_this();
        ws_.async_accept([this, self](beast::error_code ec) {
            if (!ec) {
                do_read();
            }
        });
    }
    
    void do_read() {
        auto self = shared_from_this();
        ws_.async_read(buffer_, [this, self](beast::error_code ec, std::size_t) {
            if (!ec) {
                std::string msg = beast::buffers_to_string(buffer_.data());
                buffer_.consume(buffer_.size());
                on_message(msg);
                do_read();
            }
        });
    }
    
    void on_message(const std::string& message) {
        try {
            json j = json::parse(message);
            std::string content = j.value("content", "");
            
            // 添加到历史
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
        // 定义工具
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
            std::string result;
            if (response.tool_name == "get_weather") {
                std::string city = response.tool_args["city"];
                result = weather_.get_weather(city);
            }
            
            // 添加工具调用到历史
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
                {"content", result}
            });
            
            // 再次调用 LLM
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
    
    void send(const std::string& message) {
        auto self = shared_from_this();
        ws_.async_write(asio::buffer(message), 
            [this, self](beast::error_code, std::size_t) {}
        );
    }

    beast::websocket::stream<tcp::socket> ws_;
    LLMClient& llm_;
    WeatherTool& weather_;
    beast::flat_buffer buffer_;
    std::vector<json> history_;
};

class Server {
public:
    Server(asio::io_context& io, unsigned short port, 
           LLMClient& llm, WeatherTool& weather)
        : acceptor_(io, tcp::endpoint(tcp::v4(), port)),
          llm_(llm), weather_(weather) {
        do_accept();
    }

private:
    void do_accept() {
        acceptor_.async_accept(
            [this](beast::error_code ec, tcp::socket socket) {
                if (!ec) {
                    std::make_shared<AgentSession>(
                        std::move(socket), llm_, weather_
                    )->start();
                }
                do_accept();
            }
        );
    }

    tcp::acceptor acceptor_;
    LLMClient& llm_;
    WeatherTool& weather_;
};

int main() {
    try {
        asio::io_context io;
        
        HttpClient http(io);
        LLMClient llm(http, "your-api-key-here");
        WeatherTool weather;
        
        Server server(io, 8080, llm, weather);
        
        std::cout << "Step 5 Agent Server listening on port 8080...\n";
        std::cout << "Features: LLM + Function Calling\n";
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
project(nuclaw_step05 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost REQUIRED COMPONENTS system)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

include(FetchContent)
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.2
)
FetchContent_MakeAvailable(json)

add_executable(nuclaw_step05 main.cpp)
target_link_libraries(nuclaw_step05 
    Boost::system
    Threads::Threads
    OpenSSL::SSL
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
)
```

---

## 交互示例

```
用户: 你好！
Agent: 你好！很高兴见到你。有什么我可以帮助你的吗？

用户: 北京今天天气怎么样？
Agent: [内部调用 get_weather(city="北京")]
Agent: 北京今天晴天，25°C，空气质量良好。

用户: 那上海呢？
Agent: [基于上下文理解用户问上海天气]
Agent: [调用 get_weather(city="上海")]
Agent: 上海今天多云，22°C，微风。
```

---

## 本章总结

- ✅ 解决了 Step 4 的"不够智能"问题
- ✅ 接入 LLM 实现语义理解
- ✅ 实现 Function Calling（工具调用）
- ✅ 掌握 Agent Loop：理解 → 决策 → 执行
- ✅ 代码从 350 行扩展到 450 行

---

## 课后思考

我们的 Agent 现在已经能对话和调用工具了，但还有几个明显问题：

**问题 1：对话历史无限增长**
```
对话 100 轮后，历史消息变得非常长
每次调用 LLM 都要带上全部历史
Token 费用越来越高...
```

**问题 2：没有长期记忆**
```
用户: 我叫小明
Agent: 你好小明！
...
[关闭连接，重新连接]
用户: 我叫什么？
Agent: 抱歉，我不知道...  ← 记忆丢失了！
```

**问题 3：没有用户隔离**
```
多个用户同时连接
他们共享同一个对话历史？
A 用户的消息被 B 用户看到？
```

如何解决这些问题？

<details>
<summary>点击查看后续章节 💡</summary>

**Step 6+: 状态管理与记忆系统**

后续我们将解决：
- **Step 6**: 会话管理 — 用户隔离和上下文管理
- **Step 7**: 短期记忆 — 窗口管理和摘要生成
- **Step 8**: 长期记忆 — 向量数据库和 RAG
- **Step 9**: 多 Agent 协作

最终构建一个真正有"记忆"的 Agent！

</details>
