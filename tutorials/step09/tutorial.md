# Step 9: 工具注册表 —— 依赖注入与解耦

> 目标：实现注册表模式和依赖注入，支持动态工具管理和配置驱动
> 
003e 难度：⭐⭐⭐ | 代码量：约 750 行 | 预计学习时间：3-4 小时

---

## 一、为什么需要注册表模式？

### 1.1 Step 8 的问题

Step 8 的安全沙箱提供了防护，但工具管理还是硬编码：

```cpp
class Agent {
public:
    Agent() {
        // 硬编码注册工具
        tool_manager_.register_tool(std::make_shared<WeatherTool>());
        tool_manager_.register_tool(std::make_shared<CalcTool>());
        tool_manager_.register_tool(std::make_shared<TimeTool>());
        // 每加一个工具都要改代码、重新编译！
    }
};

// 问题：
// 1. 新增工具需要修改 Agent 代码
// 2. 无法根据配置动态加载
// 3. 工具之间的依赖关系难以管理
// 4. 测试困难（无法 mock 工具）
```

### 1.2 注册表模式的价值

```
硬编码方式：                           注册表方式：
┌──────────────┐                      ┌──────────────┐
│   Agent      │                      │   Agent      │
│ ┌──────────┐ │                      │ ┌──────────┐ │
│ │ Weather  │ │ 直接依赖              │ │ Registry │ │ 通过注册表间接依赖
│ │ Calc     │ │                       │ └────┬─────┘ │
│ │ Time     │ │                       └──────┼───────┘
└──────────────┘                              │
                                              │
                                       ┌──────┴───────┐
                                       │  Tool Factory  │
                                       │  ┌───┐┌───┐  │
                                       │  │ W ││ C │  │
                                       │  └───┘└───┘  │
                                       └──────────────┘
```

**好处：**
- **解耦**：Agent 不直接依赖具体工具
- **可配置**：工具可以从配置文件加载
- **可测试**：容易替换 mock 工具
- **可扩展**：新增工具不需要改 Agent 代码

---

## 二、注册表模式设计

### 2.1 核心概念

```cpp
// 1. 工具工厂：负责创建工具实例
using ToolFactory = std::function<std::shared_ptr<ITool>()>;

// 2. 注册表：存储工具名称到工厂的映射
class ToolRegistry {
public:
    // 注册工具工厂
    void register_factory(const std::string& name, ToolFactory factory);
    
    // 创建工具实例
    std::shared_ptr<ITool> create(const std::string& name);
    
    // 获取所有已注册的工具名
    std::vector<std::string> list_tools() const;

private:
    std::map<std::string, ToolFactory> factories_;
};
```

### 2.2 依赖注入容器

更进一步的**依赖注入（DI）容器**：

```cpp
class DIContainer {
public:
    // 注册单例（整个应用共享一个实例）
    template<typename T, typename... Args>
    void register_singleton(Args... args);
    
    // 注册原型（每次请求创建新实例）
    template<typename T, typename... Args>
    void register_prototype(Args... args);
    
    // 获取实例
    template<typename T>
    std::shared_ptr<T> resolve();
    
    // 解析依赖并注入
    template<typename T>
    void wire(std::shared_ptr<T> instance);

private:
    std::map<std::type_index, std::any> registrations_;
    std::map<std::type_index, std::shared_ptr<void>> singletons_;
};
```

---

## 三、注册表实现

### 3.1 基础注册表

```cpp
class ToolRegistry {
public:
    // 注册工具工厂
    template<typename T, typename... Args>
    void register_tool(const std::string& name, Args... args) {
        static_assert(std::is_base_of_v<ITool, T>, 
                      "T must inherit from ITool");
        
        // 存储工厂函数
        factories_[name] = [...args]() -> std::shared_ptr<ITool> {
            return std::make_shared<T>(args...);
        };
        
        std::cout << "Registered tool: " << name << "\n";
    }
    
    // 创建工具实例
    std::shared_ptr<ITool> create(const std::string& name) const {
        auto it = factories_.find(name);
        if (it == factories_.end()) {
            throw std::runtime_error("Tool not found: " + name);
        }
        return it->second();  // 调用工厂函数创建实例
    }
    
    // 检查工具是否存在
    bool has_tool(const std::string& name) const {
        return factories_.count(name) > 0;
    }
    
    // 获取所有工具名
    std::vector<std::string> list_tools() const {
        std::vector<std::string> names;
        for (const auto& [name, _] : factories_) {
            names.push_back(name);
        }
        return names;
    }

private:
    std::map<std::string, ToolFactory> factories_;
};
```

### 3.2 配置文件驱动

从配置文件加载工具：

```cpp
class ToolLoader {
public:
    // 从 JSON 配置文件加载工具
    void load_from_config(const std::string& config_path,
                          ToolRegistry& registry);

private:
    // 内置工具类型映射
    std::map<std::string, std::function<ToolFactory(const json&)>> 
        builtin_tools_ = {
        {"weather", [](const json& cfg) -> ToolFactory {
            std::string api_key = cfg.value("api_key", "");
            return [api_key]() -> std::shared_ptr<ITool> {
                return std::make_shared<WeatherTool>(api_key);
            };
        }},
        {"calculator", [](const json& cfg) -> ToolFactory {
            int precision = cfg.value("precision", 2);
            return [precision]() -> std::shared_ptr<ITool> {
                return std::make_shared<CalculatorTool>(precision);
            };
        }},
        {"time", [](const json&) -> ToolFactory {
            return []() -> std::shared_ptr<ITool> {
                return std::make_shared<TimeTool>();
            };
        }}
    };
};

void ToolLoader::load_from_config(const std::string& config_path,
                                  ToolRegistry& registry) {
    // 读取配置文件
    std::ifstream file(config_path);
    json config;
    file >> config;
    
    // 遍历配置中的工具
    for (const auto& tool_config : config["tools"]) {
        std::string name = tool_config["name"];
        std::string type = tool_config["type"];
        
        if (!builtin_tools_.count(type)) {
            std::cerr << "Unknown tool type: " << type << "\n";
            continue;
        }
        
        // 创建工厂函数并注册
        ToolFactory factory = builtin_tools_[type](tool_config);
        registry.register_factory(name, factory);
    }
}
```

**配置文件示例（tools.json）：**

```json
{
    "tools": [
        {
            "name": "get_weather",
            "type": "weather",
            "api_key": "${WEATHER_API_KEY}",
            "timeout_ms": 5000
        },
        {
            "name": "calculate",
            "type": "calculator",
            "precision": 2
        },
        {
            "name": "get_time",
            "type": "time"
        }
    ]
}
```

---

## 四、依赖注入实现

### 4.1 依赖标记

```cpp
// 依赖注入标记
// 在工具类中使用，声明依赖的其他工具
struct Dependency {
    std::string name;
    std::type_index type;
};

#define INJECT(type, name) \
    static std::vector<Dependency> get_dependencies() { \
        return {{name, std::type_index(typeid(type))}}; \
    } \
    std::shared_ptr<type> name##_
```

### 4.2 依赖解析

```cpp
class DependencyResolver {
public:
    DependencyResolver(ToolRegistry& registry) : registry_(registry) {}
    
    // 解析工具的所有依赖
    void resolve_dependencies(std::shared_ptr<ITool> tool);
    
    // 检查循环依赖
    bool has_circular_dependency(const std::string& tool_name);

private:
    ToolRegistry& registry_;
    std::map<std::string, std::shared_ptr<ITool>> instances_;
};

void DependencyResolver::resolve_dependencies(std::shared_ptr<ITool> tool) {
    // 获取工具声明的依赖
    auto deps = tool->get_dependencies();
    
    for (const auto& dep : deps) {
        // 检查是否已有实例
        if (!instances_.count(dep.name)) {
            // 创建依赖实例
            auto dep_tool = registry_.create(dep.name);
            
            // 递归解析依赖的依赖
            resolve_dependencies(dep_tool);
            
            instances_[dep.name] = dep_tool;
        }
        
        // 注入依赖
        tool->inject_dependency(dep.name, instances_[dep.name]);
    }
}
```

### 4.3 带依赖的工具示例

```cpp
// 旅行规划工具，依赖天气和酒店查询工具
class TravelPlanningTool : public ITool {
public:
    // 声明依赖
    INJECT(WeatherTool, weather_tool);
    INJECT(HotelSearchTool, hotel_tool);
    
    std::string get_name() const override { return "plan_travel"; }
    
    std::string get_description() const override {
        return "规划旅行行程，包括天气查询和酒店推荐";
    }
    
    ToolResult execute(const json& args, const ToolContext& ctx) override {
        std::string destination = args["destination"];
        
        // 使用注入的依赖工具
        auto weather = weather_tool_->get_weather(destination);
        auto hotels = hotel_tool_->search_hotels(destination);
        
        // 整合结果
        return ToolResult{
            .success = true,
            .data = {
                {"destination", destination},
                {"weather", weather},
                {"hotels", hotels}
            }
        };
    }
};
```

---

## 五、完整集成

### 5.1 新的 Agent 初始化流程

```cpp
class Agent {
public:
    Agent(const std::string& config_path) {
        // 1. 创建注册表
        ToolRegistry registry;
        
        // 2. 从配置文件加载工具
        ToolLoader loader;
        loader.load_from_config(config_path, registry);
        
        // 3. 创建依赖解析器
        DependencyResolver resolver(registry);
        
        // 4. 创建所有工具实例并解析依赖
        for (const auto& name : registry.list_tools()) {
            auto tool = registry.create(name);
            resolver.resolve_dependencies(tool);
            tool_manager_.register_tool(tool);
        }
        
        // 5. 初始化安全组件
        security_checker_ = std::make_unique<SecurityChecker>(config_path);
        audit_logger_ = std::make_unique<AuditLogger>("./logs");
    }

private:
    ToolManager tool_manager_;
    std::unique_ptr<SecurityChecker> security_checker_;
    std::unique_ptr<AuditLogger> audit_logger_;
};
```

---

## 六、本章小结

**核心收获：**

1. **注册表模式**：
   - 工厂函数注册
   - 延迟创建实例
   - 解耦工具创建和使用

2. **配置文件驱动**：
   - JSON/YAML 配置
   - 动态加载工具
   - 无需重新编译

3. **依赖注入**：
   - 声明依赖关系
   - 自动解析和注入
   - 支持循环依赖检测

---

## 七、引出的问题

### 7.1 知识检索问题

目前 Agent 只有实时工具调用，没有知识库：

```
用户: "公司的请假流程是什么？"

Agent: 无法回答（没有相关知识库）
```

**需要：** 向量数据库 + RAG 检索。

---

**下一章预告（Step 10）：**

我们将实现**RAG 检索**：
- 向量嵌入（Embedding）
- 向量数据库（Chroma/pgvector）
- 相似度检索
- 知识增强回复

工具系统已经完善，接下来要让 Agent 拥有"记忆"和"知识"。
