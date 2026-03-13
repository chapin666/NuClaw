# Step 6: Tools 系统（上）- 同步工具、注册表、参数校验

> 目标：构建完整的工具系统框架，实现工具注册、参数校验和同步执行
> 
> 难度：⭐⭐⭐ (中等)
> 
003e 代码量：约 480 行

## 本节收获

- 理解 Tool Schema 设计
- 掌握参数类型系统和校验
- 实现工具注册表模式
- 理解同步工具执行的优缺点

---

## 第一部分：为什么需要工具系统？

### 1.1 从硬编码到可扩展

**Step 3-5 的问题：**
```cpp
// 硬编码的工具调用
if (input.find("/calc") != std::string::npos) {
    // 执行计算器
} else if (input.find("/time") != std::string::npos) {
    // 执行时间查询
}
// 每加一个工具都要改代码！
```

**工具系统的目标：**
```cpp
// 声明式注册
registry.register_tool(std::make_shared<CalculatorTool>());
registry.register_tool(std::make_shared<WeatherTool>());
// ... 无限扩展，无需修改核心代码
```

### 1.2 工具系统的核心功能

```
┌─────────────────────────────────────────────────────────────┐
│                     工具系统架构                             │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│   ┌──────────────┐     ┌──────────────┐     ┌──────────┐   │
│   │   Schema     │     │   Registry   │     │ Executor │   │
│   │   (定义)      │────▶│   (注册表)    │────▶│ (执行器) │   │
│   └──────────────┘     └──────────────┘     └──────────┘   │
│          │                    │                    │        │
│          │                    │                    │        │
│          ▼                    ▼                    ▼        │
│   ┌────────────────────────────────────────────────────┐   │
│   │                     Tools                         │   │
│   │  ┌──────────┐  ┌──────────┐  ┌──────────┐        │   │
│   │  │Calculator│  │FileReader│  │ HTTP API │  ...   │   │
│   │  └──────────┘  └──────────┘  └──────────┘        │   │
│   └────────────────────────────────────────────────────┘   │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

**Schema**：工具的定义（名称、参数、描述）
**Registry**：工具的注册和查找
**Executor**：工具的调用和生命周期管理

---

## 第二部分：Tool Schema 设计

### 2.1 Schema 的重要性

Schema 是工具的自描述，让 LLM 知道：
- 有哪些工具可用
- 每个工具是做什么的
- 需要什么参数

```json
{
    "name": "calculator",
    "description": "执行数学计算",
    "parameters": {
        "expression": {
            "type": "string",
            "description": "数学表达式，如 1+2*3",
            "required": true
        },
        "precision": {
            "type": "integer",
            "description": "结果精度",
            "required": false,
            "default": 2
        }
    }
}
```

### 2.2 参数类型系统

```cpp
enum class ParamType {
    STRING,     // 字符串
    INTEGER,    // 整数
    NUMBER,     // 数字（整数或浮点）
    BOOLEAN,    // 布尔值
    ARRAY,      // 数组
    OBJECT      // 对象
};

struct Parameter {
    std::string name;
    ParamType type;
    std::string description;
    bool required;
    json::value default_value;  // 可选参数的默认值
};
```

### 2.3 与 OpenAI Function Calling 对比

OpenAI 的函数定义格式：
```json
{
    "type": "function",
    "function": {
        "name": "calculator",
        "description": "执行数学计算",
        "parameters": {
            "type": "object",
            "properties": {
                "expression": {"type": "string"},
                "precision": {"type": "integer"}
            },
            "required": ["expression"]
        }
    }
}
```

我们的简化版本更易于理解和处理。

---

## 第三部分：参数校验

### 3.1 为什么需要参数校验？

**问题场景：**
```
用户输入: {\"tool\": \"calculator\", \"args\": {\"expr\": \"1+1\"}}
                ↑
        参数名错误！应该是 \"expression\"

没有校验: 工具崩溃或返回错误结果
有校验: 返回清晰的错误信息
```

### 3.2 校验规则

```cpp
class ParameterValidator {
public:
    static std::vector<std::string> validate(
        const json::object& args,
        const std::vector<Parameter>& schema) {
        
        std::vector<std::string> errors;
        
        // 1. 检查必需参数
        for (const auto& param : schema) {
            if (param.required && !args.contains(param.name)) {
                errors.push_back("Missing: " + param.name);
            }
        }
        
        // 2. 检查参数类型
        for (const auto& [key, value] : args) {
            auto param = find_param(schema, key);
            if (!validate_type(value, param.type)) {
                errors.push_back("Type mismatch: " + key);
            }
        }
        
        // 3. 检查未知参数
        for (const auto& [key, _] : args) {
            if (!has_param(schema, key)) {
                errors.push_back("Unknown: " + key);
            }
        }
        
        return errors;
    }
};
```

### 3.3 错误处理策略

| 策略 | 描述 | 适用场景 |
|:---|:---|:---|
| 严格模式 | 任何错误都拒绝执行 | 生产环境 |
| 宽松模式 | 忽略未知参数 | 快速原型 |
| 默认值 | 缺失参数使用默认值 | 可选参数多 |

---

## 第四部分：工具注册表

### 4.1 注册表模式

注册表是设计模式中常见的**服务定位器**模式：

```cpp
class ToolRegistry {
public:
    // 注册工具
    void register_tool(std::shared_ptr<Tool> tool);
    
    // 按名称获取工具
    std::shared_ptr<Tool> get_tool(const std::string& name);
    
    // 列出所有工具
    std::vector<std::string> list_tools();
    
    // 获取所有 Schema（给 LLM 看）
    json::array get_all_schemas();
};
```

### 4.2 注册 vs 发现

**静态注册（本节实现）：**
```cpp
// 启动时注册
registry.register_tool(std::make_shared<CalculatorTool>());
registry.register_tool(std::make_shared<FileReadTool>());
```

**动态发现（高级）：**
```cpp
// 从插件加载
for (const auto& plugin : load_plugins("tools/")) {
    registry.register_tool(plugin.create_tool());
}
```

### 4.3 工具生命周期

```
创建 ──▶ 注册 ──▶ 发现 ──▶ 执行 ──▶ （可选）销毁
         ↑                     │
         └─────────────────────┘
              可重复执行
```

---

## 第五部分：同步执行

### 5.1 同步 vs 异步工具

**同步工具：**
```cpp
json::value execute(const json::object& args) {
    // 立即执行，阻塞等待结果
    auto result = do_something(args);
    return result;
}
```

优点：
- 实现简单
- 容易调试
- 无竞态条件

缺点：
- 阻塞调用线程
- 无法并发执行多个工具
- 长时间操作会卡住

**适用场景：**
- 计算密集型（数学运算）
- 本地文件操作
- 内存操作

### 5.2 工具基类设计

```cpp
class Tool {
public:
    virtual ~Tool() = default;
    
    // 子类必须实现
    virtual ToolSchema get_schema() const = 0;
    virtual json::value execute(const json::object& args) = 0;
    
    // 基类提供带校验的执行
    json::value execute_safe(const json::object& args) {
        auto errors = validate(args);
        if (!errors.empty()) {
            return make_error(errors);
        }
        return execute(args);
    }
};
```

### 5.3 具体工具示例

**计算器工具：**
```cpp
class CalculatorTool : public Tool {
public:
    ToolSchema get_schema() const override {
        return {
            "calculator",
            "执行数学计算",
            {
                {"expression", ParamType::STRING, "数学表达式", true},
                {"precision", ParamType::INTEGER, "精度", false, 2}
            }
        };
    }
    
    json::value execute(const json::object& args) override {
        std::string expr = args.at("expression").as_string();
        double result = evaluate(expr);
        return make_result(result);
    }
};
```

---

## 第六部分：运行测试

### 6.1 编译运行

```bash
cd src/step06
mkdir build && cd build
cmake .. && make
./nuclaw_step06
```

### 6.2 测试工具调用

```bash
wscat -c ws://localhost:8081

# 计算器
> {\"tool\": \"calculator\", \"args\": {\"expression\": \"3.14159\"}}

# 系统信息
> {\"tool\": \"system_info\", \"args\": {\"type\": \"time\"}}

# 参数错误测试
> {\"tool\": \"calculator\", \"args\": {}}
# 应该返回参数缺失错误
```

---

## 下一步

→ **Step 7: Tools 系统（中）- 异步工具**

我们将解决同步工具的问题：
- 长时间操作阻塞
- 无法并发执行
- 没有超时控制

学习如何使用 Asio 实现异步工具执行。
