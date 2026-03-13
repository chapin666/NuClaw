# Step 9: 工具注册表 - 解决硬编码问题

> 目标：掌握注册表模式，实现可扩展的工具系统
> 
> 难度：⭐⭐⭐⭐ (较难)
> 
> 代码量：约 750 行
> 
> 预计学习时间：4-5 小时

---

## 📚 前置知识

### 软件设计原则回顾

#### 开闭原则（Open/Closed Principle）

**定义**：软件实体应该对扩展开放，对修改关闭。

**通俗理解：**
- ✅ **扩展开放**：可以增加新功能
- ❌ **修改关闭**：不需要改动已有代码

**现实类比：**
```
电源插座：
- 对扩展开放：可以插各种电器（手机充电器、台灯、电脑）
- 对修改关闭：不需要改动插座内部结构

VS

固定接线：
- 每加一个电器要重新接线（违反开闭原则）
```

#### 依赖倒置原则（Dependency Inversion Principle）

**定义**：高层模块不应该依赖低层模块，两者都应该依赖抽象。

**通俗理解：**
- 依赖接口，不依赖具体实现
- 这样替换实现时不需要改动调用方

**现实类比：**
```
USB 接口：
- 电脑（高层）依赖 USB 接口（抽象）
- U盘、鼠标、键盘（低层）都实现 USB 接口
- 换鼠标不需要改电脑代码
```

### Step 6-8 的问题分析

**硬编码分发的痛点：**

```cpp
// 每加一个工具都要修改这里！
if (call.name == "get_weather") return WeatherTool::execute(...);
else if (call.name == "get_time") return TimeTool::execute(...);
else if (call.name == "calculate") return CalcTool::execute(...);
// ... 无限增长的分支
```

**产生的问题：**

| 问题 | 说明 | 后果 |
|:---|:---|:---|
| **代码膨胀** | 工具越多，if-else 链越长 | 难以维护 |
| **编译依赖** | 执行器需要包含所有工具头文件 | 编译变慢 |
| **测试困难** | 无法单独 mock 某个工具 | 单元测试复杂 |
| **无法扩展** | 第三方无法添加工具 | 生态封闭 |
| **违反 OCP** | 每加工具改核心代码 | 设计缺陷 |

### 注册表模式简介

**核心思想：**
```
传统方式（硬编码）：
  执行器 ──知道──> 工具A
     ├──知道──> 工具B
     ├──知道──> 工具C
     └──...（耦合所有工具）

注册表方式（解耦）：
  工具A ──注册──> 注册表 <──查询── 执行器
  工具B ──注册──> 注册表     （只依赖注册表）
  工具C ──注册──> 注册表
```

**组件职责：**

| 组件 | 职责 | 知道谁 |
|:---|:---|:---|
| **工具（Tool）** | 实现具体功能 | 只知道自己 |
| **注册表（Registry）** | 管理工具注册与查找 | 知道所有工具 |
| **执行器（Executor）** | 调用工具执行 | 只知道注册表 |

---

## 第一步：设计工具接口

### 为什么要抽象接口？

**接口的作用：**
1. **定义契约**：规定工具必须实现的方法
2. **实现多态**：统一处理不同类型的工具
3. **解耦合**：调用方只依赖接口，不依赖实现

### 工具接口设计

```cpp
// tool.hpp - 工具抽象基类
#pragma once
#include <string>
#include <boost/json.hpp>

namespace json = boost::json;

// 工具执行结果
struct ToolResult {
    bool success;
    std::string data;      // JSON 格式
    std::string error;
    
    static ToolResult ok(const std::string& json_data) {
        return {true, json_data, ""};
    }
    
    static ToolResult fail(const std::string& err) {
        return {false, "", err};
    }
};

// 工具调用请求
struct ToolCall {
    std::string name;
    std::string arguments;
};

// 工具抽象基类
class Tool {
public:
    virtual ~Tool() = default;
    
    // 获取工具名称（唯一标识）
    virtual std::string get_name() const = 0;
    
    // 获取工具描述（给 LLM 看的）
    virtual std::string get_description() const = 0;
    
    // 执行工具
    virtual ToolResult execute(const std::string& arguments) const = 0;
};
```

### 具体工具实现示例

```cpp
// weather_tool.hpp
#pragma once
#include "tool.hpp"
#include <map>

class WeatherTool : public Tool {
public:
    std::string get_name() const override { return "get_weather"; }
    
    std::string get_description() const override {
        return "查询指定城市的天气信息，支持北京、上海、深圳、广州";
    }
    
    ToolResult execute(const std::string& city) const override {
        // 模拟天气数据
        static const std::map<std::string, json::object> weather_data = {
            {"北京", {{"city", "北京"}, {"weather", "晴天"}, {"temp", 25}}},
            {"上海", {{"city", "上海"}, {"weather", "多云"}, {"temp", 22}}},
            {"深圳", {{"city", "深圳"}, {"weather", "小雨"}, {"temp", 28}}},
            {"广州", {{"city", "广州"}, {"weather", "阴天"}, {"temp", 26}}}
        };
        
        std::string city_clean = city;
        // 去除空白字符
        city_clean.erase(0, city_clean.find_first_not_of(" \t\n\r"));
        city_clean.erase(city_clean.find_last_not_of(" \t\n\r") + 1);
        
        auto it = weather_data.find(city_clean);
        if (it != weather_data.end()) {
            return ToolResult::ok(json::serialize(it->second));
        }
        
        // 默认返回北京天气
        return ToolResult::ok(json::serialize(weather_data.at("北京")));
    }
};
```

其他工具实现：

```cpp
// time_tool.hpp
class TimeTool : public Tool {
public:
    std::string get_name() const override { return "get_time"; }
    
    std::string get_description() const override {
        return "获取当前系统时间";
    }
    
    ToolResult execute(const std::string& /*arguments*/) const override {
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        
        std::stringstream ss;
        ss << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S");
        
        json::object data;
        data["datetime"] = ss.str();
        data["timezone"] = "本地时间";
        
        return ToolResult::ok(json::serialize(data));
    }
};

// calc_tool.hpp
class CalcTool : public Tool {
public:
    std::string get_name() const override { return "calculate"; }
    
    std::string get_description() const override {
        return "数学计算，支持 + - * /，如：1+2、10*5";
    }
    
    ToolResult execute(const std::string& expression) const override {
        try {
            double result = evaluate(expression);
            
            json::object data;
            data["expression"] = expression;
            data["result"] = result;
            
            return ToolResult::ok(json::serialize(data));
        } catch (const std::exception& e) {
            return ToolResult::fail(std::string("计算错误: ") + e.what());
        }
    }

private:
    double evaluate(const std::string& expr) const {
        // 简化实现：解析 "a op b" 格式
        std::istringstream iss(expr);
        double a, b;
        char op;
        
        iss >> a >> op >> b;
        
        switch (op) {
            case '+': return a + b;
            case '-': return a - b;
            case '*': return a * b;
            case '/': 
                if (b == 0) throw std::runtime_error("除数不能为零");
                return a / b;
            default: throw std::runtime_error("不支持的操作符");
        }
    }
};
```

---

## 第二步：实现注册表

### 单例模式

**为什么用单例？**
- 整个系统只需要一个注册表
- 全局可访问，方便工具注册和执行器查询

**实现要点：**
- 私有构造函数（防止外部创建）
- 静态方法获取实例
- 删除拷贝构造函数和赋值操作符

### 注册表完整实现

```cpp
// tool_registry.hpp
#pragma once

#include "tool.hpp"
#include <memory>
#include <unordered_map>
#include <mutex>
#include <iostream>

class ToolRegistry {
public:
    // 获取单例实例（线程安全）
    static ToolRegistry& instance() {
        static ToolRegistry instance;
        return instance;
    }
    
    // 删除拷贝和赋值
    ToolRegistry(const ToolRegistry&) = delete;
    ToolRegistry& operator=(const ToolRegistry&) = delete;
    
    // 注册工具
    void register_tool(std::shared_ptr<Tool> tool) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string name = tool->get_name();
        tools_[name] = tool;
        std::cout << "[+] 注册工具: " << name << std::endl;
    }
    
    // 获取工具
    std::shared_ptr<Tool> get_tool(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = tools_.find(name);
        if (it != tools_.end()) {
            return it->second;
        }
        return nullptr;
    }
    
    // 检查工具是否存在
    bool has_tool(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return tools_.find(name) != tools_.end();
    }
    
    // 获取所有工具名称
    std::vector<std::string> get_tool_names() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> names;
        for (const auto& [name, _] : tools_) {
            names.push_back(name);
        }
        return names;
    }
    
    // 生成工具描述（给 LLM 使用）
    std::string get_tools_description() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string desc = "可用工具列表：\n\n";
        
        for (const auto& [name, tool] : tools_) {
            desc += "- " + name + ": " + tool->get_description() + "\n";
        }
        
        return desc;
    }
    
    // 获取已注册工具数量
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return tools_.size();
    }

private:
    ToolRegistry() = default;
    ~ToolRegistry() = default;
    
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<Tool>> tools_;
};
```

---

## 第三步：改造工具执行器

### 改造前后对比

**改造前（硬编码）：**
```cpp
// ❌ 坏代码：每加一个工具都要改这里
class ToolExecutor {
public:
    static ToolResult execute(const ToolCall& call) {
        if (call.name == "get_weather") 
            return WeatherTool::execute(call.arguments);
        else if (call.name == "get_time") 
            return TimeTool::execute(call.arguments);
        else if (call.name == "calculate") 
            return CalcTool::execute(call.arguments);
        // ... 每加一个工具都要添加新的 if-else
        
        return ToolResult::fail("未知工具: " + call.name);
    }
};
```

**改造后（使用注册表）：**
```cpp
// ✅ 好代码：固定不变，不随工具数量增长
class ToolExecutor {
public:
    // 同步执行
    static ToolResult execute_sync(const ToolCall& call) {
        std::cout << "[⚙️ 执行工具] " << call.name << std::endl;
        
        // 从注册表获取工具
        auto tool = ToolRegistry::instance().get_tool(call.name);
        
        if (!tool) {
            return ToolResult::fail(
                "未知工具: " + call.name + "\n" +
                "可用工具: " + get_available_tools()
            );
        }
        
        // 通过虚函数调用，多态执行
        return tool->execute(call.arguments);
    }
    
    // 获取可用工具列表
    static std::string get_available_tools() {
        auto names = ToolRegistry::instance().get_tool_names();
        std::string result;
        for (size_t i = 0; i < names.size(); ++i) {
            if (i > 0) result += ", ";
            result += names[i];
        }
        return result;
    }
    
    // 检查工具是否存在
    static bool has_tool(const std::string& name) {
        return ToolRegistry::instance().has_tool(name);
    }
};
```

**优势：**
- 代码固定，不随工具数量增长
- 不依赖具体工具头文件
- 第三方工具也可以被调用

---

## 第四步：集成到 Agent 核心

### ChatEngine 实现

```cpp
// chat_engine.hpp
#pragma once

#include "llm_client.hpp"
#include "tool_executor.hpp"
#include "tool_registry.hpp"
#include <vector>
#include <utility>

struct ChatContext {
    std::vector<std::pair<std::string, std::string>> history;
    int message_count = 0;
};

class ChatEngine {
public:
    ChatEngine() = default;
    
    // 处理用户输入
    std::string process(const std::string& user_input, ChatContext& ctx) {
        ctx.message_count++;
        std::cout << "[🧠 处理] \"" << user_input << "\"" << std::endl;
        
        // 判断是否需要工具
        if (llm_.needs_tool(user_input)) {
            std::cout << "  → 需要工具" << std::endl;
            
            ToolCall call = llm_.parse_tool_call(user_input);
            std::cout << "  → 工具: " << call.name << std::endl;
            
            // 通过注册表执行工具
            ToolResult result = ToolExecutor::execute_sync(call);
            std::cout << "  → 结果: " << (result.success ? "成功" : "失败") << std::endl;
            
            std::string reply = llm_.generate_response(user_input, result, call);
            
            ctx.history.push_back({"user", user_input});
            ctx.history.push_back({"assistant", reply});
            
            return reply;
        }
        else {
            std::cout << "  → 直接回复" << std::endl;
            
            std::string reply = llm_.direct_reply(user_input);
            
            ctx.history.push_back({"user", user_input});
            ctx.history.push_back({"assistant", reply});
            
            return reply;
        }
    }
    
    std::string get_welcome_message() const {
        return "👋 欢迎来到 NuClaw Step 9！\n\n"
               "📝 本章核心：工具注册表模式\n"
               "- 使用注册表管理工具，不再硬编码\n"
               "- 支持动态注册和扩展\n"
               "- 符合开闭原则\n\n"
               "可用命令：\n"
               "- 天气查询：北京天气如何？\n"
               "- 时间查询：现在几点？\n"
               "- 数学计算：25 * 4 等于多少？\n"
               "- 查看工具：有哪些工具？";
    }

private:
    LLMClient llm_;
};
```

### 主程序

```cpp
// main.cpp
#include "tool_registry.hpp"
#include "weather_tool.hpp"
#include "time_tool.hpp"
#include "calc_tool.hpp"
#include "chat_engine.hpp"
#include <iostream>

// 初始化所有工具
void init_tools() {
    auto& registry = ToolRegistry::instance();
    
    // 注册工具 - 新增工具只需在这里添加一行！
    registry.register_tool(std::make_shared<WeatherTool>());
    registry.register_tool(std::make_shared<TimeTool>());
    registry.register_tool(std::make_shared<CalcTool>());
    
    std::cout << "[✓] 已注册 " << registry.size() << " 个工具" << std::endl;
}

int main() {
    std::cout << "========================================\n";
    std::cout << "   NuClaw Step 9: 工具注册表模式\n";
    std::cout << "========================================\n\n";
    
    // 1. 初始化工具
    init_tools();
    
    // 2. 显示工具列表
    std::cout << ToolRegistry::instance().get_tools_description() << std::endl;
    
    // 3. 创建 ChatEngine
    ChatEngine engine;
    ChatContext ctx;
    
    std::cout << "\n" << engine.get_welcome_message() << "\n\n";
    
    // 4. 演示交互
    std::vector<std::string> test_inputs = {
        "你好",
        "北京天气如何？",
        "现在几点？",
        "25 * 4 等于多少？",
        "有哪些工具？"
    };
    
    for (const auto& input : test_inputs) {
        std::cout << "\n----------------------------------------\n";
        std::cout << "👤 用户: " << input << std::endl;
        std::string reply = engine.process(input, ctx);
        std::cout << "🤖 AI: " << reply << std::endl;
    }
    
    std::cout << "\n========================================\n";
    std::cout << "演示完成！\n";
    std::cout << "========================================\n";
    
    return 0;
}
```

---

## 第五步：设计模式总结

### 本节涉及的模式

| 模式 | 用途 | 体现位置 |
|:---|:---|:---|
| **注册表模式** | 管理工具注册与查找 | ToolRegistry |
| **单例模式** | 全局唯一注册表 | instance() 方法 |
| **工厂模式** | 创建工具实例 | make_shared |
| **策略模式** | 运行时选择工具 | execute() 多态 |

### 软件设计原则体现

```
开闭原则：✅ 新增工具不改执行器代码
依赖倒置：✅ 执行器依赖 Tool 接口，不依赖具体实现
单一职责：✅ 注册表只负责管理，执行器只负责执行
接口隔离：✅ Tool 接口精简专注
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
        // 解析参数（简化处理）
        // 实际应该解析 JSON
        
        // 模拟翻译结果
        json::object result;
        result["original"] = args;
        result["translated"] = "[翻译结果] " + args;
        
        return ToolResult::ok(json::serialize(result));
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

## 📖 扩展阅读

### 设计模式
- **注册表模式**：中央管理对象注册与查找
- **依赖注入**：控制反转（IoC）的实现方式
- **插件架构**：动态扩展系统功能

### 实际案例
- **Chrome 扩展系统**：插件注册与权限管理
- **VS Code**：Extension API 设计
- **Kubernetes**：Operator 模式（CRD + Controller）

---

**下一步：** Step 10 将让 Agent 具备知识检索能力（RAG）
