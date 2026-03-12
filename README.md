# NuClaw

> 用 C++17 从零构建生产级 AI Agent 网关 —— 渐进式教程

[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://isocpp.org/std/the-standard)
[![CMake](https://img.shields.io/badge/CMake-3.14+-green.svg)](https://cmake.org/)
[![License](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

## 项目定位

NuClaw 是一个**教学性质的 C++ AI Agent 网关项目**，从最简单的 HTTP Echo 服务器（50 行代码）开始，逐步演进到支持多租户、LLM 集成、实时通信的生产级架构（1100+ 行）。

**设计理念：**
- 🎯 **渐进式学习**：每章都是完整可运行的代码，代码量逐步增长
- 🔧 **核心优先**：Agent Loop、Tools、多租户等核心架构深度讲解（8/14 章）
- 🏭 **生产导向**：代码质量可直接用于生产环境
- 📚 **深入浅出**：详细中文注释，每行代码都有解释

## 架构演进

```
Step 0 (50行)       Step 3 (250行)      Step 6 (480行)      Step 9 (720行)      Step 12 (960行)     Step 14 (1100+行)
    │                   │                   │                   │                   │                   │
    ▼                   ▼                   ▼                   ▼                   ▼                   ▼
基础网络  ────▶  Agent核心  ────▶  Tools系统  ────▶  LLM集成  ────▶  多租户  ────▶  生产部署
    │                   │                   │                   │                   │                   │
    ├─ HTTP Echo       ├─ 状态机           ├─ 同步工具         ├─ 多模型接入       ├─ 数据隔离         ├─ Channel集成
    ├─ 异步I/O         ├─ 执行引擎         ├─ 异步执行         ├─ 流式响应         ├─ 租户治理         ├─ JWT认证
    └─ CMake           ├─ 高级特性         └─ 工具生态         └─ Fallback         └─ 连接池          └─ Docker部署
```

## 14章完整教程

### 第一阶段：网络基础（3章）

| 章节 | 主题 | 代码量 | 核心内容 |
|:---:|:---|:---:|:---|
| **Step 0** | HTTP Echo 服务器 | ~50行 | Boost.Asio 基础、同步 TCP、g++ 编译 |
| **Step 1** | 异步 I/O + CMake | ~120行 | async/await、Session 模式、CMake 基础 |
| **Step 2** | HTTP 长连接优化 | ~180行 | Keep-Alive、HTTP 协议完整解析 |

### 第二阶段：Agent 核心（3章）⭐ 重点

| 章节 | 主题 | 代码量 | 核心内容 |
|:---:|:---|:---:|:---|
| **Step 3** | Agent Loop（上）：状态机 | ~250行 | 对话状态机、历史管理、上下文窗口 |
| **Step 4** | Agent Loop（中）：执行引擎 | ~320行 | 循环检测、并行执行、中断处理 |
| **Step 5** | Agent Loop（下）：高级特性 | ~400行 | 记忆系统、摘要生成、质量门控 |

### 第三阶段：Tools 系统（3章）⭐ 重点

| 章节 | 主题 | 代码量 | 核心内容 |
|:---:|:---|:---:|:---|
| **Step 6** | Tools（上）：同步工具 | ~480行 | Tool Schema、注册表、参数校验 |
| **Step 7** | Tools（中）：异步工具 | ~560行 | 异步执行、超时控制、并发限制 |
| **Step 8** | Tools（下）：工具生态 | ~640行 | HTTP 工具、代码执行、安全沙箱 |

### 第四阶段：LLM 集成（2章）

| 章节 | 主题 | 代码量 | 核心内容 |
|:---:|:---|:---:|:---|
| **Step 9** | LLM Provider（上）：多模型 | ~720行 | OpenAI/Claude/本地模型、统一接口 |
| **Step 10** | LLM Provider（下）：流式 | ~800行 | SSE 流式、Fallback、负载均衡 |

### 第五阶段：企业级功能（2章）⭐ 重点

| 章节 | 主题 | 代码量 | 核心内容 |
|:---:|:---|:---:|:---|
| **Step 11** | 多租户（上）：数据隔离 | ~880行 | Schema-per-Tenant、连接池、性能 |
| **Step 12** | 多租户（下）：租户治理 | ~960行 | 生命周期、配额、计费、迁移 |

### 第六阶段：连接与部署（2章）

| 章节 | 主题 | 代码量 | 核心内容 |
|:---:|:---|:---:|:---|
| **Step 13** | Channel 集成 | ~1040行 | 飞书实战、钉钉简述、抽象层设计 |
| **Step 14** | 生产部署 | ~1120行 | JWT、TLS、Docker、监控（K8s 扩展阅读）|

## 快速开始

### 环境要求

- C++17 编译器 (GCC 9+, Clang 10+, MSVC 2019+)
- Boost 1.70+
- CMake 3.14+ (Step 1 起需要)
- PostgreSQL 12+ (Step 11 起需要)

### 安装依赖

**Ubuntu/Debian:**
```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake libboost-all-dev libssl-dev libpqxx-dev
```

**macOS:**
```bash
brew install cmake boost openssl libpqxx
```

### Step 0 编译运行（无 CMake）

```bash
# 克隆项目
git clone https://github.com/chapin666/NuClaw.git
cd NuClaw

# 编译 Step 0（使用 g++，无需 CMake）
cd src/step00
g++ -std=c++17 main.cpp -o server -lboost_system -lpthread

# 运行
./server

# 测试（另一个终端）
curl http://localhost:8080
```

### Step 1+ 使用 CMake

```bash
# 从 Step 1 开始，使用 CMake 构建
mkdir build && cd build
cmake ..
make -j$(nproc)

# 运行指定步骤
./nuclaw_step01
```

## 技术栈

| 组件 | 选型 | 用途 |
|:---|:---|:---|
| 网络 I/O | **Boost.Asio** | 异步 TCP/UDP |
| HTTP/WebSocket | **Boost.Beast** | HTTP 协议、WebSocket |
| JSON | **Boost.JSON** | 序列化/反序列化 |
| SSL/TLS | **OpenSSL** | HTTPS 加密 |
| 数据库 | **PostgreSQL + libpqxx** | 多租户数据存储 |
| 配置 | **yaml-cpp** | YAML 配置文件 (Step 14) |
| 日志 | **spdlog** | 结构化日志 (Step 14) |
| 监控 | **prometheus-cpp** | 指标采集 (Step 14) |
| 构建 | **CMake** | 跨平台构建 |

## 与 GoClaw 的关系

| 特性 | NuClaw (教程) | GoClaw (生产) |
|:---|:---|:---|
| 语言 | C++17 | Go |
| 并发模型 | 异步 I/O + 线程池 | Goroutines |
| 代码行数 | 50→1120 行 | 完整项目 |
| 目标 | **教学 + 渐进理解** | 生产使用 |
| Agent Loop | ✅ 3 章深度展开 | 内置实现 |
| Tools 系统 | ✅ 3 章深度展开 | 内置 + 扩展 |
| 多租户 | ✅ 2 章完整讲解 | Schema-per-Tenant |
| Channel | ⚠️ 2 章（飞书详细） | 6+ 平台完整支持 |

**NuClaw 不是 GoClaw 的替代品，而是学习工具。** 通过逐步实现，理解 AI Agent 网关的核心架构原理。

## 项目结构

```
NuClaw/
├── README.md                 # 本文件
├── LICENSE                   # MIT License
├── CMakeLists.txt           # 根构建配置
├── docs/                    # 教程文档
│   ├── overview.md         # 架构概述
│   ├── step00.md ~ step14.md  # 各章节详细教程
│   └── faq.md              # 常见问题
├── src/                     # 源码
│   ├── step00/ ~ step14/  # 各步骤代码
│   └── common/             # 公共工具（可选）
├── examples/               # 示例脚本
│   ├── test_step00.sh
│   └── ...
└── docker/                 # Docker 配置 (Step 14)
    ├── Dockerfile
    └── docker-compose.yml
```

## 学习路径建议

### 路径 A：完整学习（推荐）
按顺序完成 Step 0→14，每章代码都运行一遍，预计 2-3 周

### 路径 B：核心优先
只学核心章节：Step 0-2 → Step 3-5 → Step 6-8 → Step 11-12，预计 1 周

### 路径 C：快速体验
Step 0 → Step 3 → Step 5 → Step 9 → Step 11，了解整体架构

## 常见问题

### Q: 编译报错 `boost/asio.hpp: No such file`

A: 安装 Boost 库：
```bash
sudo apt-get install libboost-all-dev  # Ubuntu
brew install boost                      # macOS
```

### Q: Step 11 起需要 PostgreSQL，没有环境怎么办？

A: 使用 Docker 快速启动：
```bash
docker run -d --name nuclaw-postgres \
  -e POSTGRES_PASSWORD=password \
  -p 5432:5432 postgres:14
```

### Q: 代码量增长是否合理？

A: 每章增加 60-80 行，节奏平缓。核心章节（Agent/Tools/多租户）深度展开，连接章节（Channel）精简。

## 贡献

欢迎 Issue 和 PR！教学项目需要大家一起完善。

1. Fork 本项目
2. 创建 feature 分支 (`git checkout -b feature/amazing-feature`)
3. 提交更改 (`git commit -m 'Add amazing feature'`)
4. 推送分支 (`git push origin feature/amazing-feature`)
5. 创建 Pull Request

## 许可证

[MIT](LICENSE) © NuClaw Authors

---

> **Why Nu?** Nu = New，希腊字母 N 读作 "New"。从零开始，全新构建。
