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

### 软件设计原则回顾

在深入注册表模式之前，让我们回顾几个重要的软件设计原则：

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

**定义**：
1. 高层模块不应该依赖低层模块，两者都应该依赖抽象
2. 抽象不应该依赖细节，细节应该依赖抽象

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

### 注册表 vs 其他模式

| 模式 | 特点 | 适用场景 |
|:---|:---|:---|
| **工厂模式** | 创建对象 | 需要根据条件创建不同对象 |
| **策略模式** | 替换算法 | 运行时切换算法 |
| **注册表模式** | 管理映射关系 | 动态注册和查找 |
| **服务定位器** | 全局服务查找 | 大型系统的服务管理 |

**注册表模式最适合工具系统**，因为：
- 工具数量不固定（动态增减）
- 需要根据名称查找
- 需要遍历所有工具（如生成工具列表）

---

## 第一步：设计工具接口

### 为什么要抽象接口？

**接口的作用：**
1. **定义契约**：规定工具必须实现的方法
2. **实现多态**：统一处理不同类型的工具
3. **解耦合**：调用方只依赖接口，不依赖实现

### 工具接口设计

```cpp
// Tool 接口定义了所有工具必须遵守的契约
class Tool {
public:
    virtual ~Tool() = default;
    
    // 获取工具名称（唯一标识）
    virtual std::string get_name() const = 0;
    
    // 获取工具描述（给 LLM 看的）
    virtual std::string get_description() const = 0;
    
    // 执行工具
    virtual ToolResult execute(const std::string& args) const = 0;
};
```

**设计考虑：**
- `get_name()`：用于注册和查找
- `get_description()`：用于生成 LLM 的系统提示
- `execute()`：实际执行，参数和返回值都是字符串（JSON）

### 具体工具实现示例

```cpp
class WeatherTool : public Tool {
public:
    std::string get_name() const override { return "get_weather"; }
    
    std::string get_description() const override {
        return "查询指定城市的天气";
    }
    
    ToolResult execute(const std::string& city) const override {
        // 查询天气逻辑...
        return ToolResult::ok(result_json);
    }
};
```

---

## 第二步：注册表实现

### 单例模式

**为什么用单例？**
- 整个系统只需要一个注册表
- 全局可访问，方便工具注册和执行器查询

**单例的实现要点：**
- 私有构造函数（防止外部创建）
- 静态方法获取实例
- 删除拷贝构造函数和赋值操作符

### 注册表核心代码

```cpp
class ToolRegistry {
public:
    // 获取单例实例
    static ToolRegistry& instance() {
        static ToolRegistry instance;  // C++11 线程安全
        return instance;
    }
    
    // 注册工具
    void register_tool(std::shared_ptr<Tool> tool) {
        tools_[tool->get_name()] = tool;
    }
    
    // 根据名称获取工具
    std::shared_ptr<Tool> get_tool(const std::string& name) {
        auto it = tools_.find(name);
        return (it != tools_.end()) ? it->second : nullptr;
    }
    
    // 获取所有工具名称
    std::vector<std::string> get_all_names() const;

private:
    ToolRegistry() = default;  // 私有构造
    std::map<std::string, std::shared_ptr<Tool>> tools_;
};
```

### 注册工具

```cpp
void init_tools() {
    auto& registry = ToolRegistry::instance();
    
    registry.register_tool(std::make_shared<WeatherTool>());
    registry.register_tool(std::make_shared<TimeTool>());
    registry.register_tool(std::make_shared<CalcTool>());
    // 新增工具只需在这里加一行！
}
```

---

## 第三步：执行器改造

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

**优势：**
- 代码固定，不随工具数量增长
- 不依赖具体工具头文件
- 第三方工具也可以被调用

---

## 第四步：插件化扩展

### 动态加载思路

**编译时加载 vs 运行时加载：**

| 方式 | 时机 | 灵活性 | 复杂度 |
|:---|:---|:---:|:---:|
| 编译时 | 编译阶段 | 低 | 低 |
| 运行时 | 程序启动后 | 高 | 高 |

**动态加载流程：**
```
1. 程序扫描插件目录
2. 加载 .so / .dll 文件
3. 调用注册函数
4. 工具自动注册到注册表
```

### 插件接口定义

```cpp
// 插件必须实现的导出函数
extern "C" void register_tools(ToolRegistry& registry);
```

**使用：**
```cpp
// 加载插件库
void* handle = dlopen("my_plugin.so", RTLD_LAZY);
auto register_func = dlsym(handle, "register_tools");
register_func(ToolRegistry::instance());
```

---

## 第五节：设计模式总结

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

## 本节总结

### 核心概念

1. **注册表模式**：中央管理对象注册与查找，解耦组件
2. **抽象接口**：定义契约，实现多态，隐藏实现细节
3. **单例模式**：确保全局唯一实例
4. **插件化**：运行时动态扩展功能

### 演进对比

```
Step 8: 硬编码分发（违反开闭原则）
   ↓
Step 9: 注册表模式（符合开闭原则）
   - 新增工具不改核心代码
   - 支持第三方扩展
   - 易于测试（可 mock）
```

### 最佳实践

1. **接口设计要稳定**：频繁改动接口会影响所有实现
2. **注册时机**：建议在程序初始化时统一注册
3. **错误处理**：工具不存在时给出明确错误
4. **线程安全**：注册表操作需要加锁保护

---

## 📝 课后练习

### 练习 1：优先级系统
为工具添加优先级，同类型任务优先执行高优先级工具。

### 练习 2：工具组
实现工具组概念，可以一次性注册/卸载一组相关工具。

### 练习 3：依赖检查
工具可以声明依赖其他工具，注册时自动检查依赖是否存在。

### 思考题
1. 注册表模式和全局变量有什么区别？
2. 如果注册表变得很大，如何优化查找性能？
3. 插件化架构的安全风险有哪些？

---

**恭喜！** 你现在掌握了注册表模式，工具系统变得可扩展了。下一章我们将引入 RAG，让 Agent 具备知识检索能力。
