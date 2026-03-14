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
| **15** | [IM 平台接入](tutorials/step15/tutorial.md) | 飞书/钉钉/企微/Telegram | ~1100行 | 📝 已规划 |
| **16** | [Agent 状态与记忆](tutorials/step16/tutorial.md) | 情感计算, 记忆系统 | ~1200行 | 📝 已规划 |

**学习重点：** 生产就绪（配置/监控/部署）→ 连接真实世界（IM）→ 赋予灵魂（状态/记忆）

**阶段目标：** 让 Agent 成为可上线的产品

---

### Part 4: 项目实战（Step 17-19）

| Step | 主题 | 核心概念 | 代码量 | 状态 |
|:---:|:---|:---|:---:|:---:|
| **17** | [旅行小管家](tutorials/step17/tutorial.md) | 用户画像、行程规划、记忆应用 | ~1300行 | ✅ 已规划 |
| **18** | [虚拟咖啡厅「星语轩」](tutorials/step18/tutorial.md) | 多 NPC、关系网络、自主决策 | ~1500行 | ✅ 已规划 |
| **19** | [SaaS 化 AI 助手平台](tutorials/step19/tutorial.md) | 多租户、认证授权、计费系统 | ~1800行 | ✅ 已规划 |

**学习重点：**
- **Step 17**: 单 Agent 实用应用（继承 Step 16 状态系统）
  - 用户画像建模
  - 意图识别与参数提取
  - 行程规划算法
  - 记忆在对话中的应用
  
- **Step 18**: 多 Agent 虚拟世界（扩展 Step 17 为多人互动）
  - NPC 自主决策系统
  - 关系网络建模与演化
  - 社交事件触发机制
  - 虚拟世界时间推进
  
- **Step 19**: SaaS 化平台（生产级架构）
  - 多租户数据隔离策略
  - OAuth2/API Key 认证体系
  - 按量计费与套餐管理
  - K8s 微服务部署

**阶段目标：** 综合运用，做出完整可用的产品

---

## 🎨 项目实战展示

### 🧳 Step 17: 旅行小管家

**智能旅行规划助手**，拥有记忆和情感的 AI 伙伴。

**核心功能：**
- 🧠 **记忆能力** - 记住你的偏好、历史行程、特殊需求
- 👤 **用户画像** - 旅行风格、预算范围、活动偏好
- 📍 **行程规划** - 智能推荐目的地、自动生成每日行程
- 💰 **预算管理** - 自动分配各项预算、实时跟踪
- 🎒 **行李清单** - 根据天气、活动、目的地智能生成

**技术亮点：**
- 继承 Step 16 的 `AgentState` + `MemoryManager`
- 意图识别（规则 + LLM 混合）
- 领域建模（旅行领域的抽象）

**交互示例：**
```
用户：我想去日本
小旅：日本！✨ 我记得你之前去过东京，
      这次想去关西吗？那边有京都、大阪...
      
用户：预算 1 万，5 天
小旅：收到！我知道你爱吃美食、喜欢悠闲行程，
      所以每天安排 2-3 个景点...
      等等，你脚伤刚恢复，我选了平坦的路线。
```

---

### ☕ Step 18: 虚拟咖啡厅「星语轩」

**多 Agent 虚拟世界**，5 个 NPC 有自己的生活、情感和关系。

**NPC 角色：**
- 👨‍🍳 **老王** - 42岁咖啡师，沉默寡言，暗恋林阿姨
- 🎨 **小雨** - 26岁插画师，活泼开朗，暗恋阿杰
- 💻 **阿杰** - 30岁程序员，技术宅，暗恋小雨
- 📚 **林阿姨** - 58岁退休教师，温暖，撮合年轻人
- 🐱 **橘子** - 店猫，高冷但会撒娇

**核心机制：**
- **关系网络** - NPC 之间的好感度、信任度动态变化
- **自主决策** - 每个 NPC 根据时间、地点、心情决定行动
- **社交事件** - 雨天长谈、告白、冲突、和好...
- **玩家影响** - 你可以观察、对话、甚至撮合他们

**技术亮点：**
- 继承 Step 17 的 `TravelerProfile` 作为 NPC 基类
- 世界引擎（时间、天气、空间管理）
- 社交事件系统（触发条件、执行流程、影响计算）

**交互示例：**
```
> 等待 30 分钟

[时间推进到 15:00]

突然外面开始下大雨 🌧️

老王看着窗外："看来一时半会儿停不了。"

小雨："哇——那我今天可以多画一会儿了！"

阿杰：（小声）"那我也...再待一会儿好了。"

林阿姨："下雨天最适合聊天了。"

[触发事件：雨天的长谈]
```

---

### 🏢 Step 19: SaaS 化 AI 助手平台

**生产级多租户平台**，让企业快速拥有智能客服。

**业务场景：**
```
租户 A（电商公司）      租户 B（教育机构）
├── 管理员：张总         ├── 管理员：李老师
├── 客服：小李、小王      ├── 教师：张老师
├── 知识库：产品 FAQ      ├── 知识库：课程资料
└── 接入：官网、微信      └── 接入：官网、钉钉

平台运营方（你）
├── 租户管理、套餐定价
├── 计费结算、财务报表
└── 系统监控、运维支持
```

**核心能力：**
- 🔐 **安全认证** - OAuth2、JWT、API Key 三级认证
- 🏢 **多租户隔离** - 数据、配置、资源完全隔离
- 👥 **权限管理** - 管理员、操作员、访客角色
- 💰 **计费系统** - 按量计费、套餐管理、发票生成
- 📊 **运营分析** - 租户健康度、收入报表、使用统计

**技术亮点：**
- 数据隔离策略（Database/Schema/Table Prefix）
- 租户级资源配额与限流
- 微服务架构 + K8s 部署

**API 示例：**
```bash
# 创建租户
curl -X POST /v1/admin/tenants \
  -H "Authorization: Bearer $PLATFORM_TOKEN" \
  -d '{"name":"示例科技","plan":"pro"}'

# 用户对话
curl -X POST /v1/chat \
  -H "Authorization: ApiKey sk_xxx" \
  -H "X-Tenant-ID: tenant_abc" \
  -d '{"message":"如何退款？"}'
```

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
| Part 1: 基础构建 | ✅ 完成 | 89-450 |
| Part 2: 能力扩展 | ✅ 完成 | 550-850 |
| Part 3: 产品化 | ✅ 部分完成 | 900-1200 |
| Part 4: 项目实战 | 📝 已规划 | 1300-1800 |

---

## 🎉 学习路径总结

```
Part 1 (Step 0-5): 基础构建
    网络编程 → Agent Loop → LLM 集成
    目标：让 Agent 能跑起来

Part 2 (Step 6-11): 能力扩展
    工具系统 → 安全防护 → RAG → 多 Agent 协作
    目标：让 Agent 能做更多事，更聪明

Part 3 (Step 12-16): 产品化
    配置管理 → 监控告警 → 容器化部署 → IM 接入 → 状态记忆
    目标：让 Agent 成为可上线的产品

Part 4 (Step 17-19): 项目实战
    旅行小管家 → 虚拟咖啡厅「星语轩」→ SaaS 化 AI 助手平台
    目标：综合运用，做出完整应用
```

**代码演进：**
```
89行 (Step 0) → 1800+行 (Step 19)
单文件 → 模块化 → 配置化 → 容器化 → 智能化 → SaaS化
```

## 📄 许可证

MIT © NuClaw Authors

---

**设计理念**：不是堆砌功能，而是解决问题。每章都有明确的「前一章问题 → 本章解决 → 暴露新问题」循环。
