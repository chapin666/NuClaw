# Step 4-6 详细设计

## Step 4: 规则 AI（基于 Step 3 WebSocket）

### 目标
在 WebSocket 通信基础上，实现简单的"智能"回复。

### 演进变化
```cpp
// Step 3: 纯 Echo
ws.on_message = [](std::string msg) {
    return msg;  // 原样返回
};

// Step 4: 规则匹配
ws.on_message = [](std::string msg) {
    if (msg.find("你好") != npos) return "你好！很高兴见到你";
    if (msg.find("时间") != npos) return get_current_time();
    return "我不理解";
};
```

### 新增组件
1. **ChatEngine 类** - 消息处理引擎
   - 规则列表（vector<pair<regex, response>>）
   - 上下文记忆（最近3轮对话）

2. **Context 类** - 会话上下文
   - user_id
   - message_history
   - last_intent

### 代码结构
```cpp
// chat_engine.hpp
class ChatEngine {
public:
    std::string process(const std::string& input, Context& ctx);
    
private:
    std::vector<Rule> rules_;
};

// 使用
ChatEngine engine;
ws.on_message = [&engine](std::string msg, Context& ctx) {
    return engine.process(msg, ctx);
};
```

### 教程重点
- 什么是"智能"？从固定到规则
- 正则表达式匹配
- 上下文的重要性（多轮对话）
- 规则系统的局限性（为 Step 5 铺垫）

---

## Step 5: LLM 接入（基于 Step 4）

### 目标
接入真实的 OpenAI API，替代规则匹配。

### 演进变化
```cpp
// Step 4: 规则匹配
class ChatEngine {
    std::string process(const std::string& input, Context& ctx) {
        for (auto& rule : rules_) {
            if (regex_match(input, rule.pattern)) 
                return rule.response;
        }
        return "不理解";
    }
};

// Step 5: LLM 调用
class ChatEngine {
    LLMClient llm_;  // 新增
    
    std::string process(const std::string& input, Context& ctx) {
        // 构建 prompt
        std::string prompt = build_prompt(input, ctx.history);
        // 调用 LLM
        return llm_.complete(prompt);
    }
};
```

### 新增组件
1. **LLMClient 类** - HTTP 客户端封装
   - OpenAI API 调用
   - JSON 构造和解析
   - 错误处理

2. **PromptBuilder** - 提示词构建
   - system prompt
   - history 格式化
   - user input

### 代码结构
```cpp
// llm_client.hpp
class LLMClient {
public:
    LLMClient(const std::string& api_key);
    std::string complete(const std::string& prompt);
    
private:
    std::string api_key_;
    std::string api_endpoint_ = "https://api.openai.com/v1/chat/completions";
};

// 使用
LLMClient llm("sk-xxx");
ChatEngine engine(llm);
```

### 教程重点
- 规则维护困难的真实案例
- LLM 是什么（概念科普）
- HTTP POST 请求构造
- API Key 管理
- Token 和计费概念

---

## Step 6: 工具调用（基于 Step 5）

### 目标
让 LLM 能调用外部工具获取实时数据。

### 演进变化
```cpp
// Step 5: 纯 LLM
std::string process(const std::string& input, Context& ctx) {
    std::string prompt = build_prompt(input, ctx.history);
    return llm_.complete(prompt);  // 直接返回
}

// Step 6: LLM + 工具
std::string process(const std::string& input, Context& ctx) {
    // 1. 让 LLM 决定是否需要工具
    std::string prompt = build_prompt_with_tools(input, ctx.history);
    std::string decision = llm_.complete(prompt);
    
    // 2. 解析工具调用（简化版）
    if (decision contains "CALL:") {
        auto [tool_name, args] = parse_tool_call(decision);
        std::string result = execute_tool(tool_name, args);
        // 3. 返回给 LLM 生成最终回复
        return llm_.complete("工具结果：" + result);
    }
    
    return decision;  // 直接回答
}
```

### 新增组件
1. **Tool 基类** - 工具接口（硬编码版）
   ```cpp
   class Tool {
   public:
       virtual std::string name() const = 0;
       virtual std::string execute(const std::string& args) = 0;
   };
   ```

2. **具体工具实现**（硬编码）
   - WeatherTool: mock 天气数据
   - CalculatorTool: 简单计算
   - TimeTool: 返回当前时间

3. **ToolExecutor** - 工具执行器
   - 根据名称调用对应工具
   - 错误处理

### 代码结构
```cpp
// tools.hpp
class WeatherTool : public Tool {
public:
    std::string name() const override { return "get_weather"; }
    std::string execute(const std::string& args) override {
        // mock 实现
        return "{\"city\": \"" + args + "\", \"temp\": 25}";
    }
};

// chat_engine.cpp
class ChatEngine {
    LLMClient llm_;
    std::map<std::string, std::unique_ptr<Tool>> tools_;
    
public:
    ChatEngine(LLMClient& llm) : llm_(llm) {
        // 硬编码注册工具
        tools_["get_weather"] = std::make_unique<WeatherTool>();
        tools_["calculate"] = std::make_unique<CalculatorTool>();
    }
};
```

### 教程重点
- LLM 知识截止问题（无法知道实时信息）
- Function Calling 机制
- 硬编码工具的问题（每加一个都要改代码）
- 为 Step 7 的系统化铺垫

---

## 三阶段衔接

### Step 4 → Step 5
```
问题：规则维护困难
解决：用 LLM 理解语义
保留：上下文管理、对话流程
新增：HTTP 客户端、API 调用
```

### Step 5 → Step 6
```
问题：LLM 不知道实时信息
解决：加入工具调用
保留：LLMClient、对话流程
新增：Tool 基类、硬编码工具
```

### 关键检查点
- [ ] Step 4 的规则系统是否让读者理解"智能"概念？
- [ ] Step 5 是否解释清楚为什么不用规则了？
- [ ] Step 6 的工具调用是否自然（LLM 需要它）？
- [ ] 硬编码工具是否让读者觉得"需要改进"？
