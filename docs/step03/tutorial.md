# Step 3: 规则 AI - 第一个"智能"程序

> 目标：理解 AI Agent 是什么，实现第一个能"对话"的程序
> 
> 难度：⭐⭐ (进阶)
> 
> 代码量：约 350 行
> 
> 预计学习时间：2-3 小时

---

## 📚 前置知识

### 什么是 AI Agent？

**Agent（智能体）** = 能够**感知环境**并**采取行动**的实体。

通俗理解：
- **普通程序**：你输入 A，它输出 B（固定的映射）
- **Agent**：你输入 A，它**理解**你的意图，然后**决定**做什么，最后**执行**

**例子对比：**

| 类型 | 你输入 | 程序处理 | 输出 |
|:---|:---|:---|:---|
| 普通程序 | `GET /time` | 查找路由表 | `{"time": "14:30"}` |
| Agent | "现在几点了？" | 理解→决策→执行 | "现在是下午 2 点 30 分" |

Agent 更像一个**有思考能力的助手**，而不是**死板的机器**。

### Agent Loop（核心概念）

Agent 的工作流程是一个**循环**：

```
        ┌─────────────────────────────────────────┐
        │              Agent Loop                 │
        │                                         │
   ┌────┴────┐    ┌─────────┐    ┌─────────┐   │
   │  感知   │───▶│  理解   │───▶│  行动   │   │
   │ (输入)  │    │ (思考)  │    │ (输出)  │   │
   └────┬────┘    └─────────┘    └─────────┘   │
        │                                       │
        └───────────────────────────────────────┘
```

**Step 3 我们要实现的是简化版：**

```
用户输入 → 规则匹配（理解）→ 生成回复（行动）
```

虽然简单，但这已经是一个**最小可用**的 Agent 了！

---

## 第一步：回顾 Step 2 的问题

### Step 2 的路由器有什么问题？

```cpp
// Step 2 的代码
router.add_route("/hello", [](const HttpRequest& req) {
    return "{\"message\": \"Hello\"}";
});
```

**问题：太死板！**

```
用户: GET /hello
服务器: {"message": "Hello"}

用户: GET /hi        ← 404 错误！
用户: 你好            ← 不支持！
```

路由器只能匹配**精确的路径**，无法理解**自然语言**。

### 我们想要的效果

```
用户: 你好
服务器: 你好！很高兴见到你。

用户: 现在几点？
服务器: 现在是 2024-01-15 14:30:25

用户: 北京天气如何？
服务器: 北京今天晴天，25°C。
```

这就是 Agent：能理解**自然语言**，而不仅仅是**匹配路径**。

---

## 第二步：设计规则系统

### 为什么从"规则"开始？

作为初学者，直接学习 LLM（ChatGPT）太复杂。我们先从**简单的规则匹配**入手：

**规则系统的优点：**
1. **简单易懂**：if-else 逻辑，清晰明了
2. **确定性强**：同样的输入，永远得到同样的输出
3. **易于调试**：哪里出错一眼就能看到

**规则系统的缺点：**
1. **不够智能**：只能匹配预设的模式
2. **维护困难**：规则多了会变得复杂
3. **无法泛化**：没见过的问题就答不上来

**学习路径：**
```
Step 3 (规则)  ──→  Step 5 (LLM)
   简单确定           智能灵活
   适合学习基础       适合生产环境
```

### 规则设计思路

我们要设计一个**天气助手** Agent：

```
用户说 "你好" → 回复问候语
用户说 "时间" → 回复当前时间
用户说 "北京天气" → 回复北京天气
用户说 "上海天气" → 回复上海天气
```

用代码表示：

```cpp
// 伪代码
if (用户输入包含 "你好") {
    return "你好！我是天气助手。";
} else if (用户输入包含 "时间") {
    return get_current_time();
} else if (用户输入包含 "北京" && 用户输入包含 "天气") {
    return "北京今天晴天，25°C。";
}
// ... 更多规则
```

### 规则的数据结构

为了代码更清晰，我们定义一个 `Rule` 结构：

```cpp
struct Rule {
    std::regex pattern;        // 正则表达式匹配模式
    std::string intent;        // 意图标识（用于后续处理）
    std::function<std::string()> response;  // 回复生成函数
};

// 使用示例
Rule hello_rule = {
    std::regex("你好|嗨|hello", std::regex::icase),  // 匹配"你好""嗨""hello"
    "greeting",                                        // 意图：问候
    []() { return "你好！很高兴见到你。"; }            // 回复
};
```

---

## 第三步：理解正则表达式

### 什么是正则表达式？

**正则表达式（Regex）** 是一种**模式匹配**语言，用于从文本中查找特定模式。

**简单示例：**

```cpp
// 匹配 "你好"
std::regex("你好")

// 匹配 "你好" 或 "您好" 或 "Hello"
std::regex("你好|您好|Hello")

// 匹配任意数字
std::regex("[0-9]+")

// 匹配以"天气"结尾的句子
std::regex(".*天气")
```

### 本章用到的正则

```cpp
// 1. 问候（忽略大小写）
std::regex("你好|嗨|hello|hi", std::regex::icase)
// 匹配：你好、嗨、Hello、hello、Hi、HI

// 2. 时间查询
std::regex("时间|几点|现在", std::regex::icase)
// 匹配：现在几点、时间是多少、现在

// 3. 天气查询（捕获城市名）
std::regex("(.*)天气|(.*)温度", std::regex::icase)
// 匹配：北京天气、上海温度、今天天气如何
// (.*) 表示捕获任意字符，用于提取城市名
```

### 测试正则表达式

你可以用在线工具测试正则：
- https://regex101.com/
- 选择 flavor: C++ (std::regex)

---

## 第四步：代码实现详解

### 整体架构

```
src/step03/
└── main.cpp          (350行，包含所有代码)

代码结构：
1. HTTP 请求解析（从 Step 2 复用）
2. ChatEngine 类（新增，Agent 核心）
3. Session/Server 类（从 Step 2 复用）
4. main 函数（入口）
```

### ChatEngine 类（核心）

```cpp
// ChatEngine - Agent 的大脑
class ChatEngine {
public:
    // 构造函数：初始化规则
    ChatEngine() {
        init_rules();  // 初始化所有规则
    }
    
    // 处理用户输入（核心函数）
    std::string process(const std::string& input, ChatContext& ctx) {
        // 1. 尝试匹配规则
        for (const auto& rule : rules_) {
            // 正则匹配
            if (std::regex_search(input, rule.pattern)) {
                // 匹配成功，记录上下文
                ctx.last_intent = rule.intent;
                ctx.last_time = std::chrono::steady_clock::now();
                
                // 执行回复函数
                return rule.response(input);
            }
        }
        
        // 2. 没有匹配到规则
        return "我不理解你的问题，可以试试：你好、时间、北京天气";
    }

private:
    std::vector<Rule> rules_;  // 规则列表
    
    // 初始化规则
    void init_rules() {
        // 规则 1：问候
        rules_.push_back({
            std::regex("你好|嗨|hello|hi", std::regex::icase),
            "greeting",
            [](const std::string&) {
                return "你好！我是 NuClaw AI，可以帮你查天气、时间。";
            }
        });
        
        // 规则 2：时间查询
        rules_.push_back({
            std::regex("时间|几点|现在", std::regex::icase),
            "time_query",
            [](const std::string&) {
                // 获取当前时间
                auto now = std::chrono::system_clock::now();
                std::time_t t = std::chrono::system_clock::to_time_t(now);
                char buf[100];
                std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", 
                             std::localtime(&t));
                return std::string(buf);
            }
        });
        
        // 规则 3：天气查询
        rules_.push_back({
            std::regex("(.*)天气|(.*)温度", std::regex::icase),
            "weather_query",
            [](const std::string& input) {
                // 简单提取城市名（实际应该用更智能的方法）
                if (input.find("北京") != std::string::npos) 
                    return "北京今天晴天，25°C，空气质量良好。";
                if (input.find("上海") != std::string::npos) 
                    return "上海今天多云，22°C，有微风。";
                return "请告诉我具体城市名（如：北京、上海）";
            }
        });
    }
};
```

### 上下文管理

```cpp
// ChatContext - 保存对话上下文
struct ChatContext {
    std::string last_intent;      // 上次的意图
    std::string last_topic;       // 上次的话题
    std::chrono::steady_clock::time_point last_time;  // 上次对话时间
};
```

**为什么要上下文？**

实现多轮对话：
```
用户：北京天气如何？
AI：北京今天晴天，25°C。

用户：那上海呢？        ← 这里"那"指代"天气"
AI：上海今天多云，22°C。  ← 需要知道上文是问天气
```

**但是！HTTP 是无状态的**，每次请求都是新的 `ChatContext`，所以 Step 3 无法真正实现上下文。这就是下一章要解决的问题！

---

## 第五步：运行和测试

### 编译

```bash
cd src/step03
g++ -std=c++17 main.cpp -o server -lboost_system -lpthread
```

### 测试

```bash
# 启动服务器
./server

# 在另一个终端测试
curl -X POST -d "你好" http://localhost:8080/chat
# 输出：{"reply": "你好！我是 NuClaw AI..."}

curl -X POST -d "现在几点" http://localhost:8080/chat
# 输出：{"reply": "2024-01-15 14:30:25"}

curl -X POST -d "北京天气" http://localhost:8080/chat
# 输出：{"reply": "北京今天晴天，25°C..."}
```

### 测试上下文（会失败）

```bash
# 第一次请求
curl -X POST -d "北京天气" http://localhost:8080/chat
# → 北京今天晴天

# 第二次请求（紧接着）
curl -X POST -d "那上海呢" http://localhost:8080/chat
# → "我不理解"  ← 失败！上下文丢失了
```

**失败原因**：HTTP 是无状态协议，两次请求之间没有关联。`ChatContext` 每次都是新的。

---

## 第六步：常见问题

### Q1: 为什么我的正则匹配不到？

**可能原因：**
1. **大小写问题**：默认是区分大小写的，要加 `std::regex::icase`
2. **特殊字符**：`.` `*` `+` 等特殊字符需要转义
3. **匹配函数**：`regex_search` 是部分匹配，`regex_match` 是完全匹配

**调试方法：**
```cpp
std::string input = "Hello World";
std::regex pattern("hello", std::regex::icase);

if (std::regex_search(input, pattern)) {
    std::cout << "匹配成功!\n";
} else {
    std::cout << "匹配失败\n";
}
```

### Q2: 如何添加新规则？

**三步走：**
1. 想好关键词（如："帮助"、"help"）
2. 编写正则：`std::regex("帮助|help", std::regex::icase)`
3. 在 `init_rules()` 中添加：

```cpp
rules_.push_back({
    std::regex("帮助|help", std::regex::icase),
    "help",
    [](const std::string&) {
        return "我可以帮你：\n1. 查天气\n2. 看时间\n3. 问候聊天";
    }
});
```

### Q3: 规则太多会不会很慢？

**会！** 这就是规则系统的局限。

**优化方法：**
1. 把最常用的规则放在前面
2. 用更精确的正则减少匹配次数
3. 最终解决方案：Step 5 用 LLM 替代规则

---

## 本节总结

### 核心概念

1. **Agent Loop**：输入 → 理解 → 行动 → 输出的循环
2. **规则系统**：用 if-else 实现简单的"智能"
3. **正则表达式**：模式匹配的强大工具
4. **上下文管理**：实现多轮对话的关键

### 代码演进

```
Step 2: HTTP Router (271行)
   ↓ 新增约 80 行
Step 3: Rule-based AI (350行)
   - ChatEngine 类
   - 规则系统
   - 上下文管理
```

### 暴露的问题

1. **HTTP 无状态**：上下文无法持久化
2. **规则死板**：只能匹配预设模式
3. **维护困难**：规则多了难以管理

### 下章预告

**Step 4: WebSocket 实时通信**
- 解决 HTTP 上下文丢失问题
- 实现真正的多轮对话

---

## 📝 课后练习

### 练习 1：添加新规则
添加一个"笑话"规则：
- 关键词："笑话"、"搞笑"、"joke"
- 回复：一个简短的笑话

### 练习 2：改进天气规则
让天气规则支持更多城市：
- 添加：深圳、广州、成都
- 每个城市返回不同的天气描述

### 练习 3：调试正则
测试以下输入是否能正确匹配：
- "现在时间" → 应该匹配时间规则
- "今天的天气" → 应该匹配天气规则
- "Hello" → 应该匹配问候规则

### 思考题
1. 规则系统的优缺点分别是什么？
2. 为什么 HTTP 无法实现真正的上下文？
3. 如果要支持 100 个城市，规则系统还能用吗？

---

## 📖 扩展阅读

### 正则表达式速查

| 符号 | 含义 | 示例 |
|:---|:---|:---|
| `.` | 任意字符 | `a.c` 匹配 abc, aac |
| `*` | 0或多个 | `ab*` 匹配 a, ab, abb |
| `+` | 1或多个 | `ab+` 匹配 ab, abb |
| `?` | 0或1个 | `ab?` 匹配 a, ab |
| `|` | 或 | `a|b` 匹配 a 或 b |
| `()` | 分组 | `(ab)+` 匹配 ab, abab |
| `[]` | 字符类 | `[abc]` 匹配 a, b, 或 c |
| `^` | 开头 | `^abc` 匹配以 abc 开头 |
| `$` | 结尾 | `abc$` 匹配以 abc 结尾 |

### C++ 正则函数

```cpp
// 检查是否包含模式
std::regex_search(string, pattern)

// 检查是否完全匹配
std::regex_match(string, pattern)

// 替换匹配内容
std::regex_replace(string, pattern, replacement)
```

---

**恭喜！** 你已经实现了第一个 AI Agent（虽然还很简陋）。下一章我们将解决 HTTP 的问题，实现真正的对话上下文。
