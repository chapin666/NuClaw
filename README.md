# NuClaw

> 用 C++17 从零构建生产级 AI Agent 网关 —— 深入浅出教程

NuClaw 是一个教学性质的 C++ AI Agent 网关项目，从最简单的 HTTP Echo 服务器（50 行代码）开始，逐步演进到支持多租户、LLM 集成、WebSocket 实时通信的生产级架构。

## 设计理念

- **渐进式学习**：每步都是完整可运行的代码，代码量从 50 行增长到 300 行
- **简洁优先**：专注核心架构，不堆砌 Provider 和 Channel（保持扩展性）
- **生产导向**：代码质量可直接用于生产环境
- **GoClaw 灵感**：架构设计参考 GoClaw，但用 C++ 原生实现

## 架构演进

```
Step 0 (50行)     Step 1 (100行)    Step 2 (150行)    Step 3 (200行)
┌─────────┐       ┌─────────┐       ┌─────────┐       ┌─────────┐
│  HTTP   │  ──▶  │  HTTP   │  ──▶  │  Async  │  ──▶  │WebSocket│
│  Echo   │       │ Routing │       │  I/O    │       │+ Agent  │
└─────────┘       └─────────┘       └─────────┘       └─────────┘
   同步阻塞          请求解析           高并发支持         全双工通信

Step 4 (250行)    Step 5 (300行)
┌─────────┐       ┌─────────┐
│  LLM    │  ──▶  │ Multi   │
│  API    │       │ Tenant  │
└─────────┘       └─────────┘
  OpenAI集成      Schema隔离
```

## 快速开始

```bash
# 克隆项目
git clone https://github.com/chapin666/NuClaw.git
cd NuClaw

# 安装依赖 (Ubuntu/Debian)
sudo apt-get install libboost-all-dev libssl-dev libpqxx-dev

# 构建
mkdir build && cd build
cmake ..
make -j$(nproc)

# 运行各步骤示例
./nuclaw_step00  # HTTP Echo
./nuclaw_step01  # HTTP Routing
./nuclaw_step02  # Async I/O
./nuclaw_step03  # WebSocket
./nuclaw_step04  # LLM Integration
./nuclaw_step05  # Multi-Tenant
```

## 目录结构

```
NuClaw/
├── README.md              # 本文件
├── docs/                  # 详细教程文档
│   ├── step00.md         # HTTP Echo 详解
│   ├── step01.md         # HTTP Routing 详解
│   ├── step02.md         # Async I/O 详解
│   ├── step03.md         # WebSocket + Agent Loop
│   ├── step04.md         # LLM 集成
│   └── step05.md         # 多租户架构
├── src/                   # 源码
│   ├── step00/           # Step 0 代码
│   ├── step01/
│   ├── step02/
│   ├── step03/
│   ├── step04/
│   └── step05/
├── CMakeLists.txt        # CMake 构建配置
└── examples/             # 示例和测试脚本
```

## 技术栈

| 组件 | 选型 | 理由 |
|------|------|------|
| 网络 I/O | Boost.Asio | 异步 I/O 标准库 |
| HTTP/WebSocket | Boost.Beast | 基于 Asio，功能完整 |
| JSON | Boost.JSON | 无需额外依赖 |
| SSL | OpenSSL | 标准 TLS 实现 |
| 数据库 | PostgreSQL + libpqxx | 生产级多租户支持 |
| 构建 | CMake | 跨平台标准 |

## 与 GoClaw 的关系

| 特性 | NuClaw | GoClaw |
|------|--------|--------|
| 语言 | C++17 | Go |
| 并发 | 异步 I/O + 线程池 | Goroutines |
| 目标 | 教学 + 生产 | 生产 |
| 代码量 | 50-300 行/步 | 完整项目 |
| 扩展 | 保持简洁，易于理解 | 功能完整 |

NuClaw 不是 GoClaw 的替代品，而是**学习工具**。通过逐步实现，理解 AI Agent 网关的核心架构原理。

## 教程大纲

### [Step 0: HTTP Echo 服务器](docs/step00.md)
- Boost.Asio 基础
- 同步 TCP 服务器
- HTTP 协议基础

### [Step 1: HTTP 路由系统](docs/step01.md)
- HTTP 请求解析
- 路由分发器
- RESTful API 设计

### [Step 2: 异步 I/O 和多线程](docs/step02.md)
- Asio 异步模型
- Session 模式
- 多线程 io_context

### [Step 3: WebSocket 和 Agent Loop](docs/step03.md)
- WebSocket 协议
- Agent Loop 架构
- 会话状态管理

### [Step 4: LLM 集成](docs/step04.md)
- HTTPS 客户端
- OpenAI API 调用
- 流式响应 (SSE)

### [Step 5: 多租户架构](docs/step05.md)
- Schema-per-Tenant
- PostgreSQL 数据隔离
- 租户生命周期管理

## 扩展方向

- [ ] 工具系统 (Tool Calling)
- [ ] 多模型支持 (OpenAI, Claude, Local)
- [ ] 消息队列集成 (Redis/RabbitMQ)
- [ ] 分布式部署 (gRPC)
- [ ] 监控指标 (Prometheus)

## 贡献

欢迎 Issue 和 PR！教学项目需要大家一起完善。

## 许可证

MIT License - 自由使用和修改。

---

> **Why Nu?** Nu = New，希腊字母 N 读作 "New"。从零开始，全新构建。
