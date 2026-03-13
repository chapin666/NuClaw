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

### 演进路线

```
Step 0 (89行)      Step 3 (350行)      Step 6 (550行)
   │                    │                    │
   v                    v                    v
基础网络 ──────→ 规则 AI ──────→ 工具系统 ──────→ ...
 同步 I/O          HTTP问题           硬编码问题
                WebSocket解决      注册表解决(Step 9)
```

### 章节说明

| Step | 主题 | 代码 | 演进 |
|:---:|:---|:---:|:---|
| **0** | Echo 服务器 | 89行 | 起点 |
| **1** | 异步 I/O | 184行 | +Session |
| **2** | HTTP 协议 | 271行 | +Router |
| **3** | 规则 AI | 350行 | **Agent Loop 起点** |
| **4** | WebSocket | 400行 | 解决 HTTP 上下文丢失 |
| **5** | LLM 接入 | 450行 | 解决规则死板 |
| **6** | 工具调用 | 550行 | **模块化开始** |
| **7** | 异步工具 | 600行 | +并发控制 |
| **8** | 安全沙箱 | 700行 | +SSRF/路径防护 |

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

## 🔍 演进示例

**Step 6 → 7 的变化：**
```bash
git diff step06 step07 --stat
# 8 文件相同 | 2 文件演进 | 0 新增
# 
# tool_executor.hpp  +80行  (添加异步执行)
# chat_engine.hpp    +50行  (添加 process_async)
```

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

## 📊 状态

| 阶段 | 进度 | 代码行数 |
|:---|:---:|:---:|
| 网络基础 | ✅ | 89-271 |
| Agent 核心 | ✅ | 350-450 |
| Tools 系统 | ✅ | 550-700 |
| 企业功能 | ⏳ 计划中 | - |

## 📄 许可证

MIT © NuClaw Authors

---

**设计理念**：不是堆砌功能，而是解决问题。每章都有明确的「前一章问题 → 本章解决 → 暴露新问题」循环。
