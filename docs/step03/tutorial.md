# Step 3: 规则 AI - 第一个"智能"程序

> 目标：让程序看起来"智能"，能理解和回应用户
> 
> 难度：⭐⭐ (进阶)
> 
> 代码量：约 350 行

## 本节收获

- 理解什么是 AI Agent 的核心循环
- 掌握规则匹配的设计方法
- 体验 HTTP 无状态的问题（为下一章铺垫）
- 建立"感知 → 理解 → 响应"的思维模型

---

## 第一步：回顾 Step 2 的问题

Step 2 我们实现了一个 HTTP 路由器：

```
用户: GET /hello
服务器: {"message": "Hello"}
```

但这只是**固定响应**，程序没有"思考"过程。

**我们想要的是：**
```
用户: 你好
服务器: 理解语义 → 生成回复 → "你好！很高兴见到你"
```

这就是 **AI Agent 的核心循环**（Agent Loop）：
```
输入 → 理解 → 决策/行动 → 输出
```

---

## 第二步：Agent Loop 基础

### 什么是最简单的 Agent？

不是 ChatGPT，而是**带有规则匹配的回复系统**：

```cpp
// 最简单的 Agent
string process(string input) {
    if (input contains "你好") return "你好！";
    if (input contains "时间") return get_current_time();
    return "我不理解";
}
```

虽然简单，但已经具备 Agent 的基本结构：
1. **感知**：接收用户输入
2. **理解**：规则匹配（关键词）
3. **响应**：返回结果

### 本章代码演进

从 Step 2 演进（单文件，新增约 80 行）：

```diff
+ // 新增：ChatEngine 类
+ class ChatEngine {
+     string process(string input, Context& ctx) {
+         // 规则匹配
+         if (regex_match(input, "你好")) return "你好！";
+         if (regex_match(input, "时间")) return get_time();
+         ...
+     }
+ };
```

---

## 第三步：规则系统设计

### 为什么用规则？

**优势：**
- 确定性：输入确定，输出确定
- 可解释：知道为什么这样回复
- 易于调试

**劣势：**
- 需要预设所有模式
- 无法理解意外输入
- 维护成本高

本章我们先体验规则系统，**Step 5 会用 LLM 解决这些问题**。

### 规则引擎实现

```cpp
struct Rule {
    regex pattern;        // 匹配模式
    string intent;        // 意图标识
    function<string()> response;  // 回复生成
};

vector<Rule> rules = {
    {regex("你好"), "greeting", []() { return "你好！"; }},
    {regex("时间"), "time_query", []() { return get_time(); }},
    {regex("天气"), "weather_query", [input]() { return get_weather(input); }}
};
```

### 上下文理解（初步）

多轮对话需要**记忆**：

```
用户：北京天气如何？
AI：北京今天晴天。

用户：那上海呢？  ← AI 需要知道"那"指代天气
```

```cpp
struct Context {
    string last_intent;   // 上轮意图：weather_query
    string last_topic;    // 上轮话题：北京
};
```

---

## 第四步：HTTP 的问题暴露

### 问题发现

运行代码测试：

```bash
# 请求 1
$ curl -X POST -d "北京天气" http://localhost:8080/chat
{"reply": "北京今天晴天"}

# 请求 2（紧接着）
$ curl -X POST -d "那上海呢" http://localhost:8080/chat  
{"reply": "我不理解"}  ← 失败！
```

**为什么失败？**

```cpp
router.add_route("/chat", [](const HttpRequest& req) {
    ChatContext ctx;  // ← 每次请求都新建 ctx！
    // 上轮对话的记忆丢失了
});
```

HTTP 是**无状态协议**：
- 每次请求独立
- 服务器不保存客户端状态
- 上次对话的 `Context` 已经销毁

### 这就是 Agent Loop 的第一个挑战

Agent 需要**持续的状态**来维护对话，但 HTTP 无法提供。

**下一章解决方案：WebSocket 长连接**

---

## 第五步：运行测试

### 编译运行

```bash
g++ -std=c++17 main.cpp -o server -lboost_system -lpthread
./server
```

### 功能测试

```bash
# 1. 问候
curl -X POST -d "你好" http://localhost:8080/chat
# → {"reply": "你好！我是 NuClaw AI..."}

# 2. 时间查询
curl -X POST -d "现在几点" http://localhost:8080/chat
# → {"reply": "2024-01-15 14:30:25"}

# 3. 天气查询
curl -X POST -d "北京天气" http://localhost:8080/chat
# → {"reply": "北京今天晴天，25°C"}

# 4. 上下文测试（会失败，证明 HTTP 问题）
curl -X POST -d "北京天气" http://localhost:8080/chat
curl -X POST -d "那上海呢" http://localhost:8080/chat
# → {"reply": "我不理解"}
```

---

## 本节总结

### 我们实现了什么？

1. **Agent Loop 基础版**：输入 → 规则匹配 → 输出
2. **规则引擎**：可配置的关键词 → 回复映射
3. **初步的上下文**：结构定义（但 HTTP 限制无法持久化）

### 代码演进

```
Step 2: HTTP Router
   ↓ +80 行
Step 3: Rule-based AI
   - ChatEngine 类
   - 规则匹配系统
   - Context 结构（准备用于多轮对话）
```

### 暴露的问题

**核心问题：HTTP 无状态**
- 每次请求独立，上下文丢失
- 无法维护长期对话
- Agent Loop 不完整（缺少持久状态）

### 下章预告

**Step 4: WebSocket 实时通信**

解决 HTTP 问题：
- 长连接保持（一次握手，持续通信）
- 上下文持久化（连接期间状态不丢失）
- 服务器主动推送

```
用户：北京天气如何？
AI：晴天
用户：那上海呢？
AI：理解上下文！上海今天多云。
```

---

## 附：核心代码结构

```cpp
// Agent Loop 的基础结构（本章）
class ChatEngine {
public:
    string process(string input, Context& ctx) {
        // 1. 规则匹配（理解）
        for (auto& rule : rules_) {
            if (regex_match(input, rule.pattern)) {
                // 2. 生成回复（响应）
                return rule.response();
            }
        }
        return "不理解";
    }
};
```

**这就是 Agent Loop 的雏形**，虽然简单，但包含了核心要素：
- 输入处理
- 理解/决策
- 输出生成

下一章我们会用 WebSocket 让它真正工作起来。
