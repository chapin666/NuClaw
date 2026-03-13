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

### 回顾 Step 6-8 的问题

**Step 6-8 的硬编码问题：**

```cpp
// tool_executor.hpp - 硬编码分发
static ToolResult execute(const ToolCall& call) {
    if (call.name == "get_weather") 
        return WeatherTool::execute(call.args);
    else if (call.name == "get_time") 
        return TimeTool::execute(call.args);
    else if (call.name == "calculate") 
        return CalcTool::execute(call.args);
    // ... 每加一个工具都要修改这里！
    
    return ToolResult::fail("未知工具");
}
```

**产生的问题：**

| 问题 | 说明 | 后果 |
|:---|:---|:---|
| **违反开闭原则** | 每加工具要改核心代码 | 难以维护 |
| **编译依赖** | 执行器依赖所有工具头文件 | 编译变慢 |
| **无法扩展** | 第三方无法添加工具 | 生态封闭 |
| **测试困难** | 无法单独 mock 工具 | 单元测试复杂 |

### 生活中的类比

**硬编码方式 = 墙上固定的插座：**
```
场景：你想给手机充电
问题：墙上的插座是固定的，不能更换
结果：你的充电器插不上，需要重新装修（改代码）
```

**注册表方式 = 可插拔的插线板：**
```
场景：你想给手机充电
解决：买一个插线板，插上就能用
结果：不需要改墙上的插座，随时增减插头
```

### 什么是注册表模式？

**核心思想：** 中央注册表管理所有工具，执行器只与注册表交互。

```
硬编码方式：                    注册表方式：
执行器 ──知道──> 工具A           工具A ──注册──> 注册表
   ├──知道──> 工具B    vs       工具B ──注册──> 注册表 <──查询── 执行器
   ├──知道──> 工具C              工具C ──注册──> 注册表
   └──...（耦合所有工具）         （执行器只依赖注册表）
```

**优势：**
- ✅ 新增工具不改执行器代码
- ✅ 支持第三方扩展
- ✅ 易于测试（可 mock）
- ✅ 符合开闭原则

---

## 第一步：设计工具接口

### 为什么要抽象接口？

**接口的作用：**
1. **定义契约**：规定工具必须实现的方法
2. **实现多态**：统一处理不同类型的工具
3. **解耦合**：调用方只依赖接口，不依赖实现

**类比：** USB 接口
```
电脑（调用方）依赖 USB 接口（抽象）
U盘、鼠标、键盘（工具）都实现 USB 接口
换设备不需要改电脑代码
```

### 工具接口设计

```cpp
// tool.hpp - 工具抽象基类
class Tool {
public:
    virtual ~Tool() = default;
    
    // 工具名称（唯一标识）
    virtual std::string get_name() const = 0;
    
    // 工具描述（给 LLM 看的）
    virtual std::string get_description() const = 0;
    
    // 执行工具
    virtual ToolResult execute(const std::string& args) const = 0;
};
```

### 具体工具实现示例

```cpp
// weather_tool.hpp
class WeatherTool : public Tool {
public:
    std::string get_name() const override { return "get_weather"; }
    
    std::string get_description() const override {
        return "查询指定城市的天气信息";
    }
    
    ToolResult execute(const std::string& city) const override {
        // 查询天气...
        return ToolResult::ok(result_json);
    }
};
```

---

## 第二步：实现注册表

### 单例模式

**为什么用单例？**
- 整个系统只需要一个注册表
- 全局可访问，方便工具注册和执行器查询

**类比：** 电话号码簿
```
整个城市共用一本电话簿
任何人都可以查（执行器查询）
任何人都可以登记（工具注册）
```

### 注册表核心代码

```cpp
// tool_registry.hpp
class ToolRegistry {
public:
    // 获取单例
    static ToolRegistry& instance() {
        static ToolRegistry instance;
        return instance;
    }
    
    // 注册工具
    void register_tool(std::shared_ptr<Tool> tool) {
        tools_[tool->get_name()] = tool;
    }
    
    // 获取工具
    std::shared_ptr<Tool> get_tool(const std::string& name) {
        auto it = tools_.find(name);
        return (it != tools_.end()) ? it->second : nullptr;
    }

private:
    std::map<std::string, std::shared_ptr<Tool>> tools_;
};
```

---

## 第三步：改造执行器

### 改造前后对比

**改造前（硬编码）：**
```cpp
ToolResult execute(const ToolCall& call) {
    if (call.name == "weather") return WeatherTool::execute(...);
    else if (call.name == "time") return TimeTool::execute(...);
    // ... 无限增长
}
```

**改造后（使用注册表）：**
```cpp
ToolResult execute(const ToolCall& call) {
    auto tool = ToolRegistry::instance().get_tool(call.name);
    if (!tool) return ToolResult::fail("未知工具");
    
    return tool->execute(call.arguments);  // 多态调用
}
```

**优势：** 代码固定，不随工具数量增长

---

## 第四步：使用注册表

### 注册工具

```cpp
void init_tools() {
    auto& registry = ToolRegistry::instance();
    
    registry.register_tool(std::make_shared<WeatherTool>());
    registry.register_tool(std::make_shared<TimeTool>());
    registry.register_tool(std::make_shared<CalcTool>());
    
    // 新增工具只需加一行！
}
```

---

## 本节总结

### 核心概念

1. **注册表模式**：中央管理工具注册与查找
2. **抽象基类**：定义统一接口，实现多态
3. **单例模式**：确保全局唯一实例

### 代码演进

```
Step 8: 硬编码分发（违反开闭原则）
   ↓
Step 9: 注册表模式（符合开闭原则）
   - 新增工具不改核心代码
   - 支持第三方扩展
```

---

## 📝 课后练习

### 练习 1：添加翻译工具
实现一个翻译工具，支持中英互译。

### 练习 2：工具版本管理
扩展注册表支持工具版本控制。

### 思考题
1. 为什么用 shared_ptr 而不是原始指针？
2. 如果注册表很大，如何优化查找性能？

---

## 📖 扩展阅读

- **设计模式**：工厂模式、依赖注入
- **软件原则**：SOLID 原则详解
- **实际案例**：Chrome 扩展系统
