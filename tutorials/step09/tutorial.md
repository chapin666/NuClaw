# Step 9: 工具注册表 - 解决硬编码问题

> 目标：掌握注册表模式，实现插件化工具系统
> 
> 难度：⭐⭐⭐⭐ (较难)
> 
> 代码量：约 750 行
> 
> 预计学习时间：4-5 小时

---

## 🎯 Agent 开发知识点

**本节核心问题：** 如何让 Agent 的工具系统支持动态扩展？

**Agent 架构中的位置：**
```
用户输入 → Agent Core → 工具注册表 → 具体工具
                              ↑
                         动态管理工具生命周期
```

**关键能力：**
- 运行时注册/注销工具
- 第三方工具插件化
- 工具版本管理

---

## 📚 理论基础 + 代码实现

### 1. 开闭原则与工具系统

**问题分析（理论）：**

硬编码分发违反开闭原则：
```cpp
// ❌ 坏代码：每加一个工具都要改这里
class ToolExecutor {
    ToolResult execute(const ToolCall& call) {
        if (call.name == "weather") return WeatherTool::execute(...);
        else if (call.name == "time") return TimeTool::execute(...);
        else if (call.name == "calc") return CalcTool::execute(...);
        // ... 无限增长
    }
};
```

**Agent 开发影响：**
- 无法热插拔工具
- 第三方无法扩展
- 测试困难（必须加载所有工具）

**解决方案（代码实现）：**

```cpp
// tool.hpp - 抽象接口
class Tool {
public:
    virtual ~Tool() = default;
    virtual std::string get_name() const = 0;
    virtual std::string get_description() const = 0;
    virtual ToolResult execute(const std::string& args) const = 0;
};

// tool_registry.hpp - 注册表实现
class ToolRegistry {
public:
    static ToolRegistry& instance() {
        static ToolRegistry instance;
        return instance;
    }
    
    void register_tool(std::shared_ptr<Tool> tool) {
        std::lock_guard<std::mutex> lock(mutex_);
        tools_[tool->get_name()] = tool;
    }
    
    std::shared_ptr<Tool> get_tool(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = tools_.find(name);
        return (it != tools_.end()) ? it->second : nullptr;
    }

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<Tool>> tools_;
};
```

**改造后的执行器：**
```cpp
// ✅ 好代码：固定不变，不随工具数量增长
class ToolExecutor {
public:
    static ToolResult execute_sync(const ToolCall& call) {
        auto tool = ToolRegistry::instance().get_tool(call.name);
        if (!tool) {
            return ToolResult::fail("未知工具: " + call.name);
        }
        return tool->execute(call.arguments);  // 多态调用
    }
};
```

### 2. 具体工具实现

**理论：** 每个工具都是独立插件，遵循统一契约

```cpp
// weather_tool.hpp - 天气工具
class WeatherTool : public Tool {
public:
    std::string get_name() const override { 
        return "get_weather"; 
    }
    
    std::string get_description() const override {
        return "查询指定城市天气，支持北京、上海等";
    }
    
    ToolResult execute(const std::string& city) const override {
        // 实际实现：调用天气 API
        json::object data;
        data["city"] = city;
        data["temp"] = 25;
        data["weather"] = "晴天";
        return ToolResult::ok(json::serialize(data));
    }
};

// calc_tool.hpp - 计算工具
class CalcTool : public Tool {
public:
    std::string get_name() const override { 
        return "calculate"; 
    }
    
    ToolResult execute(const std::string& expr) const override {
        // 解析并计算表达式
        double result = evaluate(expr);
        return ToolResult::ok("{\"result\": " + std::to_string(result) + "}");
    }

private:
    double evaluate(const std::string& expr) const {
        // 简化实现
        return 42.0;
    }
};
```

### 3. Agent 核心集成

```cpp
// chat_engine.hpp
class ChatEngine {
public:
    std::string process(const std::string& input, ChatContext& ctx) {
        // 1. 判断是否需要工具
        if (llm_.needs_tool(input)) {
            ToolCall call = llm_.parse_tool_call(input);
            
            // 2. 通过注册表执行（解耦！）
            ToolResult result = ToolExecutor::execute_sync(call);
            
            // 3. 生成回复
            return llm_.generate_response(input, result, call);
        }
        
        return llm_.direct_reply(input);
    }
};

// main.cpp - 初始化
void init_tools() {
    auto& registry = ToolRegistry::instance();
    
    // 注册内置工具
    registry.register_tool(std::make_shared<WeatherTool>());
    registry.register_tool(std::make_shared<TimeTool>());
    registry.register_tool(std::make_shared<CalcTool>());
    
    // 第三方工具也可以在这里注册！
    // registry.register_tool(std::make_shared<ThirdPartyTool>());
}
```

---

## 🔧 实战练习

### 练习 1：添加翻译工具

**要求：** 实现一个翻译工具，支持中英互译

**Agent 开发要点：**
- 工具名称：`translate`
- 参数：`text`（原文）、`target_lang`（目标语言）
- 返回值：JSON 格式 `{"original": "...", "translated": "..."}`

**参考实现：**
```cpp
class TranslateTool : public Tool {
public:
    std::string get_name() const override { return "translate"; }
    
    std::string get_description() const override {
        return "翻译文本，参数：{\"text\": \"要翻译的内容\", \"to\": \"en/zh\"}";
    }
    
    ToolResult execute(const std::string& args) const override {
        // 解析参数
        // 调用翻译 API 或本地词典
        // 返回结果
    }
};
```

### 练习 2：工具版本管理

**要求：** 扩展注册表支持工具版本

**Agent 开发场景：**
- 新旧版本工具共存
- 根据配置选择版本

```cpp
struct ToolVersion {
    std::string name;
    std::string version;  // "1.0.0"
    std::shared_ptr<Tool> tool;
};

class VersionedRegistry {
public:
    void register_tool(std::shared_ptr<Tool> tool, 
                       const std::string& version);
    
    std::shared_ptr<Tool> get_tool(const std::string& name,
                                      const std::string& version = "latest");
};
```

---

## 📋 Agent 开发检查清单

- [ ] 工具接口设计是否稳定？
- [ ] 注册表是否线程安全？
- [ ] 工具描述是否清晰（给 LLM 看）？
- [ ] 是否支持第三方工具注册？
- [ ] 错误处理是否完善（工具不存在时）？

---

## 📝 课后思考题

1. **设计决策**：为什么用 `shared_ptr<Tool>` 而不是原始指针？
2. **性能考量**：注册表查询是 O(1) 还是 O(n)？如何优化？
3. **扩展性**：如何实现运行时卸载工具？
4. **安全性**：如何防止恶意工具注册？

---

**下一步：** Step 10 将让 Agent 具备知识检索能力（RAG）
