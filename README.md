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
- 📚 **循序渐进**：从 89 行单文件到 1500+ 行完整项目

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

### Part 1: 基础构建（Step 0-5）

| Step | 主题 | 核心概念 | 代码量 |
|:---:|:---|:---|:---:|
| **0** | [Echo 服务器](tutorials/step00/tutorial.md) | Boost.Asio, 同步 TCP | 89行 |
| **1** | [异步 I/O](tutorials/step01/tutorial.md) | Session, async_read/write | 184行 |
| **2** | [HTTP 协议](tutorials/step02/tutorial.md) | HTTP 解析, Router | 271行 |
| **3** | [规则 AI](tutorials/step03/tutorial.md) | Agent Loop, 正则匹配 | 350行 |
| **4** | [WebSocket](tutorials/step04/tutorial.md) | 实时通信, 上下文保持 | 400行 |
| **5** | [LLM 接入](tutorials/step05/tutorial.md) | API 调用, Prompt 工程 | 450行 |

**学习重点：** C++ 网络编程 → Agent Loop 设计 → LLM 集成

**阶段目标：** 让 Agent 能跑起来，能对话

---

### Part 2: 能力扩展（Step 6-11）

| Step | 主题 | 核心概念 | 代码量 | 状态 |
|:---:|:---|:---|:---:|:---:|
| **6** | [工具调用](tutorials/step06/tutorial.md) | 工具接口, 硬编码执行 | 550行 | ✅ 完成 |
| **7** | [异步工具](tutorials/step07/tutorial.md) | 并发控制, 超时机制 | 600行 | ✅ 完成 |
| **8** | [安全沙箱](tutorials/step08/tutorial.md) | SSRF/路径防护, 审计 | 700行 | ✅ 完成 |
| **9** | [工具注册表](tutorials/step09/tutorial.md) | 注册表模式, 依赖注入 | ~750行 | ✅ 完成 |
| **10** | [RAG 检索](tutorials/step10/tutorial.md) | 向量数据库, Embedding | ~800行 | ✅ 完成 |
| **11** | [多 Agent 协作](tutorials/step11/tutorial.md) | Agent 通信, 任务分发 | ~850行 | ✅ 完成 |

**学习重点：** 工具系统 → 安全防护 → 注册表模式 → RAG 检索 → 多 Agent 协作

**阶段目标：** 让 Agent 能做更多事，更聪明

---

### Part 3: 产品化（Step 12-16）

| Step | 主题 | 核心概念 | 代码量 | 状态 |
|:---:|:---|:---|:---:|:---:|
| **12** | [配置管理](tutorials/step12/tutorial.md) | YAML/JSON 配置, 热加载 | ~900行 | ✅ 完成 |
| **13** | [监控告警](tutorials/step13/tutorial.md) | Metrics, Logging, Tracing | ~950行 | ✅ 完成 |
| **14** | [部署运维](tutorials/step14/tutorial.md) | Docker, K8s, CI/CD | ~1000行 | ✅ 完成 |
| **15** | [IM 平台接入](tutorials/step15/tutorial.md) | 飞书/钉钉/企微/Telegram | ~1100行 | ✅ 完成 |
| **16** | [Agent 状态与记忆](tutorials/step16/tutorial.md) | 情感计算, 记忆系统 | ~1200行 | ✅ 完成 |

**学习重点：** 生产就绪（配置/监控/部署）→ 连接真实世界（IM）→ 赋予灵魂（状态/记忆）

**阶段目标：** 让 Agent 成为可上线的产品

---

### Part 4: 项目实战（Step 17-20）—— 智能客服 SaaS 平台

| Step | 主题 | 核心概念 | 代码量 | 状态 |
|:---:|:---|:---|:---:|:---:|
| **17** | [需求分析与架构设计](tutorials/step17/tutorial.md) | 需求分析、系统架构、技术选型 | ~2000行 | ✅ 完成 |
| **18** | [核心功能实现](tutorials/step18/tutorial.md) | Chat Service、AI Service、Knowledge Service | ~1500行 | ✅ 完成 |
| **19** | [高级功能实现](tutorials/step19/tutorial.md) | 人机协作、多租户、计费、Admin | ~1200行 | ✅ 完成 |
| **20** | [生产部署](tutorials/step20/tutorial.md) | Docker、K8s、监控、压测 | ~800行 | ✅ 完成 |

**项目：SmartSupport —— 智能客服 SaaS 平台**

一个基于前面所有章节知识构建的完整商业级 SaaS 产品：

```
核心价值：
• 7×24 小时 AI 自动回复，降低 80% 人工成本
• 多平台统一接入（网站/微信/飞书/钉钉）
• 人机协作，复杂问题无缝转人工
• 多租户隔离，数据安全有保障

技术架构：
├── Chat Service（WebSocket 会话管理）
├── AI Service（Agent + RAG + 工具调用）
├── Knowledge Service（向量检索）
├── Human Service（人工客服工作台）
├── Tenant Service（多租户隔离）
├── Billing Service（计费与套餐）
└── Admin Dashboard（运营管理）
```

---

## 🎨 项目实战展示

### 🏢 SmartSupport —— 智能客服 SaaS 平台

**生产级多租户智能客服平台**，综合运用 Step 0-16 全部技术栈。

**系统架构：**
```
┌─────────────────────────────────────────────────────────────┐
│                        接入层 (Gateway)                      │
│  WebSocket / HTTP / 飞书 Bot / 钉钉 Bot / Telegram Bot       │
└──────────────────────────┬──────────────────────────────────┘
                           │
┌──────────────────────────▼──────────────────────────────────┐
│                        服务层 (Services)                     │
│  ┌────────────┐ ┌────────────┐ ┌────────────┐              │
│  │   Chat     │ │    AI      │ │ Knowledge  │              │
│  │  Service   │ │  Service   │ │  Service   │              │
│  └────────────┘ └────────────┘ └────────────┘              │
│  ┌────────────┐ ┌────────────┐ ┌────────────┐              │
│  │   Human    │ │   Tenant   │ │  Billing   │              │
│  │  Service   │ │  Service   │ │  Service   │              │
│  └────────────┘ └────────────┘ └────────────┘              │
└──────────────────────────┬──────────────────────────────────┘
                           │
┌──────────────────────────▼──────────────────────────────────┐
│                        数据层 (Data)                         │
│  PostgreSQL + pgvector │ Redis │ MinIO (对象存储)            │
└─────────────────────────────────────────────────────────────┘
```

**核心功能：**

| 功能模块 | 说明 | 对应章节 |
|:---|:---|:---:|
| **AI 自动回复** | RAG + LLM 智能回复，支持知识库检索 | Step 5, 10 |
| **人工客服** | 智能分配、客服工作台、会话接管 | Step 19 |
| **多租户** | 数据隔离、资源配额、成员管理 | Step 19 |
| **计费系统** | 套餐管理、用量统计、发票生成 | Step 19 |
| **IM 接入** | 飞书/钉钉/企微/Telegram 统一接入 | Step 15 |
| **监控告警** | Prometheus + Grafana 全链路监控 | Step 13 |

**技术亮点：**
- 多租户数据隔离（RLS + 租户上下文）
- WebSocket 实时通信 + 消息路由
- RAG 知识检索 + 置信度评估
- 人机协作智能分配算法
- K8s 微服务部署 + HPA 自动扩缩容

**业务场景：**
```
租户 A（电商公司）      租户 B（教育机构）
├── 知识库：产品 FAQ     ├── 知识库：课程资料
├── 客服：小李、小王     ├── 教师：张老师
├── 接入：官网、微信     └── 接入：官网、钉钉
└── AI 自动回复 90% 问题

平台运营方（你）
├── 租户管理、套餐定价
├── 计费结算、财务报表
└── 系统监控、运维支持
```

---

### 代码结构

```
# Step 0-5: 单文件（简单概念）
src/step0X/
├── main.cpp      (< 500行)
└── CMakeLists.txt

# Step 6-16: 模块化（复杂系统）
src/step0X/
├── main.cpp
├── chat_engine.hpp      # Agent 核心
├── llm_client.hpp       # LLM 调用
├── tool_executor.hpp    # 工具执行
├── *_tool.hpp           # 具体工具
├── config.hpp           # 配置管理 (Step 12+)
├── metrics.hpp          # 监控指标 (Step 13+)
└── ...

# Step 17-20: 微服务架构（SaaS 平台）
src/step0X/
├── CMakeLists.txt
├── include/
│   └── smartsupport/
│       ├── common/
│       │   ├── types.hpp
│       │   └── database.hpp
│       └── services/
│           ├── chat/
│           ├── ai/
│           ├── knowledge/
│           ├── human/       # Step 19+
│           ├── tenant/      # Step 19+
│           └── billing/     # Step 19+
├── src/
│   ├── main.cpp
│   ├── common/
│   └── services/
├── config/
├── k8s/                   # Step 20
└── tests/
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

# 演示程序 (demos)
cd ../../..
mkdir build && cd build
cmake .. && make -j4
./step06_rest_demo
```

## 📊 当前状态

| 阶段 | 进度 | 代码行数 | 章节数 |
|:---|:---:|:---:|:---:|
| Part 1: 基础构建 | ✅ 完成 | 89-450 | 6 |
| Part 2: 能力扩展 | ✅ 完成 | 550-850 | 6 |
| Part 3: 产品化 | ✅ 完成 | 900-1200 | 5 |
| Part 4: 项目实战 | ✅ 完成 | 1500-2000 | 4 |
| **总计** | **✅ 全部完成** | **~80,000 字** | **21** |

---

## 🎉 学习路径总结

```
Part 1 (Step 0-5): 基础构建
    网络编程 → Agent Loop → LLM 集成
    目标：让 Agent 能跑起来，能对话

Part 2 (Step 6-11): 能力扩展
    工具系统 → 安全防护 → RAG → 多 Agent 协作
    目标：让 Agent 能做更多事，更聪明

Part 3 (Step 12-16): 产品化
    配置管理 → 监控告警 → 容器化部署 → IM 接入 → 状态记忆
    目标：让 Agent 成为可上线的产品

Part 4 (Step 17-20): 项目实战
    智能客服 SaaS 平台：架构设计 → 核心功能 → 高级功能 → 生产部署
    目标：综合运用，做出完整可商用的产品
```

**代码演进：**
```
89行 (Step 0) → 2000+行 (Step 20)
单文件 → 模块化 → 配置化 → 容器化 → 智能化 → SaaS化
```

## 📄 许可证

MIT © NuClaw Authors

---

**设计理念**：不是堆砌功能，而是解决问题。每章都有明确的「前一章问题 → 本章解决 → 暴露新问题」循环。
