# NuClaw

<div class="nuclaw-hero" markdown="1">

# **NuClaw**

<p class="tagline">用 C++17 从零构建 AI Agent —— 渐进式教程</p>

<p>
  <a href="step00/tutorial/" class="cta-button">🚀 开始你的第一个 Step</a>
</p>

</div>

## 💡 这是什么？

**NuClaw** 是一个动手实践的 C++ AI Agent 教程项目。

!!! quote "不是纸上谈兵，而是一行一行敲出来的"

    从 89 行单文件到 1800+ 行生产级 SaaS 平台，**每章都有可运行的代码**。
    你将亲手构建一个完整的 AI Agent 框架，涵盖：
    
    - 网络编程、异步架构、LLM 集成
    - 工具系统、RAG 检索、多 Agent 协作
    - 状态记忆、情感计算、虚拟世界
    - 多租户 SaaS、生产级部署

## 🎯 你能获得什么？

<div class="highlight-box" markdown="1">

### 从零开始，构建完整的 AI Agent 系统

- ✅ **扎实的 C++ 网络编程能力** — Boost.Asio、异步 I/O、HTTP/WebSocket
- ✅ **深入理解 AI Agent 架构** — Agent Loop、工具调用、记忆系统、情感计算
- ✅ **LLM 应用开发实战经验** — API 集成、Prompt 工程、RAG 检索增强
- ✅ **复杂系统设计能力** — 多 Agent 协作、虚拟世界、关系网络
- ✅ **生产级 SaaS 架构** — 多租户、认证授权、计费系统、运维监控

</div>

## 📈 学习路径

<div class="progress-showcase" markdown="1">

### 代码演进之路

<div class="progress-line">
  <span class="progress-step beginner">89行</span>
  <span class="progress-arrow">→</span>
  <span class="progress-step intermediate">271行</span>
  <span class="progress-arrow">→</span>
  <span class="progress-step advanced">450行</span>
  <span class="progress-arrow">→</span>
  <span class="progress-step expert">850行</span>
  <span class="progress-arrow">→</span>
  <span class="progress-step master">1200行</span>
  <span class="progress-arrow">→</span>
  <span class="progress-step production">1800+行</span>
</div>

</div>

### 四大阶段

<div class="learning-path" markdown="1">

<div class="path-card part-1" markdown="1">

### 📡 Part 1: 基础构建
**Step 0-5**

网络基础 → HTTP 协议 → Agent Loop → LLM 集成。

<span class="code-count">89行 → 450行</span>

</div>

<div class="path-card part-2" markdown="1">

### 🛠️ Part 2: 能力扩展
**Step 6-12**

工具系统 → Skill 封装 → RAG 检索 → 多 Agent 协作 → MCP 生态。

<span class="code-count">550行 → 900行</span>

</div>

<div class="path-card part-3" markdown="1">

### 🚀 Part 3: 产品化
**Step 13-17**

配置热加载、监控告警、Docker/K8s 部署、IM 接入、记忆系统。

<span class="code-count">950行 → 1300行</span>

</div>

<div class="path-card part-4" markdown="1">

### 🏢 Part 4: 实战项目
**Step 18-21**

智能客服 SaaS 平台：架构设计 → 核心功能 → 高级功能 → 生产部署。

<span class="code-count">1500行 → 2000+行</span>

</div>

</div>

## 🎨 项目实战展示

<div class="project-showcase" markdown="1">

<div class="project-card" markdown="1">

### 🏢 SmartSupport —— 智能客服 SaaS 平台

**生产级多租户智能客服平台**，综合运用 Step 0-17 全部技术栈

- 🤖 AI 自动回复 + RAG 知识检索
- 👨‍💼 人机协作智能分配
- 🏢 多租户数据隔离
- 💰 计费与套餐管理
- 🚀 K8s 微服务部署

[Step 18: 架构设计 →](step18/tutorial/)  
[Step 19: 核心功能 →](step19/tutorial/)  
[Step 20: 高级功能 →](step20/tutorial/)  
[Step 21: 生产部署 →](step21/tutorial/)

</div>

</div>

## ✨ 为什么选择 NuClaw？

| 特点 | 说明 |
|:---|:---|
| **🔄 代码演进** | 每章基于前一章，`git diff` 可见变化，学习过程透明可见 |
| **🎯 问题驱动** | 每章解决前一章的实际问题，不是功能堆砌 |
| **📚 循序渐进** | 从单文件到模块化到配置化到 SaaS 化，符合真实项目演进 |
| **🔧 即学即用** | 每章都有完整可运行的代码，边学边练 |
| **🎨 图解丰富** | 架构图、流程图、时序图，复杂概念一目了然 |
| **🎓 项目实战** | 3 个完整项目，从应用到虚拟世界到 SaaS 平台 |

## 🚀 快速开始

```bash
# 克隆项目
git clone https://github.com/chapin666/NuClaw.git
cd NuClaw

# Step 0: 最简单的 Echo 服务器
cd src/step00
g++ -std=c++17 main.cpp -o server -lboost_system -lpthread
./server

# 测试
curl http://localhost:8080/hello
```

## 👥 适合谁学？

- 💻 想系统学习 C++ 网络编程的开发者
- 🤖 对 AI Agent 架构感兴趣的工程师  
- 🧠 希望深入理解 LLM 应用底层实现的程序员
- 🏭 准备将 Agent 系统部署到生产环境的技术负责人
- 🚀 想构建 AI SaaS 产品的创业者

## 🎓 设计理念

!!! quote "不是堆砌功能，而是解决问题"

    每章都有明确的「前一章问题 → 本章解决 → 暴露新问题」循环。
    
    代码演进路径：
    ```
    单文件 → 模块化 → 配置化 → 容器化 → SaaS化
    ```
    
    技术深度路径：
    ```
    网络编程 → Agent 核心 → 工具系统 → 记忆情感 → 多 Agent → 生产架构
    ```

---

<div style="text-align:center; margin: 2rem 0;" markdown="1">

## 准备好开始了吗？ 👇

<a href="step00/tutorial/" class="md-button md-button--primary">🚀 从 Step 0 开始</a>

<a href="step18/tutorial/" class="md-button">🧳 或先看实战项目</a>

</div>

---

## 📄 许可证

MIT © NuClaw Authors
