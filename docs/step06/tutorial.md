# Step 6: Tools 系统（上）- 同步工具、注册表、参数校验

> 目标：构建完整的工具系统框架，实现工具注册、参数校验和同步执行
> 
> 难度：⭐⭐⭐ (中等)
> 
> 代码量：约 480 行

## 本节收获

- 理解 Tool Schema 设计的核心思想
- 掌握参数类型系统和校验机制
- 实现工具注册表模式（Registry Pattern）
- 学会设计可扩展的工具基类
- 掌握工具安全的基本考虑

---

## 第一部分：为什么需要工具系统？

### 1.1 从硬编码到可扩展

**Step 3-5 的问题：** 工具调用是硬编码的

```cpp
// 糟糕的做法：每个工具都要改主代码
if (input.find("calculate") != string::npos) {
    result = calculate(args);
} else if (input.find("search") != string::npos) {
    result = search(args);
} 
// ... 每加一个工具都要改这里
```

**问题：**
- 违反开闭原则（对扩展开放，对修改关闭）
- 代码臃肿，难以维护
- LLM 无法自动了解有哪些工具可用

### 1.2 工具系统的核心思想

```
声明式定义 + 自动发现 + 统一调用

┌─────────────────────────────────────────────────────────────┐
│                     Tool Schema（声明）                      │
│  {                                                           │
│    "name": "calculator",                                     │
│    "description": "执行数学计算",                            │
│    "parameters": {                                           │
│      "expression": {"type": "string", "required": true}     │
│    }                                                         │
│  }                                                           │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                    ToolRegistry（注册表）                    │
│  name ──> Tool 实例                                          │
│  name ──> Schema 定义                                        │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                      LLM 使用工具                            │
│  1. 获取所有 Schema ──> 了解可用工具                         │
│  2. 决定调用哪个工具                                         │
│  3. 生成符合 Schema 的参数                                   │
│  4. 调用 execute() ──> 获得结果                              │
└─────────────────────────────────────────────────────────────┘
```

### 1.3 Tool Schema 的重要性

Schema 是工具的"自描述文档"，让 LLM 能够：

1. **发现工具**：获取所有可用工具列表
2. **理解用途**：通过 description 了解工具功能
3. **正确调用**：根据 parameters 生成正确的参数
4. **错误预防**：required 字段避免遗漏关键参数

**与 OpenAI Function Calling 对比：**

```json
// OpenAI 格式
{
  "type": "function",
  "function": {
    "name": "calculator",
    "parameters": {
      "type": "object",
      "properties": {...},
      "required": ["expression"]
    }
  }
}

// 我们的简化格式（更易理解和处理）
{
  "name": "calculator",
  "description": "执行数学计算",
  "parameters": [
    {"name": "expression", "type": "string", "required": true}
  ]
}
```

---

## 第二部分：核心组件详解

### 2.1 参数类型系统

```cpp
enum class ParamType {
    STRING,     // "hello"
    INTEGER,    // 42
    NUMBER,     // 3.14 (整数或浮点)
    BOOLEAN,    // true/false
    ARRAY,      // [1, 2, 3]
    OBJECT      // {"key": "value"}
};
```

**为什么用 enum class 而不是 enum？**

```cpp
// enum（C 风格）- 不推荐
enum ParamType { STRING, INTEGER };
ParamType t = STRING;
int x = t;  // 隐式转换为 int，危险！

// enum class（C++11）- 推荐  
enum class ParamType { STRING, INTEGER };
ParamType t = ParamType::STRING;  // 必须带作用域
// int x = t;  // 编译错误！类型安全
```

### 2.2 Parameter 结构体设计

```cpp
struct Parameter {
    string name;           // 参数名
    ParamType type;        // 参数类型
    string description;    // 描述（给 LLM 看的）
    bool required;         // 是否必填
    json::value default_value;  // 可选参数的默认值
};
```

**设计考虑：**

| 字段 | 用途 | 示例 |
|:---|:---|:---|
| name | 参数标识符 | "expression" |
| type | 类型校验 | ParamType::STRING |
| description | LLM 理解用途 | "数学表达式，如 1+2*3" |
| required | 强制校验 | true |
| default_value | 简化调用 | 2 (precision 默认 2 位) |

### 2.3 参数校验器实现

```cpp
class ParameterValidator {
public:
    // 类型校验
    static bool validate_type(const json::value& value, ParamType expected);
    
    // 完整校验
    static vector<string> validate(const json::object& args, 
                                   const vector<Parameter>& schema);
};
```

**校验流程：**

```
输入：用户提供的参数 { "expression": "1+1", "precision": 2 }
       
步骤 1: 检查必需参数
  - expression: required=true, 存在 ✓
  
步骤 2: 检查参数类型
  - expression: type=string, 实际是 string ✓
  - precision: type=integer, 实际是 integer ✓
  
步骤 3: 检查未知参数
  - 所有参数都在 schema 中 ✓
  
输出：无错误，校验通过
```

**错误示例：**

```json
// 用户输入
{"expression": 123, "unknown_param": "test"}

// 校验结果
[
  "Parameter 'expression' should be string, got number",
  "Unknown parameter: unknown_param"
]
```

---

## 第三部分：工具基类设计

### 3.1 抽象基类 Tool

```cpp
class Tool {
public:
    virtual ~Tool() = default;
    
    // 子类必须实现：返回 Schema 定义
    virtual ToolSchema get_schema() const = 0;
    
    // 子类必须实现：执行工具逻辑
    virtual json::value execute(const json::object& args) = 0;
    
    // 基类提供：带校验的执行
    json::value execute_safe(const json::object& args) {
        // 1. 校验参数
        auto errors = ParameterValidator::validate(args, get_schema().parameters);
        if (!errors.empty()) {
            return make_error_response(errors);
        }
        // 2. 执行工具
        return execute(args);
    }
};
```

**设计模式：模板方法模式（Template Method）**

```
execute_safe() [基类定义流程]
    ├── 校验参数 [固定步骤]
    ├── 执行工具 [子类实现]
    └── 返回结果 [固定步骤]
```

### 3.2 具体工具实现：CalculatorTool

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
        string expr = args.at("expression").as_string();
        double result = evaluate(expr);
        
        json::object response;
        response["success"] = true;
        response["result"] = result;
        return response;
    }
};
```

### 3.3 具体工具实现：FileReadTool（带安全检查）

```cpp
class FileReadTool : public Tool {
public:
    json::value execute(const json::object& args) override {
        string path = args.at("path").as_string();
        
        // 安全检查：防止路径遍历攻击
        if (path.find("..") != string::npos || path[0] == '/') {
            return error("Access denied: invalid path");
        }
        
        // 执行文件读取...
    }
};
```

**安全考虑：**
- **路径遍历**：禁止 `..` 和绝对路径
- **读取限制**：限制最大行数（防止读取大文件导致内存溢出）
- **错误处理**：不暴露文件系统细节

---

## 第四部分：工具注册表

### 4.1 注册表模式（Registry Pattern）

```cpp
class ToolRegistry {
private:
    map<string, shared_ptr<Tool>> tools_;      // name -> Tool
    map<string, ToolSchema> schemas_;          // name -> Schema
    
public:
    void register_tool(shared_ptr<Tool> tool) {
        auto schema = tool->get_schema();
        tools_[schema.name] = tool;
        schemas_[schema.name] = schema;
    }
    
    shared_ptr<Tool> get_tool(const string& name);
    json::array get_all_schemas();
    json::value execute(const string& name, const json::object& args);
};
```

**优势：**
- **解耦**：工具与调用者解耦
- **动态**：运行时注册/发现工具
- **集中**：统一管理所有工具

### 4.2 使用示例

```cpp
// 1. 创建注册表
ToolRegistry registry;

// 2. 注册工具
registry.register_tool(make_shared<CalculatorTool>());
registry.register_tool(make_shared<FileReadTool>());

// 3. 获取所有 Schema（给 LLM 看）
auto schemas = registry.get_all_schemas();

// 4. 执行工具
auto result = registry.execute("calculator", 
    json::object{{"expression", "1+1"}});
```

---

## 第五部分：完整运行测试

### 5.1 编译运行

```bash
cd src/step06
mkdir build && cd build
cmake .. && make
./nuclaw_step06
```

### 5.2 测试工具调用

```bash
wscat -c ws://localhost:8081

# 计算器
> {"tool": "calculator", "args": {"expression": "3.14159"}}
< {"success": true, "result": 3.14}

# 参数错误测试
> {"tool": "calculator", "args": {}}
< {"success": false, "error": "Validation failed", "details": [...]}

# 系统信息
> {"tool": "system_info", "args": {"type": "time"}}
< {"success": true, "time": "2024-01-15 10:30:00"}
```

### 5.3 测试文件读取安全

```bash
# 正常访问
> {"tool": "file_read", "args": {"path": "test.txt"}}

# 路径遍历攻击（应被拒绝）
> {"tool": "file_read", "args": {"path": "../../etc/passwd"}}
< {"success": false, "error": "Access denied: invalid path"}
```

---

## 第六部分：与 Step 3-5 的对比

| 特性 | Step 3-5 (基础) | Step 6 (工具系统) |
|:---|:---|:---|
| 工具定义 | 硬编码 if/else | Schema 声明式定义 |
| 参数校验 | 无 | 完整类型校验 |
| 扩展性 | 修改主代码 | 注册新工具即可 |
| LLM 发现 | 无法自动发现 | 通过 Schema 自动发现 |
| 安全性 | 无 | 参数校验 + 安全检查 |

---

## 第七部分：常见问题

### Q: 为什么要用 shared_ptr 管理 Tool？

A: 
1. 工具可能在多个地方被引用（注册表、执行队列）
2. 生命周期自动管理，避免内存泄漏
3. 支持多态（基类指针指向派生类对象）

### Q: Schema 中的 description 有什么用？

A: 
description 是给 LLM 看的"使用说明"。例如：
```cpp
{"name": "file_read", "description": "读取文件内容（限制访问范围）"}
```
LLM 看到后会理解：
- 这个工具可以读取文件
- 有访问限制（不能随意访问任何文件）

### Q: 如何添加新工具？

A: 三步：
1. 继承 Tool 基类
2. 实现 get_schema() 和 execute()
3. 在 AgentLoop 构造函数中注册

无需修改任何现有代码！

---

## 下一步

→ **Step 7: Tools 系统（中）- 异步工具执行**

我们将解决 Step 6 同步工具的问题：
- 同步阻塞问题
- 无法并发执行
- 缺少超时控制

学习如何使用异步回调模式实现非阻塞工具执行。
