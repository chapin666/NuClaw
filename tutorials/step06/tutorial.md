# Step 6: 工具调用 - 让 LLM 能"做事"

> 目标：理解工具系统架构，实现 LLM + 工具的完整 Agent
> 
> 难度：⭐⭐⭐ (中等)
> 
> 代码量：约 550 行（分散在 10 个文件中）
> 
> 预计学习时间：4-5 小时

---

## 📚 前置知识

### 为什么 LLM 需要工具？

**LLM 的能力边界：**

| 能力 | 说明 | 示例 |
|:---|:---|:---|
| ✅ 语言理解 | 理解自然语言 | 理解"今天适合出门吗"是在问天气 |
| ✅ 知识回忆 | 回忆训练数据 | 知道巴黎是法国首都 |
| ❌ 实时信息 | 获取当前数据 | 不知道现在几点、北京天气 |
| ❌ 数学计算 | 精确计算 | 大数乘法容易出错 |
| ❌ 外部操作 | 调用 API/查数据库 | 无法直接操作外部系统 |

**类比：LLM 像是一个博学但闭门不出的教授**
- 他读过很多书，知识渊博
- 但他不看新闻，不知道外面现在发生了什么
- 他不会用电脑，不能上网查资料
- 他需要一个助手（工具）帮他和外界交互

### 工具调用的核心思想

**ReAct 模式（推理 + 行动）：**

```
思考(Reason) → 行动(Act) → 观察(Observe) → ... → 回答
```

**具体流程：**

```
用户：北京天气如何？
    ↓
LLM 思考：用户问天气，我需要天气数据
    ↓
LLM 决策：调用 get_weather 工具，参数：city="北京"
    ↓
系统执行：get_weather("北京") → {"temp": 25, "weather": "晴天"}
    ↓
LLM 观察：拿到天气数据了
    ↓
LLM 生成回复："北京今天晴天，25°C，很适合出门！"
    ↓
用户：收到回复
```

**这就是完整的 Agent Loop！**

### 工具系统的架构

```
┌─────────────────────────────────────────────────────────────┐
│                     工具系统架构                             │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│   ┌─────────────┐                                          │
│   │    用户     │                                          │
│   └──────┬──────┘                                          │
│          │ 输入问题                                         │
│          ▼                                                  │
│   ┌─────────────────────────────────────────────────────┐   │
│   │                   Agent Core                         │   │
│   │  ┌─────────────────────────────────────────────┐    │   │
│   │  │  LLM (大脑)                                  │    │   │
│   │  │  • 理解用户意图                               │    │   │
│   │  │  • 决定调用哪个工具                           │    │   │
│   │  │  • 根据工具结果生成回复                        │    │   │
│   │  └─────────────────────────────────────────────┘    │   │
│   │                      │                              │   │
│   │  ┌───────────────────┼───────────────────┐          │   │
│   │  │                   │                   │          │   │
│   │  ▼                   ▼                   ▼          │   │
│   │ ┌────────┐      ┌────────┐      ┌─────────┐        │   │
│   │ │Weather │      │  Time  │      │  Calc   │        │   │
│   │ │ Tool   │      │  Tool  │      │  Tool   │        │   │
│   │ └────────┘      └────────┘      └─────────┘        │   │
│   │      │               │                │             │   │
│   │      └───────────────┼────────────────┘             │   │
│   │                      ▼                              │   │
│   │            ┌──────────────────┐                     │   │
│   │            │   ToolExecutor   │                     │   │
│   │            │   (工具执行器)    │                     │   │
│   │            └──────────────────┘                     │   │
│   └─────────────────────────────────────────────────────┘   │
│                          │                                  │
│                          ▼                                  │
│                   ② 返回结果                                │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

**关键组件：**
1. **Tool（工具）**：具体的功能实现（天气、时间、计算）
2. **ToolExecutor（执行器）**：管理工具的执行
3. **LLM（大脑）**：决策是否调用工具、调用哪个工具
4. **Agent Core**：协调各组件

---

## 第一步：LLM 的局限性深度分析

### Step 5 的问题复盘

```
用户：北京现在多少度？
LLM：抱歉，我没有实时天气数据。

用户：现在几点？
LLM：我无法获取当前时间。

用户：23456789 * 98765432 等于多少？
LLM：可能是 231855...（算错了）
```

**根本原因分析：**

| 问题 | 原因 | 解决方案 |
|:---|:---|:---|
| 不知道实时天气 | 训练数据截止到某时间点 | 调用天气 API |
| 不知道当前时间 | 没有实时时钟 | 调用系统时间 |
| 大数计算错误 | 神经网络不擅长精确计算 | 调用计算器 |

### 工具的本质：扩展 LLM 的能力边界

**没有工具的 LLM：**
```
知识范围：训练数据中的静态知识
时间感知：无
计算能力：近似计算，容易出错
外部交互：无
```

**有工具的 LLM（Agent）：**
```
知识范围：训练数据 + 实时数据（通过工具获取）
时间感知：通过工具获取当前时间
计算能力：精确计算（通过计算器工具）
外部交互：可以操作外部系统（通过 API 工具）
```

---

## 第二步：工具系统设计

### 工具接口定义

首先定义工具的通用接口：

```cpp
// tool.hpp - 工具基类
struct ToolResult {
    bool success;           // 是否成功
    std::string data;       // 返回数据（JSON 格式）
    std::string error;      // 错误信息（失败时）
    
    static ToolResult ok(const std::string& data) {
        return {true, data, ""};
    }
    
    static ToolResult fail(const std::string& error) {
        return {false, "", error};
    }
};

struct ToolCall {
    std::string name;       // 工具名称
    std::string arguments;  // 参数（JSON 格式）
};
```

**为什么用 JSON？**
- 结构化数据，易于解析
- 通用格式，各种语言都支持
- 可以表示复杂的数据结构

### 具体工具实现

#### 1. 天气工具（WeatherTool）

```cpp
// weather_tool.hpp
class WeatherTool {
public:
    static std::string get_name() { return "get_weather"; }
    
    static std::string get_description() {
        return "查询指定城市的天气信息，支持北京、上海、深圳、广州";
    }
    
    static ToolResult execute(const std::string& city) {
        // 模拟天气数据（实际应该调用天气 API）
        if (city == "北京") {
            return ToolResult::ok(R"({
                "city": "北京",
                "weather": "晴天",
                "temp": 25,
                "aqi": 50
            })");
        }
        else if (city == "上海") {
            return ToolResult::ok(R"({
                "city": "上海",
                "weather": "多云",
                "temp": 22,
                "aqi": 45
            })");
        }
        // ... 其他城市
        
        return ToolResult::fail("不支持的城市: " + city);
    }
};
```

#### 2. 时间工具（TimeTool）

```cpp
// time_tool.hpp
class TimeTool {
public:
    static std::string get_name() { return "get_time"; }
    
    static ToolResult execute(const std::string& timezone = "") {
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        
        char buf[100];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", 
                     std::localtime(&t));
        
        return ToolResult::ok(R"({
            "datetime": ")" + std::string(buf) + R"(",
            "timezone": ")" + (timezone.empty() ? "本地" : timezone) + R"("
        })");
    }
};
```

#### 3. 计算器工具（CalcTool）

```cpp
// calc_tool.hpp
class CalcTool {
public:
    static std::string get_name() { return "calculate"; }
    
    static ToolResult execute(const std::string& expression) {
        try {
            // 解析 "1 + 2" 格式的表达式
            double a, b;
            char op;
            std::istringstream iss(expression);
            iss >> a >> op >> b;
            
            double result;
            switch (op) {
                case '+': result = a + b; break;
                case '-': result = a - b; break;
                case '*': result = a * b; break;
                case '/': 
                    if (b == 0) return ToolResult::fail("除数不能为零");
                    result = a / b; 
                    break;
                default: 
                    return ToolResult::fail("不支持的操作符");
            }
            
            return ToolResult::ok(R"({
                "expression": ")" + expression + R"(",
                "result": )" + std::to_string(result) + R"(
            })");
        } catch (...) {
            return ToolResult::fail("计算错误");
        }
    }
};
```

### 工具执行器

```cpp
// tool_executor.hpp
class ToolExecutor {
public:
    // 执行工具调用
    static ToolResult execute(const ToolCall& call) {
        std::cout << "[⚙️ 执行工具] " << call.name << std::endl;
        
        // 硬编码分发 - 这就是问题所在！
        if (call.name == "get_weather") {
            return WeatherTool::execute(call.arguments);
        }
        else if (call.name == "get_time") {
            return TimeTool::execute(call.arguments);
        }
        else if (call.name == "calculate") {
            return CalcTool::execute(call.arguments);
        }
        
        return ToolResult::fail("未知工具: " + call.name);
    }
};
```

**硬编码的问题：**
- 每加一个工具都要改这里
- 违反开闭原则（对扩展开放，对修改关闭）
- 下一章用注册表模式解决

---

## 第三步：LLM 决策流程

### 如何判断是否需要工具？

```cpp
class LLMClient {
public:
    // 判断用户问题是否需要工具
    bool needs_tool(const std::string& input) const {
        // 简单版本：关键词匹配
        // 真实 LLM 会基于语义判断
        
        std::vector<std::string> tool_keywords = {
            "天气", "温度", "下雨", "晴天",    // 需要天气工具
            "时间", "几点", "日期", "现在",    // 需要时间工具
            "计算", "等于", "+", "-", "*", "/"  // 需要计算工具
        };
        
        for (const auto& kw : tool_keywords) {
            if (input.find(kw) != std::string::npos) {
                return true;
            }
        }
        return false;
    }
};
```

### 如何解析工具调用？

```cpp
// 从用户输入中提取工具名称和参数
ToolCall parse_tool_call(const std::string& user_input) {
    ToolCall call;
    
    // 简化版本：关键词匹配
    // 真实 LLM 会输出结构化的工具调用
    
    if (user_input.find("天气") != std::string::npos) {
        call.name = "get_weather";
        // 提取城市名
        if (user_input.find("北京") != std::string::npos) 
            call.arguments = "北京";
        else if (user_input.find("上海") != std::string::npos) 
            call.arguments = "上海";
        else 
            call.arguments = "北京"; // 默认
    }
    else if (user_input.find("时间") != std::string::npos) {
        call.name = "get_time";
        call.arguments = "";
    }
    else if (user_input.find("计算") != std::string::npos ||
             user_input.find("+") != std::string::npos) {
        call.name = "calculate";
        call.arguments = extract_expression(user_input);
    }
    
    return call;
}
```

### 完整的决策流程

```cpp
// chat_engine.hpp
class ChatEngine {
public:
    std::string process(const std::string& user_input, ChatContext& ctx) {
        std::cout << "[🧠 处理] \"" << user_input << "\"" << std::endl;
        
        // Step 1: 判断是否需要工具
        if (llm_.needs_tool(user_input)) {
            std::cout << "  → 需要工具" << std::endl;
            
            // Step 2: 解析工具调用
            ToolCall call = llm_.parse_tool_call(user_input);
            std::cout << "  → 工具: " << call.name << std::endl;
            
            // Step 3: 执行工具
            ToolResult result = ToolExecutor::execute(call);
            std::cout << "  → 结果: " << (result.success ? "成功" : "失败") << std::endl;
            
            // Step 4: 根据结果生成回复
            return generate_reply(user_input, result, call);
        }
        else {
            std::cout << "  → 直接回复" << std::endl;
            return llm_.direct_reply(user_input);
        }
    }

private:
    std::string generate_reply(const std::string& input,
                               const ToolResult& result,
                               const ToolCall& call) {
        if (!result.success) {
            return "抱歉，" + call.name + "执行失败: " + result.error;
        }
        
        // 解析工具返回的 JSON
        json::value data = json::parse(result.data);
        
        if (call.name == "get_weather") {
            std::string city = data["city"].as_string();
            std::string weather = data["weather"].as_string();
            int temp = data["temp"].as_int64();
            
            return city + "今天" + weather + "，气温" + 
                   std::to_string(temp) + "°C。";
        }
        else if (call.name == "get_time") {
            return "当前时间是 " + data["datetime"].as_string();
        }
        else if (call.name == "calculate") {
            return data["expression"].as_string() + " = " + 
                   std::to_string(data["result"].as_double());
        }
        
        return "工具执行成功";
    }
};
```

---

## 第四步：代码结构详解

### 文件职责说明

| 文件 | 职责 | 关键类/函数 |
|:---|:---|:---|
| `tool.hpp` | 定义工具接口 | `ToolResult`, `ToolCall` |
| `weather_tool.hpp` | 天气工具实现 | `WeatherTool::execute()` |
| `time_tool.hpp` | 时间工具实现 | `TimeTool::execute()` |
| `calc_tool.hpp` | 计算器工具 | `CalcTool::execute()` |
| `tool_executor.hpp` | 工具调度执行 | `ToolExecutor::execute()` |
| `llm_client.hpp` | LLM 客户端 | `LLMClient::needs_tool()` |
| `chat_engine.hpp` | Agent 核心 | `ChatEngine::process()` |
| `server.hpp` | WebSocket 服务 | `WebSocketSession` |
| `main.cpp` | 程序入口 | `main()` |

### 数据流图

```
用户输入
    ↓
WebSocketSession::on_read()
    ↓
ChatEngine::process()
    ↓
判断：需要工具？
    ├── 否 → LLMClient::direct_reply() → 返回
    ↓
    是
    ↓
LLMClient::parse_tool_call()
    ↓
ToolExecutor::execute()
    ↓
分发到具体工具
    ├── WeatherTool::execute()
    ├── TimeTool::execute()
    └── CalcTool::execute()
    ↓
返回 ToolResult
    ↓
ChatEngine::generate_reply()
    ↓
返回给用户
```

---

## 第五步：运行测试

### 编译

```bash
cd src/step06
mkdir build && cd build
cmake .. && make
./nuclaw_step06
```

### 功能测试

```bash
wscat -c ws://localhost:8080/ws

# 1. 问候（不需要工具）
> 你好
< 你好！我是带工具调用能力的 AI。
< 我可以帮你：查天气、算数学、看时间。

# 2. 天气查询（调用工具）
> 北京天气如何？
[🧠 处理] "北京天气如何？"
  → 需要工具
  → 工具: get_weather
  → 结果: 成功
< 北京今天晴天，气温25°C。

# 3. 时间查询（调用工具）
> 现在几点？
[🧠 处理] "现在几点？"
  → 需要工具
  → 工具: get_time
  → 结果: 成功
< 当前时间是 2024-01-15 14:30:25

# 4. 计算（调用工具）
> 12345 * 67890 等于多少？
[🧠 处理] "12345 * 67890 等于多少？"
  → 需要工具
  → 工具: calculate
  → 结果: 成功
< 12345 * 67890 = 838102050
```

---

## 第六步：硬编码问题分析

### 当前实现的问题

```cpp
// tool_executor.hpp - 硬编码分发
static ToolResult execute(const ToolCall& call) {
    if (call.name == "get_weather") 
        return WeatherTool::execute(call.arguments);
    if (call.name == "get_time") 
        return TimeTool::execute(call.arguments);
    if (call.name == "calculate") 
        return CalcTool::execute(call.arguments);
    // 每加一个工具都要改这里！
    return ToolResult::fail("未知工具");
}
```

**问题：**
1. **扩展困难**：加新工具要修改核心代码
2. **违反开闭原则**：对修改不开放
3. **耦合严重**：执行器依赖所有工具

### 更好的设计（下一章实现）

```cpp
// 注册表模式
class ToolRegistry {
    std::map<std::string, std::shared_ptr<Tool>> tools_;
    
public:
    void register_tool(std::shared_ptr<Tool> tool) {
        tools_[tool->get_name()] = tool;
    }
    
    ToolResult execute(const std::string& name, const std::string& args) {
        auto it = tools_.find(name);
        if (it != tools_.end()) {
            return it->second->execute(args);
        }
        return ToolResult::fail("未知工具");
    }
};

// 使用
registry.register_tool(std::make_shared<WeatherTool>());
registry.register_tool(std::make_shared<TimeTool>());
// 新增工具只需要一行！
```

---

## 本节总结

### 核心概念

1. **工具调用**：扩展 LLM 能力的关键机制
2. **ReAct 模式**：思考 → 行动 → 观察 → 回答
3. **工具接口**：统一的输入输出规范
4. **硬编码问题**：维护困难，需要注册表模式

### Agent Loop 完整了！

```
感知（用户输入）
    ↓
理解（LLM 语义分析）
    ↓
决策（判断需要工具）
    ↓
行动（执行工具）
    ↓
观察（获取工具结果）
    ↓
响应（生成自然语言回复）
```

### 代码演进

```
Step 5: WebSocket + LLM (450行)
   ↓ + 工具系统
Step 6: 550行（10个文件）
   + tool.hpp, *_tool.hpp
   + tool_executor.hpp
   + 工具调用流程
```

### 仍存在的问题

**硬编码难以扩展：**
- 每加工具要改执行器
- 下一章用注册表模式解决

**同步执行阻塞：**
- 工具执行时无法处理其他请求
- Step 7 用异步解决

**没有安全控制：**
- 工具可能被滥用
- Step 8 用沙箱解决

---

## 📝 课后练习

### 练习 1：添加新工具
添加一个"随机数"工具：
- 名称：`random_number`
- 功能：生成指定范围的随机数
- 使用：`random 1 100` → 返回 1-100 之间的随机数

### 练习 2：改进参数解析
让天气工具支持更自然的表达：
- "北京今天天气怎么样" → 提取"北京"
- "查询上海的天气" → 提取"上海"

### 练习 3：工具链
实现组合工具调用：
- 用户："现在北京天气如何，适合出门吗？"
- 系统：调用时间工具 + 天气工具 → 综合判断

### 思考题
1. 工具系统和函数调用有什么区别？
2. 如果工具执行很慢，会怎样影响用户体验？
3. 如何设计一个通用的工具接口，支持任意类型的工具？

---

## 📖 扩展阅读

### 工具设计最佳实践

1. **单一职责**：每个工具只做一件事
2. **幂等性**：同样的输入，应该得到同样的输出
3. **错误处理**：详细的错误信息
4. **参数校验**：输入参数的合法性检查

### 工具调用 vs Function Calling

| 特性 | 工具调用 | Function Calling |
|:---|:---|:---|
| 来源 | 本文实现 | OpenAI API |
| 复杂度 | 简单 | 复杂 |
| 灵活性 | 高 | 受限于 API |
| 学习价值 | 高（理解原理）| 高（了解业界方案）|

### 真实世界的工具系统

- **OpenAI Functions**：GPT-4 的函数调用能力
- **LangChain**：Python 的 LLM 应用框架
- **AutoGPT**：自主决策的 AI Agent

---

**恭喜！** 你现在有了一个完整的工具系统。下一章我们将让它支持异步执行，提升性能。
