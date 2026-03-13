# NuClaw 重构执行计划

## 目标
- 按 OUTLINE_V2.md 新大纲重写教程
- 代码演进式：每章基于前一章，git diff 可见变化
- 每章衔接自然，知识点不跳跃

## 阶段一：基础重构（Step 0-3）

### Step 0: Echo 服务器（保持）
- 现状：已是最简形式
- 检查：确保 50 行左右，单文件
- 成果：同步 TCP Echo

### Step 1: 异步 I/O（重写）
- 基于 Step 0 演进
- 变化：
  - Server 类（保持 io_context/acceptor）
  - 新增 Session 类（处理单个连接）
  - 新增多线程 io.run()
- git diff：+70 行左右
- 教程：解释为什么要异步（Step 0 的阻塞问题）

### Step 2: HTTP 协议（重写）
- 基于 Step 1 演进
- 变化：
  - Session 增加 HTTP 解析
  - 新增 HttpRequest 结构体
  - 新增 Router 类
- git diff：+60 行左右
- 教程：HTTP 是应用层协议，需要解析

### Step 3: WebSocket（重写）
- 基于 Step 2 演进
- 变化：
  - Server 支持 Upgrade 检测
  - Session 可选择 HTTP 或 WebSocket 模式
  - WebSocket 帧解析
- git diff：+70 行左右
- 教程：HTTP 不能推送，需要 WebSocket

**检查点 1**：Step 0→1→2→3 代码演进是否自然？

---

## 阶段二：AI 核心（Step 4-6）

### Step 4: 规则 AI（全新）
- 基于 Step 3 演进
- 变化：
  - 新增 ChatSession 类（继承/组合 Session）
  - 简单的关键词匹配回复
  - 会话上下文记忆
- git diff：+70 行左右
- 教程：先让程序看起来"智能"（规则）

### Step 5: LLM 接入（重写）
- 基于 Step 4 演进
- 变化：
  - 新增 LLMClient 类
  - 替换规则匹配为 HTTP POST 到 OpenAI
  - 保持会话管理
- git diff：+80 行左右
- 教程：规则维护困难，引入 LLM

### Step 6: 工具调用（重写）
- 基于 Step 5 演进
- 变化：
  - 新增 Tool 概念（硬编码实现）
  - LLM 决定调用工具
  - 执行工具后返回结果给 LLM
- git diff：+100 行左右
- 教程：LLM 需要实时数据，引入工具

**检查点 2**：Step 4→5→6 是否让读者理解"为什么需要 LLM"和"为什么需要工具"？

---

## 阶段三：工具系统（Step 7-9）

### Step 7: Tool 系统架构（重写）
- 基于 Step 6 演进
- 变化：
  - 硬编码工具 → Tool 基类 + 注册表
  - 新增 ToolSchema
  - 参数校验
- git diff：+100 行（开始模块化）
- 教程：硬编码难以扩展，需要系统架构

### Step 8: 异步工具（重写）
- 基于 Step 7 演进
- 变化：
  - 新增异步执行接口
  - std::async/future 使用
  - 超时控制
- git diff：+100 行
- 教程：同步阻塞，需要异步

### Step 9: 安全沙箱（重写）
- 基于 Step 8 演进
- 变化：
  - 新增 Sandbox 类
  - HTTP 工具 SSRF 保护
  - 代码执行黑名单
- git diff：+200 行
- 教程：工具危险，需要安全控制

**检查点 3**：工具系统三章是否形成完整知识体系？

---

## 阶段四：企业功能（Step 10-12）

### Step 10: 持久化（上）
- 基于 Step 9 演进
- 变化：
  - 新增 Database 类
  - 对话历史存 SQLite
- git diff：+100 行
- 教程：内存数据重启丢失，需要持久化

### Step 11: 多租户（下）
- 基于 Step 10 演进
- 变化：
  - 新增 Tenant 概念
  - 用户隔离
  - 配额管理
- git diff：+100 行
- 教程：多人使用需要隔离

### Step 12: Channel 集成
- 基于 Step 11 演进
- 变化：
  - 新增 Channel 抽象
  - 飞书适配器实现
- git diff：+200 行
- 教程：不仅 WebSocket，还要支持 IM

---

## 阶段五：生产部署（Step 13-14）

### Step 13: 安全认证
- JWT、HTTPS、输入验证
- git diff：+100 行

### Step 14: 容器化
- Dockerfile、监控、性能调优
- git diff：配置文件为主

---

## 文件组织

```
src/
├── step00/              # 基础
│   └── main.cpp
├── step01/              # 基于 step00
│   ├── main.cpp
│   └── session.hpp      # 新增
├── step02/              # 基于 step01
│   ├── main.cpp
│   ├── session.hpp      # 修改
│   ├── router.hpp       # 新增
│   └── http_parser.hpp  # 新增
├── step03/              # 基于 step02
│   ├── main.cpp
│   ├── session.hpp      # 修改（支持 WebSocket）
│   ├── router.hpp       # 保持
│   └── websocket.hpp    # 新增
└── ...
```

---

## 关键原则

1. **每章 git diff 可见**：
   ```bash
   git diff step02 step03 --stat
   # 应该显示修改了哪些文件，新增了多少行
   ```

2. **主要类保持连续性**：
   - Server 类：Step 1→2→3 持续演进
   - Session 类：Step 1→2→3 从 HTTP 升级到 WebSocket
   - 避免突然消失或完全重写

3. **增量变化**：
   - 每章新增 50-100 行代码
   - 不是 500 行重写

4. **文档跟随代码**：
   - 教程中的代码片段 = 实际代码的 diff
   - "我们在上一章基础上，增加..."

---

## 执行顺序

由于改动量大，建议按以下顺序：

1. **先完成 Step 0-3**（基础框架）
   - 验证演进式代码是否可行
   - 确定代码风格

2. **再完成 Step 4-6**（AI 核心）
   - 这是新大纲的重点
   - 规则 AI → LLM → 工具 的链条必须清晰

3. **最后 Step 7-14**（高级功能）
   - 基于前面的基础扩展

---

## 验证标准

每章完成后检查：

- [ ] 代码能编译运行
- [ ] git diff 前一章 < 200 行
- [ ] 主要类名保持一致
- [ ] 教程开头解释"本章解决什么问题"
- [ ] 教程结尾预告"下一章解决什么问题"
