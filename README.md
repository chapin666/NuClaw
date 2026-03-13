# NuClaw V2

> 用 C++17 从零构建生产级 AI Agent —— 渐进式教程（重构版）

[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://isocpp.org/std/the-standard)
[![CMake](https://img.shields.io/badge/CMake-3.14+-green.svg)](https://cmake.org/)
[![License](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

## 🎯 项目定位

NuClaw 是一个**教学性质的 C++ AI Agent 项目**，采用**渐进式代码演进**教学方法：

- 每章代码基于前一章演进（`git diff` 可见变化）
- 从 50 行单文件开始，逐步构建完整 Agent
- 每章解决前一章的问题，形成自然的学习曲线

**分支说明：**
- `master`: 旧版（大纲跳跃，不推荐）
- `v2-evolutionary`: 新版（渐进式演进，推荐）⭐

---

## 🚀 快速开始

```bash
# 克隆项目
git clone https://github.com/chapin666/NuClaw.git
cd NuClaw

# 切换到 V2 分支
git checkout v2-evolutionary

# 编译运行 Step 0
cd src/step00
g++ -std=c++17 main.cpp -o server -lboost_system -lpthread
./server

# 测试
curl http://localhost:8080/hello
# → 响应 "Echo: hello"
```

---

## 📚 新大纲（渐进式演进）

### 演进路线

```
Step 0      Step 3      Step 6      Step 9       Step 12
(89行)      (350行)     (550行)     (800行)      (1100行)
   │           │           │           │            │
   v           v           v           v            v
基础网络  →  规则AI   →  工具系统  →  企业功能  →  生产部署
(WebSocket)   (HTTP)     (模块化)    (多租户)     (Docker)
```

### 核心原则：问题解决驱动

| 章节 | 前一章问题 | 本章解决 | 代码演进 |
|:---|:---|:---|:---|
| **Step 3** | - | 起点：规则 AI | 单文件 |
| **Step 4** | HTTP 上下文丢失 | WebSocket 长连接 | 单文件 |
| **Step 5** | 规则匹配死板 | LLM 语义理解 | 单文件 |
| **Step 6** | LLM 无实时数据 | 工具调用 | **模块化** |
| **Step 7** | 同步阻塞 | 异步执行 | 模块化 |
| **Step 8** | 无安全控制 | 安全沙箱 | 模块化 |

---

## 📖 完整教程

### 第一阶段：网络基础（3章）- 单文件

| 章节 | 主题 | 代码 | 演进说明 |
|:---:|:---|:---:|:---|
| **Step 0** | Echo 服务器 | 89行 | 基础：同步 I/O |
| **Step 1** | 异步 I/O | 184行 | +Session 模式 |
| **Step 2** | HTTP 协议 | 271行 | +Router 路由 |

### 第二阶段：Agent 核心（3章）- 单文件 ⭐

| 章节 | 主题 | 代码 | 演进说明 |
|:---:|:---|:---:|:---|
| **Step 3** | 规则 AI | 350行 | **Agent Loop 起点**，暴露 HTTP 问题 |
| **Step 4** | WebSocket | 400行 | 解决 HTTP 上下文丢失 |
| **Step 5** | LLM 接入 | 450行 | 解决规则死板问题 |

### 第三阶段：Tools 系统（3章）- 模块化 ⭐⭐

| 章节 | 主题 | 代码 | 演进说明 |
|:---:|:---|:---:|:---|
| **Step 6** | 工具调用 | 550行 | **Agent Loop 完整**，硬编码问题 |
| **Step 7** | 异步工具 | 600行 | 8文件相同+2文件演进 |
| **Step 8** | 安全沙箱 | 700行 | 10文件相同+1演进+4新增 |

**模块化说明：**
- Step 6 开始模块化（工具数量增加，必须拆分）
- 文件结构保持一致，方便读者熟悉
- `git diff step06 step07` 清晰展示演进

### 第四阶段及以后（计划中）

| 阶段 | 内容 |
|:---|:---|
| Step 9-10 | Tool 注册表架构、Background Agent |
| Step 11-12 | 多租户系统 |
| Step 13-14 | 生产部署 |

---

## 🔍 代码演进验证

### Step 6 → 7 演进示例

```bash
# 查看文件变化
git diff step06 step07 --stat

# 输出：
# calc_tool.hpp       |  0  (相同)
# chat_engine.hpp     | 50 +++ (演进：+process_async)
# tool_executor.hpp   | 80 +++ (演进：+execute_async)
# ... 其他 8 个文件相同
```

### 演进统计

| 步骤 | 相同文件 | 演进文件 | 新增文件 |
|:---|:---:|:---:|:---:|
| 6 → 7 | 8 | 2 | 0 |
| 7 → 8 | 10 | 1 | 4 |

---

## 📁 代码结构规范

### Step 0-5: 单文件模式
```
src/step0X/
├── CMakeLists.txt
└── main.cpp    (< 500行)
```

### Step 6-8: 模块化模式
```
src/step0X/
├── CMakeLists.txt
├── main.cpp              (入口，< 100行)
├── chat_engine.hpp       (Agent 核心)
├── llm_client.hpp        (LLM 调用)
├── tool_executor.hpp     (工具执行)
├── tool.hpp              (工具基类)
├── *_tool.hpp            (具体工具)
└── server.hpp            (WebSocket)
```

---

## 🛠️ 编译运行

### 单文件章节 (Step 0-5)
```bash
cd src/step03
g++ -std=c++17 main.cpp -o server -lboost_system -lpthread
./server
```

### 模块化章节 (Step 6-8)
```bash
cd src/step06
mkdir build && cd build
cmake .. && make
./nuclaw_step06
```

---

## 📊 项目状态

| 阶段 | 状态 | 代码 |
|:---|:---:|:---:|
| 网络基础 (0-2) | ✅ 完成 | 89-271行 |
| Agent 核心 (3-5) | ✅ 完成 | 350-450行 |
| Tools 系统 (6-8) | ✅ 完成 | 550-700行 |
| 企业功能 (9-12) | ⏳ 计划中 | - |
| 生产部署 (13-14) | ⏳ 计划中 | - |

**当前分支**: `v2-evolutionary`  
**旧分支已清理**: feature/step-00 到 step-08 已删除

---

## 🤝 贡献

欢迎 Issue 和 PR！请基于 `v2-evolutionary` 分支开发。

### 贡献原则
1. **渐进式**: 新代码必须基于前一章演进
2. **单文件优先**: Step 0-5 保持单文件
3. **模块化规范**: Step 6+ 遵循模块化结构

---

## 📄 许可证

[MIT](LICENSE) © NuClaw Authors

---

**Why V2?**  
V1 大纲存在章节跳跃、概念不连贯的问题。V2 采用「问题解决驱动」的渐进式设计，每章都自然引出下一章，学习曲线更平滑。
