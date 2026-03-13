# Step 9: 工具注册表 - 解决硬编码问题

> 目标：掌握注册表模式，实现插件化工具系统
> 
> 难度：⭐⭐⭐⭐ (较难)
> 
> 代码量：约 750 行
> 
> 预计学习时间：4-5 小时

---

## 📚 前置知识

### 回顾 Step 6-8 的问题

**Step 6-8 的硬编码问题：**

```cpp
// tool_executor.hpp - 硬编码分发
static ToolResult execute_tool(const ToolCall& call) {
    if (call.name == "get_weather") 
        return WeatherTool::execute(call.args);
    else if (call.name == "get_time") 
        return TimeTool::execute(call.args);
    else if (call.name == "calculate") 
        return CalcTool::execute(call.args);
    else if (call.name == "http_get") 
        return HttpTool::execute(call.args);
    // ... 每加一个工具都要修改这里！
    
    return ToolResult::fail("未知工具");
}
```

**问题分析：**
1. **违反开闭原则**：对扩展不开放，每加工具要改核心代码
2. **耦合严重**：执行器依赖所有工具的具体实现
3. **难以测试**：无法 mock 工具进行单元测试
4. **无法动态加载**：不能运行时加载新工具

### 设计模式：注册表模式（Registry Pattern）

**核心思想：**
- 工具自己注册到中央注册表
- 执行器只依赖注册表，不依赖具体工具
- 新增工具只需注册，无需修改执行器

**类比：**
```
硬编码方式：
  餐厅菜单印在墙上，每加菜要重新装修

注册表方式：
  餐厅用可更换的菜单板，加菜只需写在新卡片上插入
```

### 依赖注入（Dependency Injection）

**核心思想：**
- 不直接创建依赖，而是由外部注入
- 提高代码的可测试性和灵活性

**对比：**
```cpp
// 不用依赖注入 - 紧耦合
class ChatEngine {
    LLMClient llm_;           // 直接创建
    ToolExecutor executor_;   // 直接创建
};

// 使用依赖注入 - 松耦合
class ChatEngine {
    ChatEngine(LLMClient& llm, ToolExecutor& executor)  // 外部注入
        : llm_(llm), executor_(executor) {}
    
    LLMClient& llm_;
    ToolExecutor& executor_;
};
```

---

## 第一步：工具接口抽象

### 抽象基类设计

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

// ========== 工具抽象基类 ==========
class Tool {
public:
    virtual ~Tool() = default;
    
    // 工具名称（唯一标识）
    virtual std::string get_name() const = 0;
    
    // 工具描述（用于 LLM 理解）
    virtual std::string get_description() const = 0;
    
    // 执行工具
    virtual ToolResult execute(const std::string& arguments) const = 0;
};
```

### 具体工具实现

```cpp
// weather_tool.hpp
#pragma once
#include "tool.hpp"

class WeatherTool : public Tool {
public:
    std::string get_name() const override { return "get_weather"; }
    
    std::string get_description() const override {
        return R"({
            "name": "get_weather",
            "description": "查询指定城市的天气信息",
            "parameters": {
                "type": "object",
                "properties": {
                    "city": {
                        "type": "string",
                        "description": "城市名称，如：北京、上海"
                    }
                },
                "required": ["city"]
            }
        })";
    }
    
    ToolResult execute(const std::string& arguments) const override {
        // 解析参数
        std::string city = arguments;  // 简化处理
        
        // 模拟天气查询
        json::object data;
        data["city"] = city;
        data["weather"] = "晴天";
        data["temp"] = 25;
        
        return ToolResult::ok(json::serialize(data));
    }
};
```

其他工具（TimeTool, CalcTool, HttpTool 等）同样实现 `Tool` 基类...

---

## 第二步：工具注册表实现

### 注册表设计

```cpp
// tool_registry.hpp
#pragma once
#include "tool.hpp"
#include <memory>
#include <unordered_map>
#include <mutex>

class ToolRegistry {
public:
    // 获取单例实例
    static ToolRegistry& instance() {
        static ToolRegistry instance;
        return instance;
    }
    
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

private:
    ToolRegistry() = default;
    ~ToolRegistry() = default;
    ToolRegistry(const ToolRegistry&) = delete;
    ToolRegistry& operator=(const ToolRegistry&) = delete;
    
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<Tool>> tools_;
};
```

### 注册表使用

```cpp
// 注册工具
void register_all_tools() {
    auto& registry = ToolRegistry::instance();
    
    registry.register_tool(std::make_shared<WeatherTool>());
    registry.register_tool(std::make_shared<TimeTool>());
    registry.register_tool(std::make_shared<CalcTool>());
    registry.register_tool(std::make_shared<HttpTool>());
    registry.register_tool(std::make_shared<FileTool>());
    registry.register_tool(std::make_shared<CodeTool>());
}

// 执行工具
ToolResult execute_tool(const ToolCall& call) {
    auto& registry = ToolRegistry::instance();
    
    auto tool = registry.get_tool(call.name);
    if (!tool) {
        return ToolResult::fail("未知工具: " + call.name);
    }
    
    return tool->execute(call.arguments);
}
```

---

## 第三步：工具执行器重构

### 新的执行器实现

```cpp
// tool_executor.hpp
#pragma once
#include "tool_registry.hpp"
#include <future>
#include <thread>
#include <queue>

class ToolExecutor {
public:
    ToolExecutor(size_t max_concurrent = 3) 
        : max_concurrent_(max_concurrent), running_count_(0) {}
    
    // 同步执行
    ToolResult execute_sync(const ToolCall& call) {
        auto& registry = ToolRegistry::instance();
        
        auto tool = registry.get_tool(call.name);
        if (!tool) {
            return ToolResult::fail("未知工具: " + call.name);
        }
        
        return tool->execute(call.arguments);
    }
    
    // 异步执行（带并发控制）
    void execute_async(const ToolCall& call,
                       std::function<void(ToolResult)> callback,
                       std::chrono::milliseconds timeout = std::chrono::seconds(5)) {
        // 并发控制逻辑...
        // 使用注册表获取工具
        auto& registry = ToolRegistry::instance();
        auto tool = registry.get_tool(call.name);
        
        if (!tool) {
            callback(ToolResult::fail("未知工具: " + call.name));
            return;
        }
        
        // 在新线程中执行
        std::thread([tool, call, callback, timeout]() {
            auto result = execute_with_timeout(tool, call.arguments, timeout);
            callback(result);
        }).detach();
    }

private:
    ToolResult execute_with_timeout(std::shared_ptr<Tool> tool,
                                    const std::string& args,
                                    std::chrono::milliseconds timeout) {
        auto future = std::async(std::launch::async, [tool, &args]() {
            return tool->execute(args);
        });
        
        if (future.wait_for(timeout) == std::future_status::timeout) {
            return ToolResult::fail("执行超时");
        }
        
        return future.get();
    }
    
    size_t max_concurrent_;
    std::atomic<size_t> running_count_{0};
};
```

---

## 第四步：动态工具加载（高级）

### 插件化架构

```cpp
// plugin_loader.hpp
#pragma once
#include "tool_registry.hpp"
#include <dlfcn.h>  // Linux 动态库

class PluginLoader {
public:
    // 加载动态库中的工具
    static bool load_plugin(const std::string& path) {
        void* handle = dlopen(path.c_str(), RTLD_LAZY);
        if (!handle) {
            std::cerr << "无法加载插件: " << dlerror() << std::endl;
            return false;
        }
        
        // 获取注册函数
        using RegisterFunc = void(*)(ToolRegistry&);
        auto register_tools = (RegisterFunc)dlsym(handle, "register_tools");
        
        if (!register_tools) {
            std::cerr << "找不到注册函数: " << dlerror() << std::endl;
            dlclose(handle);
            return false;
        }
        
        // 调用注册函数
        auto& registry = ToolRegistry::instance();
        register_tools(registry);
        
        handles_.push_back(handle);
        return true;
    }
    
    // 卸载所有插件
    static void unload_all() {
        for (auto handle : handles_) {
            dlclose(handle);
        }
        handles_.clear();
    }

private:
    static std::vector<void*> handles_;
};
```

### 插件实现示例

```cpp
// my_plugin.cpp - 编译为动态库
#include "tool.hpp"
#include "tool_registry.hpp"

class MyCustomTool : public Tool {
public:
    std::string get_name() const override { return "my_tool"; }
    std::string get_description() const override { return "我的自定义工具"; }
    ToolResult execute(const std::string& args) const override {
        return ToolResult::ok(R"({"message": "Hello from plugin!"})");
    }
};

// 导出注册函数
extern "C" void register_tools(ToolRegistry& registry) {
    registry.register_tool(std::make_shared<MyCustomTool>());
}
```

---

## 第五步：代码演进对比

### Step 8 vs Step 9

```cpp
// Step 8: 硬编码分发
class ToolExecutor {
    static ToolResult execute(const ToolCall& call) {
        if (call.name == "get_weather") return WeatherTool::execute(...);
        else if (call.name == "get_time") return TimeTool::execute(...);
        // ... 每加工具要改这里
    }
};

// Step 9: 注册表分发
class ToolExecutor {
    ToolResult execute(const ToolCall& call) {
        auto tool = ToolRegistry::instance().get_tool(call.name);
        if (!tool) return ToolResult::fail("未知工具");
        return tool->execute(call.arguments);  // 多态调用
    }
};
```

### 优势对比

| 特性 | Step 8 硬编码 | Step 9 注册表 |
|:---|:---|:---|
| **扩展性** | 修改核心代码 | 只需注册新工具 |
| **耦合度** | 高（依赖所有工具） | 低（只依赖接口） |
| **可测试性** | 难（无法 mock） | 易（可注入 mock） |
| **动态加载** | 不支持 | 支持（插件） |
| **代码量** | 随工具增加 | 固定 |

---

## 本节总结

### 核心概念

1. **注册表模式**：中央注册表管理所有工具，解耦执行器与具体工具
2. **抽象基类**：Tool 接口统一所有工具的行为
3. **单例模式**：确保全局只有一个注册表实例
4. **动态加载**：通过插件机制运行时加载新工具

### 代码演进

```
Step 8: 硬编码工具执行 (700行)
   ↓ 演进
Step 9: 注册表模式 (750行)
   + tool.hpp: Tool 抽象基类
   + tool_registry.hpp: 单例注册表
   ~ tool_executor.hpp: 使用注册表替代硬编码
```

### 设计原则应用

1. **开闭原则**：对扩展开放（注册新工具），对修改关闭（不改执行器）
2. **依赖倒置**：依赖抽象（Tool 接口），不依赖具体实现
3. **单一职责**：注册表只负责管理，执行器只负责执行

---

## 📝 课后练习

### 练习 1：添加新工具
创建一个翻译工具（TranslateTool），支持中英互译：
- 名称：`translate`
- 参数：`text`（要翻译的文本），`target_lang`（目标语言）
- 实现：调用翻译 API 或简单字典映射

### 练习 2：工具优先级
为注册表添加优先级功能：
- 高优先级工具优先执行
- 同类型工具按优先级选择

### 练习 3：工具依赖注入
修改 ChatEngine 通过构造函数注入 ToolExecutor：
```cpp
ChatEngine(ToolExecutor& executor) : executor_(executor) {}
```

### 思考题
1. 注册表模式 vs 工厂模式，各适合什么场景？
2. 单例模式有什么缺点？如何改进？
3. 动态加载插件时如何保证安全性？

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

**恭喜！** 你现在掌握了注册表模式，工具系统变得可扩展了。下一章我们将引入 RAG，让 Agent 具备知识检索能力。
