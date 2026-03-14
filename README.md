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
| **0** | [Echo 服务器](tutorials/step00/tutorial.md) | Boost.Asio, 同步 TCP | 89行 |
| **1** | [异步 I/O](tutorials/step01/tutorial.md) | Session, async_read/write | 184行 |
| **2** | [HTTP 协议](tutorials/step02/tutorial.md) | HTTP 解析, Router | 271行 |

**学习重点：** C++ 网络编程基础、异步编程模型、HTTP 协议详解

---

### Part 2: Agent 核心（Step 3-5）

| Step | 主题 | 核心概念 | 代码量 |
|:---:|:---|:---|:---:|
| **3** | [规则 AI](tutorials/step03/tutorial.md) | Agent Loop, 正则匹配 | 350行 |
| **4** | [WebSocket](tutorials/step04/tutorial.md) | 实时通信, 上下文保持 | 400行 |
| **5** | [LLM 接入](tutorials/step05/tutorial.md) | API 调用, Prompt 工程 | 450行 |

**学习重点：** Agent Loop 设计、WebSocket 协议、LLM API 集成

---

### Part 3: Tools 系统（Step 6-8）

| Step | 主题 | 核心概念 | 代码量 |
|:---:|:---|:---|:---:|
| **6** | [工具调用](tutorials/step06/tutorial.md) | 工具接口, 硬编码执行 | 550行 |
| **7** | [异步工具](tutorials/step07/tutorial.md) | 并发控制, 超时机制 | 600行 |
| **8** | [安全沙箱](tutorials/step08/tutorial.md) | SSRF/路径防护, 审计 | 700行 |

**学习重点：** 工具系统设计模式、异步并发编程、安全防护机制

---

### Part 4: 高级 Agent（Step 9-11）

| Step | 主题 | 核心概念 | 代码量 | 状态 |
|:---:|:---|:---|:---:|:---:|
| **9** | [工具注册表](tutorials/step09/tutorial.md) | 注册表模式, 依赖注入 | ~750行 | ✅ 完成 |
| **10** | [RAG 检索](tutorials/step10/tutorial.md) | 向量数据库, Embedding | ~800行 | ✅ 完成 |
| **11** | [多 Agent 协作](tutorials/step11/tutorial.md) | Agent 通信, 任务分发 | ~850行 | ✅ 完成 |

**学习重点：**
- **Step 9**: 使用注册表模式解决硬编码问题，实现插件化工具系统
- **Step 10**: 接入向量数据库，实现 RAG（检索增强生成），减少 LLM 幻觉
- **Step 11**: 多 Agent 架构，实现 Agent 间的协作与任务分配

---

### Part 5: 生产就绪（Step 12-14）

| Step | 主题 | 核心概念 | 代码量 | 状态 |
|:---:|:---|:---|:---:|:---:|
| **12** | [配置管理](tutorials/step12/tutorial.md) | YAML/JSON 配置, 热加载 | ~900行 | ✅ 完成 |
| **13** | [监控告警](tutorials/step13/tutorial.md) | Metrics, Logging, Tracing | ~950行 | ✅ 完成 |
| **14** | [部署运维](tutorials/step14/tutorial.md) | Docker, K8s, CI/CD | ~1000行 | ✅ 完成 |

**学习重点：**
- **Step 12**: 外部化配置，支持热加载，无需重启修改参数
- **Step 13**: 可观测性三大支柱：指标监控、日志聚合、链路追踪
- **Step 14**: 容器化部署、Kubernetes 编排、自动化 CI/CD 流程

---

### Part 6: IM 平台接入（Step 15）

| Step | 主题 | 核心概念 | 代码量 | 状态 |
|:---:|:---|:---|:---:|:---:|
| **15** | [IM 平台接入](tutorials/step15/tutorial.md) | 飞书/钉钉/企微/Telegram 集成 | ~1100行 | 📝 已规划 |

**学习重点：**
- 多平台消息格式统一抽象
- Webhook、事件订阅、长连接等接入模式
- 群聊 @提及、私聊、富文本处理

---

### 实战案例（Step 16-17）

| Step | 主题 | 核心概念 | 代码量 | 状态 |
|:---:|:---|:---|:---:|:---:|
| **16** | [期中实战 — 旅行小管家](tutorials/step16/tutorial.md) | 智能旅行规划助手 | ~1200行 | 📝 已规划 |
| **17** | [期末实战 — 虚拟咖啡厅「星语轩」](tutorials/step17/tutorial.md) | 多 NPC 情感社交模拟 | ~1500行 | 📝 已规划 |

**学习重点：**
- **Step 16**: 贴近生活的智能旅行助手，整合 RAG + 工具调用 + 用户画像
- **Step 17**: 有趣的虚拟社交空间，多 Agent 情感计算、关系网络、故事驱动

---

### 代码结构

```
# Step 0-5: 单文件（简单概念）
src/step0X/
├── main.cpp      (< 500行)
└── CMakeLists.txt

# Step 6-14: 模块化（复杂系统）
src/step0X/
├── main.cpp
├── chat_engine.hpp      # Agent 核心
├── llm_client.hpp       # LLM 调用
├── tool_executor.hpp    # 工具执行
├── *_tool.hpp           # 具体工具
├── config.hpp           # 配置管理 (Step 12+)
├── metrics.hpp          # 监控指标 (Step 13+)
└── ...
```

---

## 🛠️ 编译

```bash
# 单文件章节 (0-5)
cd src/step03
g++ -std=c++17 main.cpp -o server -lboost_system -lpthread

# 模块化章节 (6-14)
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
| Part 4: 高级 Agent | ✅ 完成 | 750-850 |
| Part 5: 生产就绪 | ✅ 完成 | 900-1000 |
| Part 6: IM 平台接入 | 📝 已规划 | ~1100 |
| 实战案例 | 📝 已规划 | 1200-1500 |

---

## 🎉 学习路径总结

```
Step 0-2: 网络基础 → 掌握 C++ 网络编程
Step 3-5: Agent 核心 → 理解 Agent Loop 和 LLM 集成
Step 6-8: Tools 系统 → 工具调用、异步执行、安全防护
Step 9-11: 高级 Agent → 注册表、RAG、多 Agent 协作
Step 12-14: 生产就绪 → 配置管理、监控告警、容器化部署
Step 15: IM 平台接入 → 飞书/钉钉/企微/Telegram 集成
Step 16: 期中实战 → 智能客服机器人
Step 17: 期末实战 → 企业级多 Agent 协作平台
```

**代码演进：**
```
89行 (Step 0) → 1500+行 (Step 17)
单文件 → 模块化 → 配置化 → 容器化 → 企业级
```

## 📄 许可证

MIT © NuClaw Authors

---

**设计理念**：不是堆砌功能，而是解决问题。每章都有明确的「前一章问题 → 本章解决 → 暴露新问题」循环。
