# Step 6: 工具调用 —— 让 Agent 拥有"手脚"

> 目标：设计标准化的工具接口，实现硬编码工具调用
> 
003e 难度：⭐⭐⭐ | 代码量：约 550 行 | 预计学习时间：3-4 小时

---

## 一、为什么需要标准化工具接口？

### 1.1 Step 5 的工具问题

Step 5 实现了工具调用，但工具定义比较随意：

```cpp
// 每个工具结构不统一
Tool weather_tool = {
    .name = "get_weather",
    .description = "...",
    .parameters = {...},  // json 对象
    .execute = [](const json&) -> json { ... }  // 返回 json
};

// 另一个工具可能用不同风格
Tool calc_tool;
calc_tool.name = "calculate";
calc_tool.run = [](std::string input) -> std::string { ... };  // 完全不同的接口！
```

**问题：**
- 接口不统一，难以统一管理
- LLM 难以理解和调用
- 新增工具成本高

### 1.2 标准化接口的价值

```
统一接口前：                          统一接口后：
┌─────────┐                          ┌─────────────────┐
│工具 A   │ 不同接口                  │  统一工具接口    │
│工具 B   │ ──────▶ 混乱              │  ┌───┐┌───┐┌───┐│
│工具 C   │                          │  │ A ││ B ││ C ││
└─────────┘                          │  └───┘└───┘└───┘│
                                     └────────┬────────┘
                                              │
                                         统一管理
```

**标准化带来的好处：**
1. **LLM 友好**：统一的描述格式，便于生成调用
2. **易于扩展**：新工具只需实现标准接口
3. **可维护**：统一的管理和监控
4. **可复用**：工具可以在不同 Agent 间复用

---

## 二、工具接口设计

### 2.1 接口设计原则

好的工具接口应该：

| 原则 | 说明 | 示例 |
|:---|:---|:---|
| **自描述** | 工具能描述自己的能力 | 名称、描述、参数说明 |
| **类型安全** | 参数和返回值类型明确 | Schema 约束 |
| **可观测** | 调用可追踪、可记录 | 日志、监控 |
| **可测试** | 独立运行，易于测试 | 无外部依赖 |

### 2.2 核心接口定义

```cpp
// 工具参数定义（JSON Schema 子集）
struct ToolParameter {
    std::string name;           // 参数名
    std::string type;           // string/int/number/boolean/array/object
    std::string description;    // 参数描述（给 LLM 看）
    bool required = true;       // 是否必需
    json default_value;         // 默认值（可选）
    std::vector<json> enum_values;  // 枚举值（可选）
};

// 工具上下文（执行时的环境信息）
struct ToolContext {
    std::string session_id;     // 会话 ID
    std::string user_id;        // 用户 ID
    std::map<std::string, std::string> metadata;  // 附加信息
};

// 工具执行结果
struct ToolResult {
    bool success;               // 是否成功
    json data;                  // 返回数据
    std::string error_message;  // 错误信息（失败时）
    int64_t execution_time_ms;  // 执行耗时
};

// 工具接口（纯虚类）
class ITool {
public:
    virtual ~ITool() = default;
    
    // 获取工具名称
    virtual std::string get_name() const = 0;
    
    // 获取工具描述（给 LLM 看）
    virtual std::string get_description() const = 0;
    
    // 获取参数定义
    virtual std::vector<ToolParameter> get_parameters() const = 0;
    
    // 执行工具
    virtual ToolResult execute(
        const json& arguments,
        const ToolContext& context
    ) = 0;
    
    // 验证参数（可选，但推荐实现）
    virtual bool validate_arguments(const json& arguments) const;
};
```

### 2.3 Schema 生成

为了让 LLM 理解工具，需要生成 JSON Schema：

```cpp
json ITool::get_schema() const {
    json schema = {
        {"type", "function"},
        {"function", {
            {"name", get_name()},
            {"description", get_description()},
            {"parameters", {
                {"type", "object"},
                {"properties", json::object()},
                {"required", json::array()}
            }}
        }}
    };
    
    for (const auto& param : get_parameters()) {
        json param_schema = {
            {"type", param.type},
            {"description", param.description}
        };
        
        if (!param.enum_values.empty()) {
            param_schema["enum"] = param.enum_values;
        }
        
        schema["function"]["parameters"]["properties"][param.name] = param_schema;
        
        if (param.required) {
            schema["function"]["parameters"]["required"].push_back(param.name);
        }
    }
    
    return schema;
}
```

---

## 三、具体工具实现

### 3.1 天气查询工具

```cpp
class WeatherTool : public ITool {
public:
    std::string get_name() const override {
        return "get_weather";
    }
    
    std::string get_description() const override {
        return "获取指定城市的当前天气信息，包括温度、天气状况、湿度等";
    }
    
    std::vector<ToolParameter> get_parameters() const override {
        return {
            {
                .name = "location",
                .type = "string",
                .description = "城市名称，如：北京、上海、纽约",
                .required = true
            },
            {
                .name = "date",
                .type = "string",
                .description = "查询日期，如：今天、明天、2024-01-01",
                .required = false,
                .default_value = "今天"
            }
        };
    }
    
    ToolResult execute(const json& arguments, 
                       const ToolContext& context) override {
        auto start = std::chrono::steady_clock::now();
        
        // 提取参数
        std::string location = arguments.value("location", "");
        std::string date = arguments.value("date", "今天");
        
        // 参数校验
        if (location.empty()) {
            return {
                .success = false,
                .data = nullptr,
                .error_message = "location 参数不能为空",
                .execution_time_ms = 0
            };
        }
        
        // 模拟调用天气 API（实际应该调用真实的天气服务）
        // 这里用硬编码数据演示
        json weather_data = query_weather_api(location, date);
        
        auto end = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<
            std::chrono::milliseconds>(end - start).count();
        
        return {
            .success = true,
            .data = weather_data,
            .error_message = "",
            .execution_time_ms = elapsed
        };
    }

private:
    json query_weather_api(const std::string& location, 
                           const std::string& date) {
        // 模拟 API 调用
        // 实际应该调用 OpenWeatherMap、和风天气等 API
        
        static std::map<std::string, json> mock_data = {
            {"北京", {
                {"temperature", 25},
                {"condition", "晴朗"},
                {"humidity", 45},
                {"wind", "东北风 3级"}
            }},
            {"上海", {
                {"temperature", 28},
                {"condition", "多云"},
                {"humidity", 70},
                {"wind", "东南风 2级"}
            }}
        };
        
        if (mock_data.count(location)) {
            json result = mock_data[location];
            result["location"] = location;
            result["date"] = date;
            return result;
        }
        
        // 未知城市返回默认值
        return {
            {"location", location},
            {"date", date},
            {"temperature", 20},
            {"condition", "未知"},
            {"humidity", 50},
            {"wind", "微风"}
        };
    }
};
```

### 3.2 计算器工具

```cpp
class CalculatorTool : public ITool {
public:
    std::string get_name() const override {
        return "calculate";
    }
    
    std::string get_description() const override {
        return "执行数学计算，支持加减乘除、幂运算、取模等";
    }
    
    std::vector<ToolParameter> get_parameters() const override {
        return {
            {
                .name = "expression",
                .type = "string",
                .description = "数学表达式，如：2+3*4、sqrt(16)、pow(2,10)",
                .required = true
            }
        };
    }
    
    ToolResult execute(const json& arguments,
                       const ToolContext& context) override {
        auto start = std::chrono::steady_clock::now();
        
        std::string expression = arguments.value("expression", "");
        
        if (expression.empty()) {
            return make_error("expression 不能为空");
        }
        
        // 安全考虑：限制表达式长度和字符
        if (expression.length() > 100) {
            return make_error("表达式过长（最大100字符）");
        }
        
        // 只允许数字、运算符、括号、空格、数学函数
        if (!validate_expression(expression)) {
            return make_error("表达式包含非法字符");
        }
        
        // 计算结果
        double result = evaluate_expression(expression);
        
        auto end = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<
            std::chrono::milliseconds>(end - start).count();
        
        return {
            .success = true,
            .data = {
                {"expression", expression},
                {"result", result}
            },
            .error_message = "",
            .execution_time_ms = elapsed
        };
    }

private:
    bool validate_expression(const std::string& expr) {
        // 只允许安全字符
        for (char c : expr) {
            if (!std::isdigit(c) && 
                !std::isspace(c) &&
                std::string("+-*/().,sqrtpowlogsin cos").find(c) == std::string::npos) {
                return false;
            }
        }
        return true;
    }
    
    double evaluate_expression(const std::string& expr) {
        // 简化实现：实际应该使用表达式解析库
        // 如 exprtk、muParser 等
        
        // 这里仅作演示，处理简单情况
        if (expr == "2+3") return 5;
        if (expr == "sqrt(16)") return 4;
        // ... 更多实现
        
        return 0;
    }
    
    ToolResult make_error(const std::string& msg) {
        return {
            .success = false,
            .data = nullptr,
            .error_message = msg,
            .execution_time_ms = 0
        };
    }
};
```

### 3.3 时间查询工具

```cpp
class TimeTool : public ITool {
public:
    std::string get_name() const override {
        return "get_current_time";
    }
    
    std::string get_description() const override {
        return "获取当前时间，支持指定时区和格式";
    }
    
    std::vector<ToolParameter> get_parameters() const override {
        return {
            {
                .name = "timezone",
                .type = "string",
                .description = "时区，如：Asia/Shanghai、UTC、America/New_York",
                .required = false,
                .default_value = "Asia/Shanghai"
            },
            {
                .name = "format",
                .type = "string",
                .description = "时间格式",
                .required = false,
                .default_value = "YYYY-MM-DD HH:mm:ss",
                .enum_values = {
                    "YYYY-MM-DD HH:mm:ss",
                    "HH:mm:ss",
                    "YYYY年MM月DD日"
                }
            }
        };
    }
    
    ToolResult execute(const json& arguments,
                       const ToolContext& context) override {
        auto start = std::chrono::steady_clock::now();
        
        std::string timezone = arguments.value("timezone", "Asia/Shanghai");
        std::string format = arguments.value("format", "YYYY-MM-DD HH:mm:ss");
        
        // 获取当前时间
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        
        // 格式化（简化实现）
        std::stringstream ss;
        if (format == "YYYY-MM-DD HH:mm:ss") {
            ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
        } else if (format == "HH:mm:ss") {
            ss << std::put_time(std::localtime(&time), "%H:%M:%S");
        } else {
            ss << std::put_time(std::localtime(&time), "%c");
        }
        
        auto end = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<
            std::chrono::milliseconds>(end - start).count();
        
        return {
            .success = true,
            .data = {
                {"time", ss.str()},
                {"timezone", timezone},
                {"timestamp", time}
            },
            .error_message = "",
            .execution_time_ms = elapsed
        };
    }
};
```

---

## 四、工具管理器

### 4.1 工具管理器设计

```cpp
class ToolManager {
public:
    // 注册工具
    void register_tool(std::shared_ptr<ITool> tool);
    
    // 获取工具
    std::shared_ptr<ITool> get_tool(const std::string& name) const;
    
    // 获取所有工具的 Schema（给 LLM）
    json get_all_schemas() const;
    
    // 执行工具
    ToolResult execute(
        const std::string& tool_name,
        const json& arguments,
        const ToolContext& context
    );
    
    // 列出所有工具
    std::vector<std::string> list_tools() const;

private:
    std::map<std::string, std::shared_ptr<ITool>> tools_;
};
```

### 4.2 实现

```cpp
void ToolManager::register_tool(std::shared_ptr<ITool> tool) {
    if (!tool) {
        throw std::invalid_argument("tool cannot be null");
    }
    
    std::string name = tool->get_name();
    
    if (tools_.count(name)) {
        std::cerr << "Warning: Tool '" << name << "' already exists, overwriting\n";
    }
    
    tools_[name] = tool;
    std::cout << "Registered tool: " << name << "\n";
}

std::shared_ptr<ITool> ToolManager::get_tool(const std::string& name) const {
    auto it = tools_.find(name);
    return it != tools_.end() ? it->second : nullptr;
}

json ToolManager::get_all_schemas() const {
    json schemas = json::array();
    
    for (const auto& [name, tool] : tools_) {
        schemas.push_back(tool->get_schema());
    }
    
    return schemas;
}

ToolResult ToolManager::execute(const std::string& tool_name,
                                const json& arguments,
                                const ToolContext& context) {
    auto tool = get_tool(tool_name);
    
    if (!tool) {
        return {
            .success = false,
            .data = nullptr,
            .error_message = "Tool not found: " + tool_name,
            .execution_time_ms = 0
        };
    }
    
    // 验证参数
    if (!tool->validate_arguments(arguments)) {
        return {
            .success = false,
            .data = nullptr,
            .error_message = "Invalid arguments for tool: " + tool_name,
            .execution_time_ms = 0
        };
    }
    
    // 执行工具
    return tool->execute(arguments, context);
}
```

---

## 五、集成到 Agent

### 5.1 更新 Agent 类

```cpp
class Agent {
public:
    Agent(LLMClient& llm) : llm_(llm) {
        // 注册内置工具
        register_builtin_tools();
    }
    
    void register_tool(std::shared_ptr<ITool> tool) {
        tool_manager_.register_tool(tool);
    }
    
    void process(const std::string& user_input, 
                 const ToolContext& context,
                 std::function<void(const std::string&)> callback);

private:
    void register_builtin_tools() {
        tool_manager_.register_tool(std::make_shared<WeatherTool>());
        tool_manager_.register_tool(std::make_shared<CalculatorTool>());
        tool_manager_.register_tool(std::make_shared<TimeTool>());
    }
    
    std::string build_system_prompt() {
        std::string prompt = 
            "你是一个智能助手，可以使用以下工具来完成用户请求：\n\n";
        
        // 添加所有工具的 Schema
        json schemas = tool_manager_.get_all_schemas();
        prompt += schemas.dump(2);
        
        prompt += "\n\n当需要使用工具时，请严格按以下 JSON 格式回复：\n";
        prompt += "TOOL_CALL: {\"tool\": \"工具名\", \"arguments\": {...}}\n";
        prompt += "\n如果需要多个工具，请逐个调用。";
        
        return prompt;
    }
    
    LLMClient& llm_;
    ToolManager tool_manager_;
};
```

---

## 六、本章小结

**核心收获：**

1. **工具接口标准化**：
   - `ITool` 纯虚类定义
   - `ToolParameter`、`ToolResult` 统一结构
   - JSON Schema 自描述

2. **工具实现模式**：
   - 继承 `ITool`，实现四个纯虚函数
   - 参数校验 + 安全过滤
   - 执行时间统计

3. **工具管理**：
   - `ToolManager` 统一管理
   - Schema 聚合给 LLM
   - 生命周期管理

---

## 七、引出的问题

### 7.1 同步阻塞问题

目前的工具执行是同步的：

```cpp
ToolResult execute(...) {
    // 调用天气 API，阻塞等待响应
    auto data = http_get("https://api.weather.com/...");
    return {...};
}
```

**问题：** 如果 API 响应慢（几秒），整个 Agent 都被阻塞。

**需要：** 异步工具执行。

### 7.2 安全问题

当前工具可能存在安全隐患：

```cpp
// 危险：可能执行任意系统命令
system("curl " + user_input);

// 危险：SSRF 攻击
http_get(user_input);  // user_input = "http://localhost/admin"
```

**需要：** 安全沙箱、输入验证、访问控制。

### 7.3 工具依赖问题

复杂工具可能依赖其他工具：

```
工具：规划旅行
  ├── 依赖：查询天气
  ├── 依赖：查询航班
  └── 依赖：查询酒店
```

**需要：** 工具依赖注入、执行顺序管理。

---

**下一章预告（Step 7）：**

我们将实现**异步工具执行**：
- 异步 HTTP 调用
- 并发控制（同时执行多个工具）
- 超时机制
- 回调处理

工具接口已经标准化，接下来要让工具执行不阻塞 Agent。
