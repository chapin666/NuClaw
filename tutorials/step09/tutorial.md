# Step 9: 工具与技能系统 —— 从单工具到能力组合

> 目标：实现工具注册表和 Skill 封装，支持工具组合与复用
> 
> 难度：⭐⭐⭐ | 代码量：约 750 行 | 预计学习时间：3-4 小时

---

## 一、为什么需要技能系统？

### 1.1 Step 8 的问题

Step 8 的安全沙箱提供了防护，但工具管理还是**单工具粒度**：

```cpp
class Agent {
public:
    Agent() {
        // 硬编码注册单个工具
        tool_manager_.register_tool(std::make_shared<WeatherTool>());
        tool_manager_.register_tool(std::make_shared<CalcTool>());
        tool_manager_.register_tool(std::make_shared<TimeTool>());
        // 每加一个工具都要改代码、重新编译！
    }
    
    // 问题：工具之间没有关联，无法组合完成复杂任务
};

// 用户："帮我规划去东京的旅行"
// Agent 只能一个个调用工具，无法形成连贯的"旅行规划"能力
```

**三个核心问题：**
1. **粒度太细** - 单个工具无法表达复杂能力
2. **缺乏组合** - 工具之间无法协作
3. **难以复用** - 同样的工具组合要重复配置

### 1.2 Skill 的概念

**Skill（技能）** 是完成特定任务的**工具组合 + 工作流**：

```
单工具 vs Skill：

单工具：                              Travel Skill：
┌─────────────┐                       ┌─────────────────────────────┐
│ WeatherTool │                       │      旅行规划技能            │
└─────────────┘                       │  ┌─────────┐ ┌─────────┐  │
                                      │  │ Weather │ │ Flights │  │
┌─────────────┐                       │  └────┬────┘ └────┬────┘  │
│ FlightTool  │                       │       └─────┬─────┘       │
└─────────────┘                       │             ▼             │
                                      │      ┌──────────┐         │
┌─────────────┘                       │      │ 行程整合  │         │
│ HotelTool   │                       │      └──────────┘         │
└─────────────┘                       └─────────────────────────────┘

问题：工具零散                           优势：封装完整能力
      用户需自行组合                          一次配置，多次复用
```

**Skill 的价值：**
- **封装复杂能力** - 一次配置，多次复用
- **工具组合** - 多个工具协作完成特定任务
- **领域抽象** - 面向业务场景而非单个功能

---

## 二、技能系统设计

### 2.1 核心概念

```cpp
// Skill：封装完成特定任务的工具集合 + 工作流
struct Skill {
    std::string name;                    // 技能名称
    std::string description;             // 技能描述
    std::vector<std::string> tools;      // 包含的工具名
    json::object workflow;               // 执行工作流（工具调用顺序）
};

// 工具工厂：负责创建工具实例
using ToolFactory = std::function<std::shared_ptr<ITool>()>;

// 注册表：统一管理工具和 Skill
class ToolRegistry {
public:
    // 注册单个工具
    void register_tool(const std::string& name, ToolFactory factory);
    
    // 注册 Skill（工具组合）⭐ 新增
    void register_skill(const Skill& skill);
    
    // 创建工具实例
    std::shared_ptr<ITool> create_tool(const std::string& name);
    
    // 获取 Skill 包含的所有工具 ⭐ 新增
    std::vector<std::shared_ptr<ITool>> create_skill_tools(const std::string& skill_name);
    
    // 获取所有已注册的工具和 Skill
    std::vector<std::string> list_tools() const;
    std::vector<std::string> list_skills() const;  // ⭐ 新增

private:
    std::map<std::string, ToolFactory> factories_;
    std::map<std::string, Skill> skills_;  // ⭐ 新增
};
```

### 2.2 Skill 配置示例

```json
{
    "skills": [
        {
            "name": "travel_planning",
            "description": "规划旅行行程，包括天气查询、航班搜索和酒店推荐",
            "tools": ["weather", "flight_search", "hotel_search"],
            "workflow": {
                "steps": [
                    {"tool": "weather", "input": "{destination}"},
                    {"tool": "flight_search", "input": "{origin}_to_{destination}"},
                    {"tool": "hotel_search", "input": "{destination}"}
                ]
            }
        },
        {
            "name": "data_analysis",
            "description": "数据分析技能：查询数据库并生成图表",
            "tools": ["sql_query", "chart_generator"],
            "workflow": {
                "steps": [
                    {"tool": "sql_query", "input": "{query}"},
                    {"tool": "chart_generator", "input": "{query_result}"}
                ]
            }
        }
    ]
}
```

---

## 三、注册表实现

### 3.1 基础注册表（支持 Skill）

```cpp
class ToolRegistry {
public:
    // 注册工具工厂
    template<typename T, typename... Args>
    void register_tool(const std::string& name, Args... args) {
        static_assert(std::is_base_of_v<ITool, T>, 
                      "T must inherit from ITool");
        
        factories_[name] = [...args]() -> std::shared_ptr<ITool> {
            return std::make_shared<T>(args...);
        };
        
        std::cout << "[Registry] Registered tool: " << name << "\n";
    }
    
    // ⭐ 注册 Skill
    void register_skill(const Skill& skill) {
        // 验证所有工具都已注册
        for (const auto& tool_name : skill.tools) {
            if (!has_tool(tool_name)) {
                throw std::runtime_error(
                    "Skill '" + skill.name + "' requires unknown tool: " + tool_name
                );
            }
        }
        
        skills_[skill.name] = skill;
        std::cout << "[Registry] Registered skill: " << skill.name 
                  << " (" << skill.tools.size() << " tools)\n";
    }
    
    // 创建工具实例
    std::shared_ptr<ITool> create_tool(const std::string& name) const {
        auto it = factories_.find(name);
        if (it == factories_.end()) {
            throw std::runtime_error("Tool not found: " + name);
        }
        return it->second();
    }
    
    // ⭐ 获取 Skill 的所有工具实例
    std::vector<std::shared_ptr<ITool>> create_skill_tools(const std::string& skill_name) {
        auto it = skills_.find(skill_name);
        if (it == skills_.end()) {
            throw std::runtime_error("Skill not found: " + skill_name);
        }
        
        std::vector<std::shared_ptr<ITool>> tools;
        for (const auto& tool_name : it->second.tools) {
            tools.push_back(create_tool(tool_name));
        }
        return tools;
    }
    
    // 检查工具是否存在
    bool has_tool(const std::string& name) const {
        return factories_.count(name) > 0;
    }
    
    // ⭐ 检查 Skill 是否存在
    bool has_skill(const std::string& name) const {
        return skills_.count(name) > 0;
    }
    
    // 获取所有工具名
    std::vector<std::string> list_tools() const {
        std::vector<std::string> names;
        for (const auto& [name, _] : factories_) {
            names.push_back(name);
        }
        return names;
    }
    
    // ⭐ 获取所有 Skill 名
    std::vector<std::string> list_skills() const {
        std::vector<std::string> names;
        for (const auto& [name, _] : skills_) {
            names.push_back(name);
        }
        return names;
    }
    
    // ⭐ 获取 Skill 信息
    std::optional<Skill> get_skill(const std::string& name) const {
        auto it = skills_.find(name);
        if (it != skills_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

private:
    std::map<std::string, ToolFactory> factories_;
    std::map<std::string, Skill> skills_;
};
```

### 3.2 配置文件驱动

```cpp
class ToolLoader {
public:
    // 从 JSON 配置文件加载工具和 Skill
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
        }},
        {"flight_search", [](const json& cfg) -> ToolFactory {
            std::string api_key = cfg.value("api_key", "");
            return [api_key]() -> std::shared_ptr<ITool> {
                return std::make_shared<FlightSearchTool>(api_key);
            };
        }},
        {"hotel_search", [](const json& cfg) -> ToolFactory {
            std::string api_key = cfg.value("api_key", "");
            return [api_key]() -> std::shared_ptr<ITool> {
                return std::make_shared<HotelSearchTool>(api_key);
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
    
    // 1. 加载工具
    if (config.contains("tools")) {
        for (const auto& tool_config : config["tools"]) {
            std::string name = tool_config["name"];
            std::string type = tool_config["type"];
            
            if (!builtin_tools_.count(type)) {
                std::cerr << "Unknown tool type: " << type << "\n";
                continue;
            }
            
            ToolFactory factory = builtin_tools_[type](tool_config);
            registry.register_factory(name, factory);
        }
    }
    
    // 2. ⭐ 加载 Skill
    if (config.contains("skills")) {
        for (const auto& skill_config : config["skills"]) {
            Skill skill;
            skill.name = skill_config["name"];
            skill.description = skill_config.value("description", "");
            
            for (const auto& tool_name : skill_config["tools"]) {
                skill.tools.push_back(tool_name);
            }
            
            if (skill_config.contains("workflow")) {
                skill.workflow = skill_config["workflow"];
            }
            
            registry.register_skill(skill);
        }
    }
}
```

---

## 四、在 Agent 中使用 Skill

### 4.1 Agent 集成

```cpp
class Agent {
public:
    Agent(const std::string& config_path) {
        // 1. 创建注册表
        ToolRegistry registry;
        
        // 2. 从配置文件加载工具和 Skill
        ToolLoader loader;
        loader.load_from_config(config_path, registry);
        
        // 3. 初始化安全组件
        security_checker_ = std::make_unique<SecurityChecker>(config_path);
        audit_logger_ = std::make_unique<AuditLogger>("./logs");
        
        registry_ = std::move(registry);
    }
    
    // 处理用户消息
    std::string process_message(const std::string& user_input) {
        // 检查是否调用 Skill
        if (registry_.has_skill(user_input)) {
            return execute_skill(user_input);
        }
        
        // 否则走常规 LLM 处理
        return process_with_llm(user_input);
    }
    
    // ⭐ 执行 Skill（顺序调用工具组合）
    std::string execute_skill(const std::string& skill_name) {
        auto skill_opt = registry_.get_skill(skill_name);
        if (!skill_opt) {
            return "Skill not found: " + skill_name;
        }
        
        const auto& skill = *skill_opt;
        json::object results;
        
        std::cout << "[Agent] Executing skill: " << skill.name << "\n";
        
        // 按 workflow 顺序调用工具
        for (const auto& step : skill.workflow.at("steps").as_array()) {
            std::string tool_name = std::string(step.at("tool").as_string());
            
            auto tool = registry_.create_tool(tool_name);
            
            // 安全检查
            if (!security_checker_->check(tool_name, {})) {
                audit_logger_->log(tool_name, false, "Security check failed");
                continue;
            }
            
            // 执行工具
            ToolResult result = tool->execute({}, {});
            results[tool_name] = result.data;
            
            audit_logger_->log(tool_name, result.success, "");
        }
        
        // 整合结果返回
        return format_skill_result(skill_name, results);
    }
    
    // 显示可用能力
    void show_capabilities() {
        std::cout << "\n📦 Available Tools:\n";
        for (const auto& name : registry_.list_tools()) {
            std::cout << "  • " << name << "\n";
        }
        
        std::cout << "\n🎯 Available Skills:\n";
        for (const auto& name : registry_.list_skills()) {
            auto skill = registry_.get_skill(name);
            std::cout << "  • " << name << " - " << skill->description << "\n";
            std::cout << "    (" << skill->tools.size() << " tools)\n";
        }
    }

private:
    ToolRegistry registry_;
    std::unique_ptr<SecurityChecker> security_checker_;
    std::unique_ptr<AuditLogger> audit_logger_;
    
    std::string format_skill_result(const std::string& skill_name, 
                                    const json::object& results);
    std::string process_with_llm(const std::string& input);
};
```

---

## 五、配置示例

### 5.1 完整配置文件

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
            "name": "search_flights",
            "type": "flight_search",
            "api_key": "${FLIGHT_API_KEY}"
        },
        {
            "name": "search_hotels",
            "type": "hotel_search",
            "api_key": "${HOTEL_API_KEY}"
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
    ],
    "skills": [
        {
            "name": "travel_planning",
            "description": "规划完整旅行行程",
            "tools": ["get_weather", "search_flights", "search_hotels"],
            "workflow": {
                "steps": [
                    {"tool": "get_weather", "input": "{destination}"},
                    {"tool": "search_flights", "input": "{origin}_to_{destination}"},
                    {"tool": "search_hotels", "input": "{destination}"}
                ]
            }
        },
        {
            "name": "trip_budget",
            "description": "计算旅行预算",
            "tools": ["calculate", "get_time"],
            "workflow": {
                "steps": [
                    {"tool": "get_time", "input": "now"},
                    {"tool": "calculate", "input": "{flight_price} + {hotel_price}"}
                ]
            }
        }
    ],
    "security": {
        "allowed_domains": ["api.weather.com", "api.flights.com"],
        "max_file_size": 10485760,
        "timeout_ms": 30000
    }
}
```

---

## 六、本章小结

### 6.1 演进对比

```
Step 8: 单个工具 + 安全沙箱
    ↓ 问题：工具粒度太细，无法组合
Step 9: 工具与技能系统 ⭐ 本章
    • ToolRegistry - 统一管理工具
    • Skill - 封装工具组合
    • 配置驱动 - 外部化配置
```

### 6.2 新增能力

| 特性 | 说明 |
|:---|:---|
| **Skill 封装** | 将多个工具组合成可复用的能力单元 |
| **工作流** | 定义工具调用顺序和数据传递 |
| **统一注册表** | 工具和 Skill 统一管理 |
| **配置驱动** | JSON 配置文件定义能力 |

### 6.3 仍存在什么问题？

当前 Skill 的工具有限，都是我们自己实现的。如何接入**社区生态**中已有的工具？

---

## 七、课后思考

<details>
<summary>点击查看下一章要解决的问题 💡</summary>

**问题 1：工具生态局限**

当前所有工具都需要自己实现。如何使用社区现成的工具（如访问 GitHub、查询数据库、操作文件系统）？

**问题 2：协议标准化**

不同厂商的工具接口各异。如何让 NuClaw 支持标准化的工具协议？

**问题 3：动态发现**

目前工具和 Skill 都是配置文件写死的。如何实现运行时动态发现和加载？

**Step 12 预告：MCP 协议接入**

我们将实现 **Model Context Protocol (MCP)**：
- **MCP Client** - 连接任意 MCP Server
- **工具自动发现** - 动态获取可用工具
- **标准化接入** - 一次实现，连接 100+ 社区工具

接入 MCP 后，NuClaw 可以无缝使用：
- 文件系统操作
- 数据库查询
- GitHub 管理
- Slack 消息
- ...以及不断增长的生态

</details>

---

## 参考

- Skill 概念参考：OpenAI Functions、LangChain Tools
- 下一章 MCP：Anthropic Model Context Protocol

---

## 文件变更清单

| 文件 | 变更类型 | 说明 |
|:---|:---|:---|
| `include/nuclaw/tool_registry.hpp` | **修改** | 新增 Skill 支持 |
| `src/tool_registry.cpp` | **修改** | 实现 Skill 注册和创建 |
| `include/nuclaw/skill.hpp` | **新增** | Skill 定义结构 |
| `config/tools.json` | **修改** | 添加 skill 配置段 |
| `src/main.cpp` | **修改** | 演示 Skill 执行 |

**复用文件：**
- 复用 Step 8 的安全检查组件
- 复用 Step 6 的工具基类
