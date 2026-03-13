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
Step 0      Step 3      Step 6      Step 9      Step 12     Step 14
(50行)      (250行)     (480行)     (720行)     (960行)     (1100+行)
   |           |           |           |           |           |
   v           v           v           v           v           v
基础网络  --> Agent核心 --> Tools系统 --> LLM集成  -->  多租户  --> 生产部署
   |           |           |           |           |           |
   |- HTTP     |- 状态机   |- 同步工具  |- 多模型   |- 数据隔离  |- Channel
   |- 异步I/O  |- 执行引擎 |- 异步执行  |- 流式响应  |- 租户治理  |- JWT
   |- CMake    |- 高级特性 |- 工具生态  |- Fallback  |- 连接池   |- Docker
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
| **Step 14** | 生产部署 | ~1120行 | JWT、TLS、Docker、监控 |

## 快速开始

### 环境要求

- C++17 编译器 (GCC 9+, Clang 10+, MSVC 2019+)
- Boost 1.70+
- CMake 3.14+ (Step 1+ 需要)
- OpenSSL (HTTPS 支持)

### 安装依赖

**Ubuntu/Debian:**
```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake libboost-all-dev libssl-dev
```

**macOS:**
```bash
brew install cmake boost openssl
```

### 使用 master 分支（推荐）

master 分支包含 Step 0-5 的完整代码和详细教程：

```bash
# 克隆项目
git clone https://github.com/chapin666/NuClaw.git
cd NuClaw

# Step 0: 直接用 g++ 编译
cd src/step00
g++ -std=c++17 main.cpp -o server -lboost_system -lpthread
./server

# Step 1+: 使用 CMake
cd src/step01
mkdir build && cd build
cmake .. && make
./nuclaw_step01
```

### 使用 feature 分支（单独学习某一步）

每个步骤都有独立的 feature 分支，只包含该步骤的内容：

```bash
# 学习 Step 3 (Agent Loop)
git checkout feature/step-03-agent-loop
ls src/  # 只有 step03/
cd src/step03
mkdir build && cd build
cmake .. && make
./nuclaw_step03
```

可用分支：
- `feature/step-00-http-echo` - 基础 HTTP 服务器
- `feature/step-01-async-cmake` - 异步 I/O
- `feature/step-02-keepalive` - HTTP 长连接
- `feature/step-03-agent-loop` - Agent 基础
- `feature/step-04-execution-engine` - 执行引擎
- `feature/step-05-advanced-features` - 高级特性

## 技术栈（Step 0-5）

| 组件 | 选型 | 用途 |
|:---|:---|:---|
| 网络 I/O | **Boost.Asio** | 异步 TCP/UDP |
| HTTP/WebSocket | **Boost.Beast** | HTTP 协议、WebSocket |
| JSON | **Boost.JSON** | 序列化/反序列化 |
| SSL/TLS | **OpenSSL** | HTTPS 加密 |
| 构建 | **CMake** | 跨平台构建 |

## 文档资源

| 步骤 | 教程 | 基础知识 |
|:---|:---|:---|
| **Step 0** | [tutorial.md](docs/step00/tutorial.md) | 网络编程、TCP/IP、Socket、HTTP 协议 |
| **Step 1** | [tutorial.md](docs/step01/tutorial.md) | 异步编程、智能指针、Lambda、CMake |
| **Step 2** | [tutorial.md](docs/step02/tutorial.md) | Keep-Alive 协议、定时器 |
| **Step 3** | [tutorial.md](docs/step03/tutorial.md) | Agent 概念、ReAct 模式、状态机 |
| **Step 4** | [tutorial.md](docs/step04/tutorial.md) | 执行策略、循环检测、工具安全 |
| **Step 5** | [tutorial.md](docs/step05/tutorial.md) | 记忆系统、上下文压缩、质量评估 |
| **Step 6** | [tutorial.md](docs/step06/tutorial.md) | Tool Schema、参数校验、注册表模式 |
| **Step 7** | [tutorial.md](docs/step07/tutorial.md) | 异步工具执行、并发控制 |
| **Step 8** | [tutorial.md](docs/step08/tutorial.md) | HTTP 客户端、文件沙箱、代码安全 |

## 项目结构

```
NuClaw/
├── README.md                 # 本文件
├── LICENSE                   # MIT License
├── docs/                     # 教程文档
│   ├── step00/              # Step 0 教程
│   │   └── tutorial.md      # 详细教程（网络编程基础）
│   ├── step01/              # Step 1 教程
│   │   └── tutorial.md      # 详细教程（异步I/O）
│   ├── step02/              # Step 2 教程
│   │   └── tutorial.md      # 详细教程（Keep-Alive）
│   ├── step03/              # Step 3 教程
│   │   └── tutorial.md      # 详细教程（Agent基础）
│   ├── step04/              # Step 4 教程
│   │   └── tutorial.md      # 详细教程（执行引擎）
│   └── step05/              # Step 5 教程
│       └── tutorial.md      # 详细教程（记忆系统）
├── src/                      # 源码
│   ├── step00/              # Step 0: HTTP Echo
│   │   └── main.cpp
│   ├── step01/              # Step 1: 异步I/O + CMake
│   │   ├── CMakeLists.txt
│   │   └── main.cpp
│   ├── step02/              # Step 2: Keep-Alive
│   │   ├── CMakeLists.txt
│   │   └── main.cpp
│   ├── step03/              # Step 3: Agent Loop
│   │   ├── CMakeLists.txt
│   │   └── main.cpp
│   ├── step04/              # Step 4: 执行引擎
│   │   ├── CMakeLists.txt
│   │   └── main.cpp
│   └── step05/              # Step 5: 高级特性
│       ├── CMakeLists.txt
│       └── main.cpp
└── LICENSE
```

## 学习路径建议

### 路径 A：完整学习（推荐，2-3 周）
```
Week 1: 网络基础
  - Step 0: HTTP Echo（同步 I/O）
  - Step 1: 异步 I/O + CMake
  - Step 2: Keep-Alive 长连接

Week 2: Agent Core
  - Step 3: Agent Loop 基础（状态机、WebSocket）
  - Step 4: 执行引擎（循环检测、并行执行）
  - Step 5: 高级特性（记忆、上下文压缩）

Week 3: Tools 系统（待完成）
  - Step 6-8: 工具调用、异步工具、安全沙箱
```

### 路径 B：核心优先（1 周）
如果时间有限，只学最核心内容：
```
Day 1-2: Step 0-2（网络基础）
Day 3-5: Step 3-5（Agent Core）
```

### 路径 C：快速体验（2 天）
只想了解 Agent 怎么工作：
```
Day 1: Step 0（运行起来）
Day 2: Step 3（理解 Agent Loop）
```

## 当前进度

| 阶段 | 状态 | 代码 | 教程 |
|:---|:---|:---|:---|
| 网络基础 (Step 0-2) | ✅ 完成 | 50-180 行 | 详细 |
| Agent Core (Step 3-5) | ✅ 完成 | 250-400 行 | 详细 |
| Tools 系统 (Step 6-8) | ✅ 完成 | 480-640 行 | 详细 |
| LLM 集成 (Step 9-10) | ⏳ 计划中 | - | - |
| 多租户 (Step 11-12) | ⏳ 计划中 | - | - |
| 生产部署 (Step 13-14) | ⏳ 计划中 | - | - |

## 常见问题

### Q: 编译报错 `boost/asio.hpp: No such file`

A: 安装 Boost 开发包：
```bash
sudo apt-get install libboost-all-dev  # Ubuntu
brew install boost                      # macOS
```

### Q: CMake 找不到 Boost

A: 指定 Boost 路径：
```bash
cmake -DBOOST_ROOT=/usr/local/boost ..
```

### Q: 哪个分支适合我？

A: 
- **初学者**：使用 `master` 分支，包含 Step 0-5 完整代码和教程
- **单独学习某一步**：使用 `feature/step-XX-xxx` 分支，内容更纯净
- **查看演进过程**：对比不同 feature 分支的代码差异

### Q: 教程看不懂怎么办？

A: 每个 Step 的 `docs/stepXX/tutorial.md` 都包含：
- 前置知识讲解（网络、C++、Agent 基础）
- 代码逐行解析
- 图示和类比
- 常见问题解答

建议按顺序学习 Step 0 → Step 1 → Step 2，不要跳过。

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
