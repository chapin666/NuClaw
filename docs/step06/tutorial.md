# Step 6: 工具调用 - 让 LLM 能"做事"

> 目标：解决 LLM 无实时数据的问题，实现完整的 Agent Loop
> 
> 难度：⭐⭐⭐ (中等)
> 
> 代码量：约 550 行

## 本节收获

- 理解 LLM 的局限性（知识截止、无实时数据）
- 实现工具调用机制（Function Calling）
- 掌握"硬编码工具"的问题（为下一章铺垫）
- **Agent Loop 完整！** 感知 → 理解 → 决策 → 行动 → 响应

---

## 第一步：回顾 Step 5 的核心问题

Step 5 接入了 LLM，能理解了，但有个致命缺陷：

```
用户：北京现在多少度？
LLM：抱歉，我没有实时天气数据。

用户：现在几点？
LLM：我无法获取当前时间。

用户：1+1等于几？
LLM：2（能回答，但这是基于训练数据，不是计算）
```

### LLM 的局限性

1. **知识截止**：训练数据有截止日期（GPT-4 是 2024 年初）
2. **无实时数据**：不知道当前时间、天气、股价
3. **无法执行操作**：不能查数据库、不能发邮件

**LLM 只有"大脑"，没有"手脚"。**

---

## 第二步：工具调用解决方案

### 核心思想

让 LLM **决定**调用什么工具，我们**执行**工具，把结果**返回**给 LLM 生成回复：

```
用户：北京天气如何？
    ↓
LLM 理解：需要天气数据 → 决定调用 get_weather("北京")
    ↓
系统执行：get_weather("北京") → 返回 {"temp": 25, "weather": "晴天"}
    ↓
LLM 生成回复："北京今天晴天，25°C。"
    ↓
用户：收到回复
```

### 这就是 ReAct 模式

**Re**ason（推理）+ **Act**（行动）：

```
思考 → 行动 → 观察 → 思考 → ... → 回复
```

也是 Agent Loop 的完整形态：

```
感知（输入）→ 理解（LLM）→ 决策（调用工具）→ 行动（执行）→ 响应（生成回复）
```

---

## 第三步：代码演进

从 Step 5 演进（新增约 100 行）：

```diff
// Step 5: LLM 直接回复
class ChatEngine {
    string process(string input) {
        return llm_.complete(input);  // ← 直接回答
    }
};

// Step 6: LLM + 工具
class ChatEngine {
    LLMClient llm_;
    ToolExecutor tools_;  // ← 新增
    
    string process(string input) {
        // 1. LLM 决定是否需要工具
        if (llm_.needs_tool(input)) {
            // 2. 解析工具调用
            auto call = llm_.parse_tool_call(input);
            // 3. 执行工具
            auto result = tools_.execute(call);
            // 4. LLM 根据结果生成回复
            return llm_.generate_response(result);
        }
        // 5. 不需要工具，直接回答
        return llm_.complete(input);
    }
};

+ // 新增：工具执行器
+ class ToolExecutor {
+     ToolResult execute(ToolCall call) {
+         if (call.name == "get_weather") 
+             return get_weather(call.args);
+         if (call.name == "get_time")
+             return get_current_time();
+         ...
+     }
+ };
```

### 硬编码工具实现

```cpp
// 工具1：天气查询（模拟）
ToolResult get_weather(const string& city) {
    if (city == "北京") 
        return {true, R"({"temp": 25, "weather": "晴天"})"};
    if (city == "上海") 
        return {true, R"({"temp": 22, "weather": "多云"})"};
    return {false, "", "不支持的城市"};
}

// 工具2：当前时间
ToolResult get_time() {
    auto now = chrono::system_clock::now();
    // ... 格式化时间
    return {true, json_result};
}

// 工具3：计算器
ToolResult calculate(const string& expr) {
    // 解析 "1 + 2" → 3
    return {true, result};
}
```

---

## 第四步：工具调用流程详解

### 完整流程图

```
用户输入："北京天气如何"
    ↓
┌─────────────────────────────────────┐
│  Step 1: LLM 理解意图                │
│  - 识别出"天气"意图                  │
│  - 提取实体"北京"                    │
└─────────────────────────────────────┘
    ↓
┌─────────────────────────────────────┐
│  Step 2: 决策（是否需要工具）        │
│  - 需要天气数据 → 调用工具           │
└─────────────────────────────────────┘
    ↓
┌─────────────────────────────────────┐
│  Step 3: 构造工具调用                │
│  - 工具名：get_weather               │
│  - 参数："北京"                      │
└─────────────────────────────────────┘
    ↓
┌─────────────────────────────────────┐
│  Step 4: 执行工具                    │
│  - 调用 get_weather("北京")          │
│  - 返回：{"temp": 25, ...}           │
└─────────────────────────────────────┘
    ↓
┌─────────────────────────────────────┐
│  Step 5: LLM 生成回复                │
│  - 输入工具结果                      │
│  - 生成自然语言："北京今天晴天..."   │
└─────────────────────────────────────┘
    ↓
用户收到："北京今天晴天，25°C。"
```

### 关键代码

```cpp
string ChatEngine::process(const string& user_input) {
    cout << "[🧠 Processing] \"" << user_input << "\"" << endl;
    
    // Step 1 & 2: 判断是否需要工具
    if (llm_.needs_tool(user_input)) {
        cout << "  → Needs tool" << endl;
        
        // Step 3: 解析工具调用
        auto tool_call = llm_.parse_tool_call(user_input);
        cout << "  → Tool: " << tool_call.name << endl;
        
        // Step 4: 执行工具
        auto result = tool_executor_.execute(tool_call);
        cout << "  → Result: " << result.data << endl;
        
        // Step 5: 生成回复
        return llm_.generate_response(user_input, result, tool_call);
    }
    
    // 不需要工具，直接回答
    return llm_.direct_reply(user_input);
}
```

---

## 第五步：暴露硬编码问题

### 当前实现的问题

```cpp
class ToolExecutor {
    ToolResult execute(const ToolCall& call) {
        // 硬编码分发！
        if (call.name == "get_weather") return get_weather(call.args);
        if (call.name == "get_time") return get_time();
        if (call.name == "calculate") return calculate(call.args);
        // ...
        // 每加一个工具都要改这里！
    }
};
```

**想添加"股票查询"工具？**
1. 写 stock_query 函数
2. 在 execute() 里加 if 分支
3. 修改 LLMClient 让 LLM 知道新工具
4. 重新编译

**这是"硬编码"的痛苦！**

### 下一章解决方案

**Step 7: Tool 系统架构**
- 工具基类 + 注册表模式
- 声明式工具定义（Schema）
- 自动发现新工具

---

## 第六步：运行测试

### 编译运行

```bash
g++ -std=c++17 main.cpp -o server -lboost_system -lpthread
./server
```

### WebSocket 测试

```bash
wscat -c ws://localhost:8080/ws

> 你好
< 你好！我是带工具调用能力的 AI。
< 我可以帮你：查天气、算数学、看时间。

> 北京天气如何？
[🧠 Processing] "北京天气如何"
  → Needs tool
  → Tool: get_weather
  → Result: {"temp": 25, "weather": "晴天"}
< 北京今天晴天，25°C。

> 现在几点？
[🧠 Processing] "现在几点"
  → Needs tool
  → Tool: get_time
< 当前时间是 2024-01-15 14:30:25

> 25 * 4 等于多少？
[🧠 Processing] "25 * 4 等于多少"
  → Needs tool
  → Tool: calculate
< 25 * 4 = 100
```

---

## 本节总结

### 我们实现了什么？

**完整的 Agent Loop：**

```
感知（用户输入）
    ↓
理解（LLM 语义分析）
    ↓
决策（判断需要天气数据）
    ↓
行动（调用 get_weather）
    ↓
观察（工具返回数据）
    ↓
响应（LLM 生成自然语言回复）
```

这就是 ReAct 模式，Agent 的核心工作流程。

### Agent Loop 演进完成

```
Step 3: 规则匹配（基础）
   ↓
Step 4: WebSocket（持久连接）
   ↓
Step 5: LLM（语义理解）
   ↓
Step 6: 工具调用（行动能力）
```

**现在你有一个完整的 AI Agent！**

### 代码演进

```
Step 5: WebSocket + LLM (450行)
   ↓ +100 行
Step 6: + 工具调用 (550行)
   - Tool 结构定义
   - ToolExecutor 类
   - 硬编码工具实现
   - LLM + 工具协作流程
```

### 仍存在的问题

**硬编码难以扩展：**
```cpp
// 每加一个工具都要改这里
if (name == "get_weather") ...
if (name == "stock_query") ...  // 新增
if (name == "send_email") ...   // 再新增
```

**下一章：Tool 系统架构**
- 插件化工具注册
- 声明式 Schema
- 自动发现和加载

---

## 附：Agent Loop 完整架构

```
┌─────────────────────────────────────────────┐
│              AI Agent 完整架构               │
├─────────────────────────────────────────────┤
│                                             │
│   感知层 (Input)                             │
│   └─ WebSocket 接收用户消息                  │
│                                             │
│   理解层 (LLM)                               │
│   └─ 语义分析、意图识别                      │
│                                             │
│   决策层 (Reasoning)                         │
│   └─ 判断是否需要工具                        │
│       ├─ 不需要 → 直接生成回复               │
│       └─ 需要   → 选择工具                   │
│                                             │
│   行动层 (Tools)                             │
│   └─ 执行 get_weather / get_time / calculate │
│                                             │
│   响应层 (Output)                            │
│   └─ LLM 根据工具结果生成自然语言            │
│                                             │
└─────────────────────────────────────────────┘
```

**这就是完整的 Agent Loop！** 🎉

下一步：把硬编码工具改成可扩展的系统架构。
