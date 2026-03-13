# Step 3: Agent Loop - 状态机与对话管理

> 目标：构建 AI Agent 的核心循环，管理对话状态和历史
> 
> 难度：⭐⭐⭐ (中等)
> 
003e 代码量：约 250 行

## 本节收获

- 理解什么是 AI Agent 及其核心概念
- 掌握 Agent Loop 的工作机制
- 学会状态机设计模式
- 使用 WebSocket 实现实时通信
- 实现对话历史管理

---

## 第一部分：Agent 基础概念

### 什么是 AI Agent？

**传统程序：**
```
输入 → [固定逻辑] → 输出
         ↓
    程序员写死的规则
```

**AI Agent：**
```
输入 → [LLM 推理] → 决策 → [可能调用工具] → 输出
         ↓              ↓
    理解意图      选择行动方案
```

**Agent = LLM + 工具 + 记忆 + 规划能力**

简单理解：
- **Chatbot**：只会对话，一问一答
- **Agent**：能思考、能行动、能使用工具完成复杂任务

### Agent 的核心组件

```
┌─────────────────────────────────────────────────────────────┐
│                      AI Agent 架构                           │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│   ┌─────────────┐                                           │
│   │    用户     │                                           │
│   └──────┬──────┘                                           │
│          │ ① 输入问题                                        │
│          ▼                                                  │
│   ┌─────────────────────────────────────────────────────┐   │
│   │                    Agent Core                        │   │
│   │  ┌─────────────────────────────────────────────┐    │   │
│   │  │  LLM (大脑)                                  │    │   │
│   │  │  • 理解用户意图                               │    │   │
│   │  │  • 决定调用哪些工具                            │    │   │
│   │  │  • 生成回复内容                               │    │   │
│   │  └─────────────────────────────────────────────┘    │   │
│   │                         │                           │   │
│   │  ┌──────────────────────┼──────────────────────┐    │   │
│   │  │                      │                      │    │   │
│   │  ▼                      ▼                      ▼    │   │
│   │ ┌────────┐        ┌────────┐          ┌─────────┐   │   │
│   │ │ Tools  │        │ Memory │          │Planning │   │   │
│   │ │(工具)  │        │(记忆)  │          │(规划)   │   │   │
│   │ │        │        │        │          │         │   │   │
│   │ │• 搜索  │        │• 短期  │          │• 分解   │   │   │
│   │ │• 计算  │        │• 长期  │          │  任务   │   │   │
│   │ │• API  │        │        │          │• 反思   │   │   │
│   │ └────────┘        └────────┘          └─────────┘   │   │
│   └─────────────────────────────────────────────────────┘   │
│                          │                                  │
│                          ▼                                  │
│                   ② 返回结果                                │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### Agent vs Chatbot

| 特性 | Chatbot | Agent |
|:---|:---|:---|
| 交互模式 | 一问一答 | 多轮迭代 |
| 工具使用 | ❌ 无 | ✅ 有 |
| 任务完成 | 单次回答 | 多步骤执行 |
| 记忆能力 | 对话窗口内 | 长期记忆 |
| 示例 | 客服机器人 | AutoGPT、Devin |

### ReAct 模式（推理 + 行动）

Agent 的经典工作模式：

```
Thought（思考）→ Action（行动）→ Observation（观察）→ ... → Answer（回答）
```

**示例：**

```
用户：北京今天天气如何？

Agent:
[Thought] 用户询问北京天气，我需要调用天气工具
[Action] 调用 weather.get(city="北京")
[Observation] 得到结果：晴天，25°C
[Thought] 我已经获取到天气信息，可以直接回答
[Answer] 北京今天是晴天，气温25°C
```

**为什么这种模式有效？**
- 把复杂问题分解为步骤
- 每个步骤都有明确的输入输出
- LLM 可以自我纠正（如果工具返回错误）

---

## 第二部分：Agent Loop 详解

### 什么是 Agent Loop？

Agent Loop 是 Agent 的**主循环**，负责协调各个组件：

```
        ┌─────────────────────────────────────┐
        │           Agent Loop                │
        │                                     │
   ┌────┴────┐    ┌─────────┐    ┌────────┐ │
   │  接收   │───▶│  思考   │───▶│  行动  │ │
   │  输入   │    │(Reason) │    │(Act)   │ │
   └────┬────┘    └────┬────┘    └───┬────┘ │
        │              │             │      │
        │         ┌────┴────┐        │      │
        │         ▼         ▼        │      │
        │      ┌────────┐ ┌────────┐│      │
        │      │需要工具 │ │直接回答││      │
        │      └───┬────┘ └───┬────┘│      │
        │          │          │      │      │
        │          ▼          ▼      │      │
        │   ┌─────────────┐ ┌───────┴─┐    │
        │   │  调用工具   │ │生成回复 │    │
        │   │             │ │         │    │
        │   │ • 搜索      │ │         │    │
        │   │ • 计算      │ │         │    │
        │   │ • 查数据库  │ │         │    │
        │   └──────┬──────┘ └─────────┘    │
        │          │                       │
        └──────────┴───────────────────────┘
                     ↑_______________________|
                         循环直到完成
```

### 状态机设计

为什么 Agent 需要状态机？

```
问题：Agent 正在调用工具时，用户又发了新消息怎么办？

无状态机：
用户1: "查天气" → Agent: 调用 weather API...
用户2: "等下，先查股票" → Agent 混乱，可能同时处理两个请求

有状态机：
用户1: "查天气" → Agent: 进入 TOOL_CALLING 状态
                          ↓
                    拒绝新请求或排队
                          ↓
用户2: "查股票" → Agent: "请稍等，正在处理上一个请求"
```

**状态定义：**

```cpp
enum class State {
    IDLE,           // 空闲：可以接收新请求
    THINKING,       // 思考中：LLM 正在推理
    TOOL_CALLING,   // 调用工具：执行外部操作
    RESPONDING      // 响应中：生成回复
};
```

**状态转换规则：**

```
IDLE → THINKING      (收到用户输入)
THINKING → TOOL_CALLING  (需要调用工具)
THINKING → RESPONDING    (直接回答)
TOOL_CALLING → THINKING  (工具返回，继续思考)
TOOL_CALLING → RESPONDING (工具完成，生成回复)
RESPONDING → IDLE        (回复完成，回到空闲)
```

---

## 第三部分：工具（Tools）系统

### 什么是工具？

工具是 Agent 的**外挂能力**：

```
Agent 本身能做的：          Agent + 工具能做的：
• 文本生成                   • 搜索互联网
• 逻辑推理                   • 计算数学公式
• 代码生成                   • 查询数据库
• 知识问答                   • 发送邮件
                             • 操作文件系统
                             • 调用第三方 API
```

### 工具的数据结构

```cpp
// 工具调用请求
struct ToolCall {
    std::string id;           // 唯一标识
    std::string name;         // 工具名："calculator"
    json::value arguments;    // 参数：{"expr": "1+1"}
};

// 工具执行结果
struct ToolResult {
    std::string id;           // 对应请求的 id
    std::string output;       // 输出："2"
    bool success;             // 是否成功
};
```

### 工具调用流程

```
1. LLM 生成工具调用请求
   ↓
   {"name": "weather.get", "arguments": {"city": "北京"}}

2. Agent 解析并执行工具
   ↓
   调用 weather API

3. 工具返回结果
   ↓
   {"temperature": 25, "condition": "sunny"}

4. Agent 将结果给 LLM
   ↓
   LLM 生成最终回答："北京今天25°C，晴天"
```

---

## 第四部分：记忆系统

### 为什么需要记忆？

**短期记忆（对话历史）：**
```
用户：我叫张三
Agent：你好张三

用户：我今天生日
Agent：生日快乐！（知道"我"是张三）

用户：给我推荐个礼物
Agent：推荐送给张三的礼物（记得名字和生日上下文）
```

**长期记忆（跨会话）：**
```
昨天：
用户：我喜欢 Python
Agent：好的，我记下了

今天：
用户：推荐一门编程语言
Agent：推荐 Python（因为记得用户喜欢）
```

### 记忆的类型

| 类型 | 存储内容 | 生命周期 | 实现方式 |
|:---|:---|:---|:---|
| 短期记忆 | 当前对话 | 会话期间 | `vector<Message>` |
| 长期记忆 | 用户偏好、事实 | 永久 | 数据库/向量存储 |
| 上下文窗口 | 最近 N 条 | 受 LLM 限制 | 滑动窗口 |

---

## 第五部分：代码实现

### 整体架构

```cpp
class AgentLoop {
public:
    // 处理用户输入
    std::string process(const std::string& input, 
                       const std::string& session_id);
    
    // 获取会话信息
    json::value get_session_info(const std::string& session_id);

private:
    std::map<std::string, Session> sessions_;  // 所有会话
};
```

### 状态机实现

```cpp
std::string process(const std::string& input, 
                   const std::string& session_id) {
    auto& session = sessions_[session_id];
    
    // 1. 进入思考状态
    session.state = State::THINKING;
    
    // 2. 保存用户消息到历史
    session.add_message("user", input);
    
    // 3. 生成回复（这里简化处理）
    std::string response = generate_response(input, session);
    
    // 4. 保存助手回复到历史
    session.add_message("assistant", response);
    
    // 5. 回到空闲状态
    session.state = State::IDLE;
    
    return response;
}
```

### WebSocket 通信

为什么用 WebSocket？

```
HTTP 轮询：              WebSocket：
客户端                    客户端
   │ 有消息吗？              │ 建立连接
   ├──▶  │                  ═══════════
   │◀──  │ 没有              │ 永久双向通道
   │ 有消息吗？              │ 
   ├──▶  │                  ▼ 服务端主动推送
   │◀──  │ 有了！            
   
低效：频繁请求            高效：一次连接，实时通信
```

---

## 第六部分：完整运行测试

### 1. 编译运行

```bash
cd src/step03
mkdir build && cd build
cmake .. && make
./nuclaw_step03
```

### 2. WebSocket 测试

使用浏览器控制台：

```javascript
// 连接
const ws = new WebSocket('ws://localhost:8081');

// 接收消息
ws.onmessage = e => console.log('Received:', e.data);

// 发送消息
ws.send('Hello, Agent!');
```

### 3. 使用 wscat

```bash
npm install -g wscat
wscat -c ws://localhost:8081

> Hello
< Echo: Hello [state: idle, history: 2 msgs]
```

---

## 下一步

→ **Step 4: Agent Loop - 执行引擎**

我们将实现：
- 真正的工具调用
- 循环检测（防止无限调用）
- 并行工具执行

---

## 参考资源

- [ReAct 论文](https://arxiv.org/abs/2210.03629)
- [LangChain 文档](https://langchain.com/docs)
- [OpenAI Function Calling](https://platform.openai.com/docs/guides/function-calling)
