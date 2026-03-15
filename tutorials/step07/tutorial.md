# Step 7: 异步工具执行 —— 并发控制与超时机制

> 目标：实现异步非阻塞的工具执行，支持并发调用和超时控制
> 
003e 难度：⭐⭐⭐⭐ | 代码量：约 600 行 | 预计学习时间：3-4 小时

---

## 一、为什么需要异步工具？

### 1.1 Step 6 的阻塞问题

Step 6 的工具执行是同步阻塞的：

```cpp
ToolResult WeatherTool::execute(...) {
    // 发起 HTTP 请求，阻塞等待响应
    auto response = http_client.get("https://api.weather.com/v1/current");
    // 假设 API 响应需要 2 秒
    // 这 2 秒内，整个线程被阻塞！
    return parse_response(response);
}
```

**实际场景的问题：**

```
用户A: "查北京天气" ──▶ Agent ──▶ 调用天气 API (等待2秒)
                              ↑
用户B: "你好" ────────────────┘ 必须等待A完成！

结果：一个慢工具拖慢所有用户
```

### 1.2 异步执行的优势

```
同步执行：                              异步执行：
────────────────────────────────────────────────────────────────
时间轴 ─────────────────────────────────────────────────────▶

用户A ──请求──▶ 等待2秒 ──▶ 响应                   用户A ──请求──▶ 立即返回
    │                                          用户B ──请求──▶ 立即返回
用户B ────────────────等待2秒──────▶ 响应        用户C ──请求──▶ 立即返回
    │                                               │
用户C ───────────────────────等待2秒──────▶ 响应    ▼
                                               等待API响应
                                                   │
                                               全部同时完成！
```

**异步的好处：**
1. **不阻塞主线程**：工具执行期间 Agent 可以处理其他请求
2. **并发执行**：多个工具可以同时执行
3. **资源利用率高**：I/O 等待期间 CPU 可以做其他事

---

## 二、异步工具接口设计

### 2.1 接口升级

```cpp
// 异步工具结果回调
template<typename Callback>
using ToolCallback = std::function<void(ToolResult)>;

// 异步工具接口
class IAsyncTool {
public:
    virtual ~IAsyncTool() = default;
    
    virtual std::string get_name() const = 0;
    virtual std::string get_description() const = 0;
    virtual std::vector<ToolParameter> get_parameters() const = 0;
    
    // 关键变化：异步执行，通过回调返回结果
    virtual void execute_async(
        const json& arguments,
        const ToolContext& context,
        std::chrono::milliseconds timeout,
        std::function<void(ToolResult)> callback
    ) = 0;
    
    // 可选：取消执行
    virtual void cancel(const std::string& execution_id) {}
};
```

### 2.2 与同步工具的对比

| 特性 | 同步工具 (Step 6) | 异步工具 (Step 7) |
|:---|:---|:---|
| 返回方式 | 直接返回 `ToolResult` | 通过回调返回 |
| 阻塞性 | 阻塞线程 | 非阻塞 |
| 并发能力 | 顺序执行 | 可并发执行 |
| 超时控制 | 难以实现 | 易于实现 |
| 代码复杂度 | 简单 | 较复杂（回调处理）|

---

## 三、异步 HTTP 客户端

工具异步执行通常依赖异步 HTTP 请求：

### 3.1 异步 HTTP 实现

```cpp
class AsyncHttpClient {
public:
    AsyncHttpClient(asio::io_context& io) : io_(io) {}
    
    // 异步 GET 请求
    void get(const std::string& url,
             std::chrono::milliseconds timeout,
             std::function<void(HttpResponse)> callback);
    
    // 异步 POST 请求
    void post(const std::string& url,
              const std::string& body,
              std::chrono::milliseconds timeout,
              std::function<void(HttpResponse)> callback);

private:
    asio::io_context& io_;
};

// 使用 Boost.Beast 实现
void AsyncHttpClient::get(const std::string& url,
                          std::chrono::milliseconds timeout,
                          std::function<void(HttpResponse)> callback) {
    
    // 解析 URL（简化版，实际应该用 URL 解析库）
    std::string host = extract_host(url);
    std::string path = extract_path(url);
    
    // 创建连接组件
    auto resolver = std::make_shared<tcp::resolver>(io_);
    auto stream = std::make_shared<beast::tcp_stream>(io_);
    
    // 设置超时
    stream->expires_after(timeout);
    
    // 解析域名
    resolver->async_resolve(
        host, "80",
        [stream, host, path, callback](beast::error_code ec, 
                                        tcp::resolver::results_type results) {
            if (ec) {
                callback(HttpResponse{.success = false, 
                                      .error = ec.message()});
                return;
            }
            
            // 连接
            stream->async_connect(
                results,
                [stream, host, path, callback](beast::error_code ec, 
                                               tcp::resolver::endpoint_type) {
                    if (ec) {
                        callback(HttpResponse{.success = false, 
                                              .error = ec.message()});
                        return;
                    }
                    
                    // 构造 HTTP 请求
                    http::request<http::string_body> req{
                        http::verb::get, path, 11};
                    req.set(http::field::host, host);
                    req.set(http::field::user_agent, "NuClaw-Agent/1.0");
                    
                    // 发送请求
                    http::async_write(*stream, req,
                        [stream, callback](beast::error_code ec, size_t) {
                            if (ec) {
                                callback(HttpResponse{.success = false, 
                                                      .error = ec.message()});
                                return;
                            }
                            
                            // 读取响应
                            auto res = std::make_shared<
                                http::response<http::string_body>
                            >();
                            auto buffer = std::make_shared<beast::flat_buffer>();
                            
                            http::async_read(*stream, *buffer, *res,
                                [res, buffer, stream, callback](beast::error_code ec, 
                                                                size_t) {
                                    if (ec && ec != http::error::end_of_stream) {
                                        callback(HttpResponse{.success = false, 
                                                              .error = ec.message()});
                                        return;
                                    }
                                    
                                    // 成功返回
                                    callback(HttpResponse{
                                        .success = true,
                                        .status_code = res->result_int(),
                                        .body = res->body()
                                    });
                                    
                                    // 关闭连接
                                    stream->socket().shutdown(
                                        tcp::socket::shutdown_both, ec);
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

### 3.2 关键设计点

**组件生命周期管理：**

```cpp
// 使用 shared_ptr 管理异步组件生命周期
auto resolver = std::make_shared<tcp::resolver>(io_);
auto stream = std::make_shared<beast::tcp_stream>(io_);
auto res = std::make_shared<http::response<...>>();

// 在回调中捕获 shared_ptr，确保组件存活到回调执行
[resolver, stream, res](...) { ... }
```

**超时控制：**

```cpp
// Boost.Beast 提供方便的过期机制
stream->expires_after(std::chrono::seconds(30));

// 超时后会收到 operation_aborted 错误
if (ec == asio::error::operation_aborted) {
    // 超时处理
}
```

---

## 四、异步天气工具实现

```cpp
class AsyncWeatherTool : public IAsyncTool {
public:
    AsyncWeatherTool(asio::io_context& io) 
        : http_client_(io) {}
    
    std::string get_name() const override {
        return "get_weather_async";
    }
    
    std::string get_description() const override {
        return "异步获取指定城市的天气信息";
    }
    
    std::vector<ToolParameter> get_parameters() const override {
        return {
            {
                .name = "location",
                .type = "string",
                .description = "城市名称",
                .required = true
            },
            {
                .name = "timeout_ms",
                .type = "integer",
                .description = "超时时间（毫秒）",
                .required = false,
                .default_value = 5000
            }
        };
    }
    
    void execute_async(const json& arguments,
                       const ToolContext& context,
                       std::chrono::milliseconds default_timeout,
                       std::function<void(ToolResult)> callback) override {
        
        auto start = std::chrono::steady_clock::now();
        
        // 提取参数
        std::string location = arguments.value("location", "");
        int timeout_ms = arguments.value("timeout_ms", 5000);
        
        if (location.empty()) {
            callback(ToolResult{
                .success = false,
                .error_message = "location 不能为空"
            });
            return;
        }
        
        // 构造 API URL（使用免费的 Open-Meteo API）
        std::string url = "https://api.open-meteo.com/v1/forecast?" +
                         "latitude=39.9&longitude=116.4" +  // 简化：固定北京
                         "&current=temperature_2m,weather_code";
        
        // 异步发起请求
        http_client_.get(
            url,
            std::chrono::milliseconds(timeout_ms),
            [this, location, start, callback](HttpResponse response) {
                auto end = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<
                    std::chrono::milliseconds>(end - start).count();
                
                if (!response.success) {
                    callback(ToolResult{
                        .success = false,
                        .error_message = "HTTP 请求失败: " + response.error,
                        .execution_time_ms = elapsed
                    });
                    return;
                }
                
                // 解析 JSON 响应
                try {
                    json data = json::parse(response.body);
                    
                    ToolResult result{
                        .success = true,
                        .data = {
                            {"location", location},
                            {"temperature", data["current"]["temperature_2m"]},
                            {"unit", "°C"},
                            {"raw_response", data}
                        },
                        .execution_time_ms = elapsed
                    };
                    
                    callback(result);
                    
                } catch (const std::exception& e) {
                    callback(ToolResult{
                        .success = false,
                        .error_message = "JSON 解析失败: " + std::string(e.what()),
                        .execution_time_ms = elapsed
                    });
                }
            }
        );
        
        // 立即返回，不等待 HTTP 响应！
    }

private:
    AsyncHttpClient http_client_;
};
```

---

## 五、并发执行与超时控制

### 5.1 同时执行多个工具

```cpp
class AsyncToolManager {
public:
    // 并发执行多个工具
    void execute_parallel(
        const std::vector<ToolCall>& calls,
        std::chrono::milliseconds timeout,
        std::function<void(std::vector<ToolResult>)> callback
    );

private:
    asio::io_context& io_;
    std::map<std::string, std::shared_ptr<IAsyncTool>> tools_;
};

struct ToolCall {
    std::string tool_name;
    json arguments;
};

void AsyncToolManager::execute_parallel(
    const std::vector<ToolCall>& calls,
    std::chrono::milliseconds timeout,
    std::function<void(std::vector<ToolResult>)> callback) {
    
    auto results = std::make_shared<std::vector<ToolResult>>(
        calls.size()
    );
    auto completed = std::make_shared<std::atomic<size_t>>(0);
    auto timer = std::make_shared<asio::steady_timer>(io_);
    
    // 设置全局超时
    timer->expires_after(timeout);
    timer->async_wait([results, completed](beast::error_code ec) {
        if (!ec) {
            // 超时，标记未完成的为超时
            for (auto& r : *results) {
                if (r.execution_time_ms == 0) {
                    r.success = false;
                    r.error_message = "Timeout";
                }
            }
        }
    });
    
    // 并发启动所有工具
    for (size_t i = 0; i < calls.size(); ++i) {
        const auto& call = calls[i];
        auto tool = get_tool(call.tool_name);
        
        if (!tool) {
            (*results)[i] = ToolResult{
                .success = false,
                .error_message = "Tool not found: " + call.tool_name
            };
            (*completed)++;
            continue;
        }
        
        // 为每个工具分配独立的超时
        auto tool_timeout = timeout / calls.size();
        
        tool->execute_async(
            call.arguments,
            ToolContext{},
            tool_timeout,
            [results, completed, callback, i, timer](ToolResult result) {
                (*results)[i] = result;
                
                // 检查是否全部完成
                size_t done = ++(*completed);
                if (done == results->size()) {
                    timer->cancel();  // 取消超时定时器
                    callback(*results);
                }
            }
        );
    }
    
    // 如果没有任何工具调用，立即回调
    if (calls.empty()) {
        callback(*results);
    }
}
```

### 5.2 使用示例

```cpp
// 同时查询多个城市的天气
std::vector<ToolCall> calls = {
    {"get_weather_async", {{"location", "北京"}}},
    {"get_weather_async", {{"location", "上海"}}},
    {"get_weather_async", {{"location", "广州"}}}
};

tool_manager.execute_parallel(
    calls,
    std::chrono::seconds(10),  // 总超时 10 秒
    [](std::vector<ToolResult> results) {
        for (const auto& r : results) {
            if (r.success) {
                std::cout << "温度: " << r.data["temperature"] << "°C\n";
            } else {
                std::cout << "失败: " << r.error_message << "\n";
            }
        }
    }
);

// 所有请求同时发出，总耗时 ≈ 最慢的那个
// 而不是 3 个请求的时间之和
```

---

## 六、集成到 Agent

### 6.1 异步 Agent 实现

```cpp
class AsyncAgent {
public:
    AsyncAgent(asio::io_context& io, LLMClient& llm)
        : io_(io), llm_(llm), tool_manager_(io) {
        register_tools();
    }
    
    void process(const std::string& user_input,
                 std::function<void(const std::string&)> callback);

private:
    void call_llm_async(const std::vector<Message>& messages,
                        std::function<void(std::string)> callback);
    
    void handle_tool_calls(const std::string& llm_response,
                           std::function<void(const std::string&)> callback);

    asio::io_context& io_;
    LLMClient& llm_;
    AsyncToolManager tool_manager_;
};

void AsyncAgent::process(const std::string& user_input,
                         std::function<void(const std::string&)> callback) {
    
    std::vector<Message> messages = {
        {"system", build_system_prompt()},
        {"user", user_input}
    };
    
    // 异步调用 LLM
    call_llm_async(messages, 
        [this, messages, callback](std::string llm_response) mutable {
            
            // 检查是否需要调用工具
            if (has_tool_call(llm_response)) {
                // 执行工具（异步）
                handle_tool_calls(llm_response, 
                    [this, messages, callback](std::string tool_result) {
                        
                        // 将工具结果加入对话历史
                        messages.push_back({"assistant", llm_response});
                        messages.push_back({"tool", tool_result});
                        
                        // 再次调用 LLM 生成最终回复
                        call_llm_async(messages, callback);
                    }
                );
            } else {
                // 直接返回 LLM 回复
                callback(llm_response);
            }
        }
    );
}
```

---

## 七、本章小结

**核心收获：**

1. **异步接口设计**：
   - `execute_async` 替代同步 `execute`
   - 回调机制返回结果
   - 非阻塞执行

2. **异步 HTTP 客户端**：
   - Boost.Beast 实现
   - 组件生命周期管理（shared_ptr）
   - 超时控制

3. **并发执行**：
   - 同时发起多个工具调用
   - 等待全部完成
   - 超时处理

---

## 八、引出的问题

### 8.1 安全问题

异步 HTTP 请求仍存在安全隐患：

```cpp
// 用户可能传入恶意 URL
arguments: {"url": "http://localhost:8080/admin/delete-all"}
// SSRF 攻击！
```

**需要：** URL 白名单、IP 黑名单、协议限制。

### 8.2 资源限制问题

并发执行可能耗尽资源：

```cpp
// 用户要求同时执行 1000 个工具
calls = 1000 个工具调用
// 可能导致：内存耗尽、连接数超限、目标服务被打挂
```

**需要：** 并发数限制、队列机制、熔断保护。

---

**下一章预告（Step 8）：**

我们将实现**安全沙箱**：
- SSRF 防护（禁止访问内网）
- 文件路径限制（禁止越界访问）
- 审计日志（记录所有工具调用）

工具执行已经异步化，接下来要确保执行安全。
