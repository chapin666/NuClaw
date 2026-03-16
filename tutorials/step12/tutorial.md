# Step 12: MCP 协议接入 —— 连接外部世界

> 目标：实现 Model Context Protocol (MCP) 客户端，标准化接入外部工具
> 
> 难度：⭐⭐⭐⭐ | 代码量：约 900 行 | 预计学习时间：3-4 小时

---

## 一、为什么需要 MCP？

### 1.1 Step 9 的局限

Step 9 实现了工具注册表，但每个工具都需要**硬编码实现**：

```cpp
// Step 9: 每个工具都要手写代码
class WeatherTool : public Tool {
    json execute(const json& params) override {
        // 手写 HTTP 调用
        // 手写参数解析
        // 手写错误处理
    }
};

class DatabaseTool : public Tool {
    json execute(const json& params) override {
        // 又要手写一遍...
    }
};
```

**问题：**
- 每个新工具都要写 C++ 代码
- 工具实现和 Agent 强耦合
- 无法使用社区现成的工具

### 1.2 MCP 是什么？

**Model Context Protocol (MCP)** 是 Anthropic 推出的开放协议，标准化 LLM 与外部系统的连接：

```
┌─────────────────────────────────────────────────────────────┐
│                      MCP 架构                                │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌──────────────┐      MCP Protocol       ┌──────────────┐ │
│  │   NuClaw     │  ◄──────────────────►  │ MCP Server   │ │
│  │   Agent      │   JSON-RPC over stdio   │              │ │
│  │              │   or HTTP/SSE           │ • File System │ │
│  │ • Tools      │                        │ • Database    │ │
│  │ • Resources  │                        │ • GitHub      │ │
│  │ • Prompts    │                        │ • Slack       │ │
│  └──────────────┘                        │ • ...         │ │
│                                           └──────────────┘ │
│                                                             │
│  优势：                                                     │
│  • 一次实现，连接任意 MCP Server                            │
│  • 社区生态（100+ 现成 Server）                            │
│  • 标准化协议，跨语言/跨平台                                │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 二、MCP 核心概念

### 2.1 MCP 协议栈

| 层级 | 功能 | 说明 |
|:---|:---|:---|
| **应用层** | Tools/Resources/Prompts | 业务功能定义 |
| **协议层** | JSON-RPC 2.0 | 通信格式 |
| **传输层** | stdio / HTTP / SSE | 数据传输方式 |

### 2.2 MCP 交互流程

```
┌────────┐                              ┌─────────────┐
│ Client │                              │ MCP Server  │
└───┬────┘                              └──────┬──────┘
    │                                          │
    │  1. initialize                           │
    │ ───────────────────────────────────────► │
    │                                          │
    │  2. initialized (capabilities)           │
    │ ◄─────────────────────────────────────── │
    │                                          │
    │  3. tools/list                           │
    │ ───────────────────────────────────────► │
    │                                          │
    │  4. tools/list_result                    │
    │ ◄─────────────────────────────────────── │
    │                                          │
    │  5. tools/call (when needed)             │
    │ ───────────────────────────────────────► │
    │                                          │
    │  6. tools/call_result                    │
    │ ◄─────────────────────────────────────── │
```

---

## 三、代码实现

### 3.1 MCP Client 类

```cpp
// include/nuclaw/mcp_client.hpp
#pragma once
#include <boost/asio.hpp>
#include <boost/process.hpp>
#include <boost/json.hpp>
#include <string>
#include <memory>
#include <vector>
#include <functional>

namespace json = boost::json;
namespace bp = boost::process;
namespace asio = boost::asio;

namespace nuclaw {

// MCP Tool 定义
struct McpTool {
    std::string name;
    std::string description;
    json::object input_schema;
};

// MCP Resource 定义  
struct McpResource {
    std::string uri;
    std::string name;
    std::string mime_type;
};

class McpClient : public std::enable_shared_from_this<McpClient> {
public:
    using ToolCallback = std::function<void(const std::string& name, const json::value& result)>;
    
    explicit McpClient(asio::io_context& io);
    ~McpClient();
    
    // 连接到 MCP Server（stdio 模式）
    void connect(const std::string& command, const std::vector<std::string>& args);
    
    // 初始化握手
    asio::awaitable<void> initialize();
    
    // 获取可用工具列表
    asio::awaitable<std::vector<McpTool>> list_tools();
    
    // 调用工具
    asio::awaitable<json::value> call_tool(
        const std::string& tool_name, 
        const json::object& arguments
    );
    
    // 获取资源
    asio::awaitable<json::value> read_resource(const std::string& uri);
    
    bool is_connected() const { return connected_; }
    
private:
    asio::io_context& io_;
    std::unique_ptr<bp::child> process_;
    asio::writable_pipe stdin_pipe_;
    asio::readable_pipe stdout_pipe_;
    
    std::atomic<bool> connected_{false};
    std::atomic<int> request_id_{0};
    
    // 发送 JSON-RPC 请求
    asio::awaitable<json::value> send_request(
        const std::string& method, 
        const json::object& params
    );
    
    // 读取响应
    asio::awaitable<json::value> read_response();
    
    // 写入一行数据
    asio::awaitable<void> write_line(const std::string& data);
    
    // 读取一行数据
    asio::awaitable<std::string> read_line();
};

} // namespace nuclaw
```

### 3.2 MCP Client 实现

```cpp
// src/mcp_client.cpp
#include "nuclaw/mcp_client.hpp"
#include <iostream>

namespace nuclaw {

McpClient::McpClient(asio::io_context& io)
    : io_(io)
    , stdin_pipe_(io)
    , stdout_pipe_(io) {}

McpClient::~McpClient() {
    if (process_ && process_->running()) {
        process_->terminate();
    }
}

void McpClient::connect(
    const std::string& command, 
    const std::vector<std::string>& args
) {
    bp::async_pipe in_pipe(io_);
    bp::async_pipe out_pipe(io_);
    
    process_ = std::make_unique<bp::child>(
        command,
        bp::args(args),
        bp::std_in < in_pipe,
        bp::std_out > out_pipe,
        bp::std_err > bp::null,
        io_
    );
    
    stdin_pipe_ = std::move(in_pipe);
    stdout_pipe_ = std::move(out_pipe);
    connected_ = true;
}

asio::awaitable<void> McpClient::initialize() {
    json::object params;
    params["protocolVersion"] = "2024-11-05";
    
    json::object capabilities;
    capabilities["tools"] = json::object{};
    capabilities["resources"] = json::object{};
    params["capabilities"] = capabilities;
    
    params["clientInfo"] = json::object{
        {"name", "nuclaw-mcp-client"},
        {"version", "1.0.0"}
    };
    
    auto result = co_await send_request("initialize", params);
    
    // 发送 initialized 通知
    json::object notification;
    notification["jsonrpc"] = "2.0";
    notification["method"] = "notifications/initialized";
    
    co_await write_line(json::serialize(notification));
}

asio::awaitable<std::vector<McpTool>> McpClient::list_tools() {
    auto result = co_await send_request("tools/list", json::object{});
    
    std::vector<McpTool> tools;
    
    if (result.is_object() && result.as_object().contains("tools")) {
        const auto& tools_array = result.at("tools").as_array();
        
        for (const auto& tool_val : tools_array) {
            const auto& tool_obj = tool_val.as_object();
            
            McpTool tool;
            tool.name = std::string(tool_obj.at("name").as_string());
            tool.description = tool_obj.contains("description") 
                ? std::string(tool_obj.at("description").as_string()) 
                : "";
            
            if (tool_obj.contains("inputSchema")) {
                tool.input_schema = tool_obj.at("inputSchema").as_object();
            }
            
            tools.push_back(std::move(tool));
        }
    }
    
    co_return tools;
}

asio::awaitable<json::value> McpClient::call_tool(
    const std::string& tool_name,
    const json::object& arguments
) {
    json::object params;
    params["name"] = tool_name;
    params["arguments"] = arguments;
    
    auto result = co_await send_request("tools/call", params);
    co_return result;
}

asio::awaitable<json::value> McpClient::read_resource(const std::string& uri) {
    json::object params;
    params["uri"] = uri;
    
    auto result = co_await send_request("resources/read", params);
    co_return result;
}

asio::awaitable<json::value> McpClient::send_request(
    const std::string& method,
    const json::object& params
) {
    int id = ++request_id_;
    
    json::object request;
    request["jsonrpc"] = "2.0";
    request["id"] = id;
    request["method"] = method;
    request["params"] = params;
    
    co_await write_line(json::serialize(request));
    
    // 读取响应（简化版，实际应匹配 request id）
    auto response = co_await read_response();
    co_return response;
}

asio::awaitable<json::value> McpClient::read_response() {
    auto line = co_await read_line();
    
    if (line.empty()) {
        throw std::runtime_error("Empty response from MCP server");
    }
    
    auto value = json::parse(line);
    
    // 检查错误
    if (value.as_object().contains("error")) {
        const auto& error = value.at("error").as_object();
        auto code = error.at("code").to_number<int>();
        auto message = std::string(error.at("message").as_string());
        throw std::runtime_error("MCP error " + std::to_string(code) + ": " + message);
    }
    
    co_return value.at("result");
}

asio::awaitable<void> McpClient::write_line(const std::string& data) {
    co_await asio::async_write(
        stdin_pipe_,
        asio::buffer(data + "\n"),
        asio::use_awaitable
    );
}

asio::awaitable<std::string> McpClient::read_line() {
    asio::streambuf buf;
    
    co_await asio::async_read_until(
        stdout_pipe_,
        buf,
        '\n',
        asio::use_awaitable
    );
    
    std::string line{
        asio::buffers_begin(buf.data()),
        asio::buffers_end(buf.data())
    };
    
    // 去除换行符
    if (!line.empty() && line.back() == '\n') {
        line.pop_back();
    }
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    
    co_return line;
}

} // namespace nuclaw
```

### 3.3 MCP 集成到 Agent

```cpp
// include/nuclaw/agent_with_mcp.hpp
#pragma once
#include "nuclaw/agent.hpp"
#include "nuclaw/mcp_client.hpp"
#include <memory>
#include <vector>

namespace nuclaw {

class AgentWithMcp : public Agent {
public:
    AgentWithMcp(const std::string& name, const std::string& persona);
    
    // 添加 MCP Server 连接
    void add_mcp_server(
        const std::string& name,
        const std::string& command,
        const std::vector<std::string>& args
    );
    
    // 初始化所有 MCP 连接
    asio::awaitable<void> initialize_mcp();
    
    // 处理消息（自动使用 MCP 工具）
    asio::awaitable<std::string> process_message(const std::string& user_input);
    
    // 获取所有可用 MCP 工具
    std::vector<McpTool> get_all_mcp_tools();
    
private:
    struct McpServer {
        std::string name;
        std::shared_ptr<McpClient> client;
        std::vector<McpTool> tools;
    };
    
    std::vector<McpServer> mcp_servers_;
    asio::io_context io_;
    
    // 将 MCP 工具转换为 LLM 可用的 tool schema
    json::array build_tools_schema();
    
    // 执行 MCP 工具调用
    asio::awaitable<json::value> execute_mcp_tool(
        const std::string& server_name,
        const std::string& tool_name,
        const json::object& arguments
    );
};

} // namespace nuclaw
```

### 3.4 主程序示例

```cpp
// src/main.cpp
#include "nuclaw/agent_with_mcp.hpp"
#include <iostream>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>

using namespace nuclaw;
namespace asio = boost::asio;

asio::awaitable<void> run_demo() {
    // 创建支持 MCP 的 Agent
    AgentWithMcp agent("mcp_agent", "我是 MCP Agent，可以连接各种外部工具");
    
    std::cout << "🔌 连接到 MCP Servers...\n\n";
    
    // 连接文件系统 MCP Server（需要提前安装 @modelcontextprotocol/server-filesystem）
    agent.add_mcp_server(
        "filesystem",
        "npx",
        {"-y", "@modelcontextprotocol/server-filesystem", "/tmp"}
    );
    
    // 连接 SQLite MCP Server
    agent.add_mcp_server(
        "sqlite",
        "uvx",
        {"mcp-server-sqlite", "--db-path", "/tmp/demo.db"}
    );
    
    // 初始化所有连接
    co_await agent.initialize_mcp();
    
    // 显示可用工具
    auto tools = agent.get_all_mcp_tools();
    std::cout << "📦 可用工具（" << tools.size() << " 个）:\n";
    for (const auto& tool : tools) {
        std::cout << "  • " << tool.name << " - " << tool.description << "\n";
    }
    std::cout << "\n";
    
    // 演示对话
    std::vector<std::string> queries = {
        "列出 /tmp 目录下的所有文件",
        "创建一个名为 test_table 的表，包含 id 和 name 字段",
        "向 test_table 插入一条记录：id=1, name='hello'"
    };
    
    for (const auto& query : queries) {
        std::cout << "👤 用户: " << query << "\n";
        
        try {
            auto response = co_await agent.process_message(query);
            std::cout << "🤖 Agent: " << response << "\n\n";
        } catch (const std::exception& e) {
            std::cout << "❌ 错误: " << e.what() << "\n\n";
        }
    }
}

int main() {
    try {
        asio::io_context io;
        
        asio::co_spawn(io, run_demo(), asio::detached);
        
        io.run();
        
    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
```

---

## 四、运行演示

### 4.1 安装 MCP Servers

```bash
# 文件系统 MCP Server
npm install -g @modelcontextprotocol/server-filesystem

# SQLite MCP Server
pip install mcp-server-sqlite
# 或
uvx mcp-server-sqlite
```

### 4.2 编译运行

```bash
cd src/step12
mkdir build && cd build
cmake ..
make -j
./step12_demo
```

### 4.3 预期输出

```
🔌 连接到 MCP Servers...

📦 可用工具（8 个）:
  • read_file - 读取文件内容
  • write_file - 写入文件内容
  • list_directory - 列出目录内容
  • search_files - 搜索文件
  • sqlite_query - 执行 SQL 查询
  • sqlite_execute - 执行 SQL 命令
  • sqlite_analyze - 分析表结构
  • sqlite_export - 导出数据

👤 用户: 列出 /tmp 目录下的所有文件
🤖 Agent: 我为您查看了 /tmp 目录，包含以下文件：
   • demo.db
   • session_001.log
   • cache/

👤 用户: 创建一个名为 test_table 的表，包含 id 和 name 字段
🤖 Agent: 表创建成功！test_table 的结构：
   • id (INTEGER)
   • name (TEXT)

👤 用户: 向 test_table 插入一条记录：id=1, name='hello'
🤖 Agent: 记录插入成功！已插入：id=1, name='hello'
```

---

## 五、本章总结

### 5.1 解决了什么问题？

| Step 9 的问题 | Step 12 的解决方案 |
|:---|:---|
| 每个工具都要手写 C++ 代码 | 连接 MCP Server，自动发现工具 |
| 工具与 Agent 强耦合 | 通过标准协议解耦 |
| 无法使用社区工具 | 接入 100+ MCP Servers |

### 5.2 新增能力

- ✅ **MCP 协议实现** - JSON-RPC 2.0 通信
- ✅ **stdio 传输** - 子进程模式连接 Server
- ✅ **工具自动发现** - 动态获取可用工具列表
- ✅ **工具调用** - 统一接口执行任意 MCP 工具

### 5.3 演进路径

```
Step 6: 工具调用（硬编码）
    ↓
Step 9: 工具注册表（管理多个硬编码工具）
    ↓
Step 12: MCP 协议（标准化接入外部工具）⭐ 本章
```

---

## 六、课后思考

当前 MCP 实现还有什么问题？

<details>
<summary>点击查看下一章要解决的问题 💡</summary>

**问题 1：配置硬编码**

MCP Server 的连接信息（命令、参数）硬编码在代码中。如何支持配置文件管理？

**问题 2：缺乏监控**

MCP 工具调用的成功率、延迟、错误率无法观测。如何添加监控？

**问题 3：Server 故障处理**

MCP Server 进程崩溃后，Agent 无法自动恢复。如何实现高可用？

**Step 13 预告：配置管理**
我们将实现：
- **YAML/JSON 配置** - 外部化 MCP Server 配置
- **热加载** - 不重启更新配置
- **环境适配** - 不同环境不同配置

</details>

---

## 参考资源

- [MCP 官方文档](https://modelcontextprotocol.io/)
- [MCP Servers 列表](https://github.com/modelcontextprotocol/servers)
- [MCP Protocol Specification](https://spec.modelcontextprotocol.io/)

---

## 文件变更清单

| 文件 | 变更类型 | 说明 |
|:---|:---|:---|
| `include/nuclaw/mcp_client.hpp` | **新增** | MCP Client 头文件 |
| `src/mcp_client.cpp` | **新增** | MCP Client 实现 |
| `include/nuclaw/agent_with_mcp.hpp` | **新增** | 集成 MCP 的 Agent |
| `src/main.cpp` | **新增** | 演示程序 |
| `CMakeLists.txt` | **新增** | 构建配置 |

**复用文件：**
- 复用 Step 9 的 Tool 基类
- 复用 Step 6 的 LLM HTTP 客户端
- 复用 Step 5 的 Agent 基础架构
