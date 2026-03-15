# NuClaw 演示程序 (Demos)

本文档介绍 Step 6/7/8/10 中的演示程序，这些程序展示了如何使用通用组件连接真实的外部服务。

## 目录

- [概述](#概述)
- [快速开始](#快速开始)
- [演示详解](#演示详解)
- [通用组件](#通用组件)
- [配置说明](#配置说明)
- [故障排除](#故障排除)

---

## 概述

演示程序放在对应 Step 目录中，是原教程的补充，目标是展示如何将概念代码应用到实际场景：

| 对比 | 原教程代码 (main.cpp) | 演示程序 (stepXX_*_demo.cpp) |
|:---|:---|:---|
| **目的** | 教学，渐进式理解 | 实战，连接真实服务 |
| **代码** | 简化，易于理解 | 完整，生产可用 |
| **外部依赖** | 尽量 Mock 或内存实现 | 真实的 HTTP/DB 服务 |
| **LLM** | 模拟/关键词匹配 | 真实的 OpenAI/Moonshot API |
| **存储** | 内存数据结构 | 真实的 PostgreSQL |

**文件位置：**
```
src/
├── step06/
│   ├── main.cpp                 # 原教程
│   ├── step06_rest_demo.cpp     # REST API 演示
│   ├── test_http_components.cpp # HTTP 组件测试
│   └── test_database.cpp        # 数据库测试
├── step07/
│   ├── main.cpp
│   └── step07_llm_demo.cpp      # 真实 LLM 演示
├── step08/
│   ├── main.cpp
│   └── step08_db_demo.cpp       # 数据库集成演示
└── step10/
    ├── main.cpp
    └── step10_test_demo.cpp     # 测试策略演示
```

## 快速开始

### 1. 编译

```bash
cd NuClaw
mkdir build && cd build
cmake .. && make -j4
```

### 2. 运行演示

**Step 6: REST API 演示**
```bash
./step06_rest_demo
# 另一个终端
curl http://localhost:8080/health
curl -X POST http://localhost:8080/chat \
  -d '{"message":"北京天气如何?"}'
```

**Step 7: 真实 LLM 调用**
```bash
export OPENAI_API_KEY=sk-your-key-here
./step07_llm_demo
curl -X POST http://localhost:8080/chat \
  -d '{"message":"你好，请介绍你自己","session_id":"test001"}'
```

**Step 8: 数据库集成**
```bash
# 启动 PostgreSQL
docker run -d --name pg -e POSTGRES_PASSWORD=postgres -p 5432:5432 postgres:15

export DATABASE_URL=postgresql://postgres:postgres@localhost:5432/postgres
export OPENAI_API_KEY=sk-your-key-here

./step08_db_demo
# 会自动初始化表结构
curl -X POST http://localhost:8080/chat \
  -d '{"message":"Hello","user_id":"user001"}'
```

**Step 10: 运行测试**
```bash
./step10_test_demo
# 或使用 ctest
ctest --output-on-failure
```

---

## 演示详解

### step06_rest_demo

**功能：** REST API 服务器，支持 Mock LLM 和真实 LLM

**端点：**
- `POST /chat` - 使用 Mock LLM（关键词匹配）
- `POST /llm/chat` - 使用真实 OpenAI API（需配置 API Key）
- `GET /health` - 健康检查

**代码特点：**
- 使用 `common/http_server.hpp` 提供 REST API
- 演示路由注册和 JSON 请求/响应处理

---

### step07_llm_demo

**功能：** 真实 LLM HTTP 调用，支持会话历史

**端点：**
- `POST /chat` - 对话（自动维护会话历史）
- `GET /config` - 查看 LLM 配置
- `GET /health` - 健康检查

**代码特点：**
- 使用 `common/llm_http_client.hpp` 调用真实 API
- `ConversationManager` 管理多会话历史
- 支持 OpenAI 和 Moonshot 切换

---

### step08_db_demo

**功能：** 完整的 LLM + 数据库集成

**端点：**
- `POST /chat` - 对话（自动创建会话，持久化消息）
- `POST /history` - 获取会话历史
- `GET /sessions` - 列出所有会话
- `POST /stats` - 获取用户统计
- `GET /health` - 健康检查

**代码特点：**
- `ChatDAO` 数据访问对象封装数据库操作
- 自动初始化表结构
- 事务处理保证数据一致性

---

### step10_test_demo

**功能：** Google Test 测试示例

**测试内容：**
- **单元测试：** HTTP Response, LLM Config
- **Mock 测试：** MockLLMClient, MockDatabase
- **集成测试：** ChatService 完整业务流程
- **性能测试：** 延迟基准测试

**运行：**
```bash
./step10_test_demo
# 输出
[==========] Running 10 tests from 4 test suites...
[  PASSED  ] 10 tests.
```

---

## 通用组件

演示程序依赖 `src/include/common/` 下的通用组件（共享头文件）：

### http_client.hpp

基于 Boost.Beast 的同步 HTTP 客户端。

```cpp
#include "common/http_client.hpp"

nuclaw::http::Client client;
auto resp = client.post(
    "https://api.openai.com/v1/chat/completions",
    {{"Authorization", "Bearer " + api_key}},
    json_body,
    std::chrono::seconds(30)
);

if (resp.success) {
    std::cout << resp.body;
}
```

### llm_http_client.hpp

封装 OpenAI/Moonshot API 调用。

```cpp
#include "common/llm_http_client.hpp"

nuclaw::LLMHttpClient llm;  // 从环境变量自动配置
auto response = llm.chat("你好");

if (response.success) {
    std::cout << response.content;
    std::cout << "Tokens: " << response.tokens_used;
    std::cout << "Latency: " << response.latency_ms << "ms";
}
```

### http_server.hpp

简单的 REST API 服务器。

```cpp
#include "common/http_server.hpp"

boost::asio::io_context io;
nuclaw::http::Server server(io, 8080);

server.post("/chat", [](const json::value& req) {
    auto message = std::string(req.at("message").as_string());
    // 处理...
    return json::object({{"reply", response}});
});

server.start();
io.run();
```

### database.hpp

PostgreSQL 连接池和 ORM 简化封装。

```cpp
#include "common/database.hpp"

nuclaw::db::Database db = nuclaw::db::Database::from_env();

// 初始化表结构
db.init_schema();

// 执行查询
auto result = db.query(
    "SELECT * FROM users WHERE id = $1",
    user_id
);

// 事务
db.transaction([](pqxx::work& txn) {
    txn.exec("INSERT INTO ...");
    // 自动提交或回滚
});
```

---

## 配置说明

### 环境变量

| 变量 | 说明 | 示例 |
|:---|:---|:---|
| `OPENAI_API_KEY` | OpenAI API Key | `sk-xxx...` |
| `MOONSHOT_API_KEY` | Moonshot API Key | `sk-xxx...` |
| `LLM_PROVIDER` | 选择提供商 | `openai` 或 `moonshot` |
| `LLM_MODEL` | 模型名称 | `gpt-3.5-turbo` |
| `DATABASE_URL` | PostgreSQL 连接 URL | `postgresql://user:pass@host:5432/db` |
| `DB_HOST` | 数据库主机 | `localhost` |
| `DB_PORT` | 数据库端口 | `5432` |
| `DB_NAME` | 数据库名 | `nuclaw` |
| `DB_USER` | 数据库用户 | `postgres` |
| `DB_PASSWORD` | 数据库密码 | `secret` |

### Docker 快速启动 PostgreSQL

```bash
docker run -d \
    --name nuclaw-postgres \
    -e POSTGRES_PASSWORD=postgres \
    -e POSTGRES_DB=nuclaw \
    -p 5432:5432 \
    postgres:15

export DATABASE_URL=postgresql://postgres:postgres@localhost:5432/nuclaw
```

---

## 故障排除

### 编译错误

**错误：** `fatal error: boost/beast.hpp: No such file`
```bash
# Ubuntu/Debian
sudo apt-get install libboost-all-dev

# macOS
brew install boost
```

**错误：** `libpqxx not found`
```bash
# Ubuntu/Debian
sudo apt-get install libpqxx-dev

# 如果仍有问题，检查 pkg-config
pkg-config --modversion libpqxx
```

**错误：** `gtest not found`
```bash
# Ubuntu/Debian
sudo apt-get install libgtest-dev
```

### 运行时错误

**错误：** `LLM not configured`
- 检查是否设置了 `OPENAI_API_KEY` 或 `MOONSHOT_API_KEY`

**错误：** `Database connection failed`
- 检查 `DATABASE_URL` 是否正确
- 确认 PostgreSQL 服务是否运行
- 检查防火墙是否允许连接

**错误：** `Connection refused` (端口 8080)
- 检查端口是否被占用：`lsof -i :8080`
- 更换端口或关闭占用程序

---

## 架构文档

更多技术细节：[architecture.md](architecture.md)
