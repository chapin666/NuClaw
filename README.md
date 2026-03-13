# NuClaw

> 用 C++17 从零构建 AI Agent —— 渐进式教程

[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://isocpp.org/std/the-standard)
[![CMake](https://img.shields.io/badge/CMake-3.14+-green.svg)](https://cmake.org/)
[![License](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

## 🎯 项目简介

NuClaw 是一个**渐进式 C++ AI Agent 教程项目**。

**核心特点：**
- 📈 **代码演进**：每章基于前一章，`git diff` 可见变化
- 🎯 **问题驱动**：每章解决前一章的实际问题
- 📚 **循序渐进**：从 89 行单文件到 700 行模块化

## 🚀 快速开始

```bash
git clone https://github.com/chapin666/NuClaw.git
cd NuClaw

# Step 0: 最简单的 Echo 服务器
cd src/step00
g++ -std=c++17 main.cpp -o server -lboost_system -lpthread
./server

# 测试
curl http://localhost:8080/hello
```

## 📖 教程大纲

### Part 1: 网络基础（Step 0-2）

| Step | 主题 | 核心概念 | 代码量 |
|:---:|:---|:---|:---:|
| **0** | [Echo 服务器](docs/step00/tutorial.md) | Boost.Asio, 同步 TCP | 89行 |
| **1** | [异步 I/O](docs/step01/tutorial.md) | Session, async_read/write | 184行 |
| **2** | [HTTP 协议](docs/step02/tutorial.md) | HTTP 解析, Router | 271行 |

**学习重点：**
- C++ 网络编程基础
- 异步编程模型
- HTTP 协议详解

---

### Part 2: Agent 核心（Step 3-5）

| Step | 主题 | 核心概念 | 代码量 |
|:---:|:---|:---|:---:|
| **3** | [规则 AI](docs/step03/tutorial.md) | Agent Loop, 正则匹配 | 350行 |
| **4** | [WebSocket](docs/step04/tutorial.md) | 实时通信, 上下文保持 | 400行 |
| **5** | [LLM 接入](docs/step05/tutorial.md) | API 调用, Prompt 工程 | 450行 |

**问题驱动演进：**

```
Step 3 规则系统                    Step 4 WebSocket                Step 5 LLM
├── 能实现 AI 对话                  ├── 解决 HTTP 无状态问题          ├── 解决规则死板问题
├── 但 HTTP 无状态                  ├── 实现持久上下文               ├── 接入真实 LLM
└── 每次请求上下文丢失              └── 支持多轮对话                 └── 支持自然语言理解
```

**学习重点：**
- Agent Loop 设计
- WebSocket 协议与实时通信
- LLM API 集成

---

### Part 3: Tools 系统（Step 6-8）

| Step | 主题 | 核心概念 | 代码量 |
|:---:|:---|:---|:---:|
| **6** | [工具调用](docs/step06/tutorial.md) | 工具接口, 硬编码执行 | 550行 |
| **7** | [异步工具](docs/step07/tutorial.md) | 并发控制, 超时机制 | 600行 |
| **8** | [安全沙箱](docs/step08/tutorial.md) | SSRF/路径防护, 审计 | 700行 |

**问题驱动演进：**

```
Step 6 工具系统                   Step 7 异步执行                 Step 8 安全沙箱
├── 让 LLM 能"做事"                ├── 解决同步阻塞问题             ├── 解决安全问题
├── 但工具是硬编码                  ├── 实现并发控制                 ├── URL/路径白名单
└── 每加工具要改核心代码           └── 支持超时取消                 └── 代码注入防护
```

**学习重点：**
- 工具系统设计模式
- 异步并发编程
- 安全防护机制

---

### 代码结构

```
# Step 0-5: 单文件（简单概念）
src/step0X/
├── main.cpp      (< 500行)
└── CMakeLists.txt

# Step 6-8: 模块化（复杂系统）
src/step0X/
├── main.cpp
├── chat_engine.hpp      # Agent 核心
├── llm_client.hpp       # LLM 调用
├── tool_executor.hpp    # 工具执行
├── *_tool.hpp           # 具体工具
└── ...
```

---

## 🛠️ 编译

```bash
# 单文件章节 (0-5)
cd src/step03
g++ -std=c++17 main.cpp -o server -lboost_system -lpthread

# 模块化章节 (6-8)
cd src/step06
mkdir build && cd build
cmake .. && make
./nuclaw_step06
```

## 📊 当前状态

| 阶段 | 进度 | 代码行数 |
|:---|:---:|:---:|
| Part 1: 网络基础 | ✅ 完成 | 89-271 |
| Part 2: Agent 核心 | ✅ 完成 | 350-450 |
| Part 3: Tools 系统 | ✅ 完成 | 550-700 |
| Part 4: 企业功能 | ⏳ 计划中 | - |

## 📄 许可证

MIT © NuClaw Authors

---

**设计理念**：不是堆砌功能，而是解决问题。每章都有明确的「前一章问题 → 本章解决 → 暴露新问题」循环。
