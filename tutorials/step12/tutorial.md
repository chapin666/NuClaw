# Step 12: 配置管理 - 灵活调整 Agent 行为

> 目标：实现外部化配置，支持运行时调整参数
> 
> 难度：⭐⭐⭐⭐ (较难)
> 
> 代码量：约 900 行
> 
> 预计学习时间：4-5 小时

---

## 📚 前置知识

### 硬编码配置的问题

**场景：** 修改 LLM 模型
```cpp
// ❌ 硬编码
class LLMClient {
    string model_ = "gpt-4";  // 改模型要重新编译！
    int timeout_ = 30;         // 无法动态调整！
};
```

**问题：**
- 每次改配置都要重新编译
- 无法根据环境切换（开发/测试/生产）
- 敏感信息泄露在代码中

### 生活中的类比

**硬编码 = 刻在墙上的菜单：**
```
问题：想换一道菜
解决：需要重新装修（改代码、重新编译）
```

**配置文件 = 可更换的菜单板：**
```
问题：想换一道菜
解决：直接换菜单板（改配置文件，立即生效）
```

### 配置分层

**优先级从高到低：**
```
1. 命令行参数（临时覆盖）
   --port=8080

2. 环境变量（容器化部署）
   export NUCLAW_PORT=8080

3. 配置文件（主要配置）
   port: 8080

4. 代码默认值（保底）
   int port = 8080;
```

---

## 第一步：配置类设计

### 支持的功能

- **格式支持**：YAML、JSON
- **分层加载**：文件 → 环境变量 → 命令行
- **类型安全**：string、int、bool、double
- **默认值**：配置缺失时使用默认值

### 配置类实现

```cpp
class Config {
public:
    // 加载配置文件
    bool load_yaml(const string& path);
    bool load_json(const string& path);
    
    // 加载环境变量
    void load_env(const string& prefix = "NUCLAW_");
    
    // 读取配置（带默认值）
    string get_string(const string& key, 
                      const string& default_val = "");
    int get_int(const string& key, int default_val = 0);
    bool get_bool(const string& key, bool default_val = false);
    
    // 设置配置
    void set(const string& key, const string& value);
};
```

---

## 第二步：配置文件示例

### YAML 配置

```yaml
# config.yaml

# 服务器配置
server:
  host: "0.0.0.0"
  port: 8080

# LLM 配置
llm:
  provider: "openai"
  api_key: "${OPENAI_API_KEY}"  # 从环境变量读取
  model: "gpt-4"
  timeout: 30
  temperature: 0.7

# Agent 行为配置
agent:
  welcome_message: "你好！我是 AI 助手"
  max_history: 10

# 工具配置
tools:
  weather:
    api_key: "${WEATHER_API_KEY}"
```

---

## 第三步：集成到 Agent

### ChatEngine 配置化

```cpp
class ChatEngine {
public:
    void init(const Config& config) {
        // 从配置初始化 LLM
        llm_.set_model(config.get_string("llm.model", "gpt-3.5"));
        llm_.set_timeout(config.get_int("llm.timeout", 30));
        
        // 从配置初始化行为
        welcome_msg_ = config.get_string("agent.welcome_message");
        max_history_ = config.get_int("agent.max_history", 10);
    }
};
```

### 主程序

```cpp
int main() {
    Config config;
    
    // 1. 加载配置文件
    config.load_yaml("config.yaml");
    
    // 2. 环境变量覆盖
    config.load_env("NUCLAW_");
    
    // 3. 初始化 Agent
    ChatEngine engine;
    engine.init(config);
    
    return 0;
}
```

---

## 本节总结

### 核心概念

1. **配置分层**：配置文件 → 环境变量 → 命令行
2. **外部化**：配置与代码分离
3. **动态调整**：无需重启即可修改参数

### 最佳实践

- ✅ 敏感信息使用环境变量
- ✅ 提供合理的默认值
- ✅ 配置变更审计日志

---

## 📝 课后练习

### 练习：配置热加载
实现配置文件变更后自动重载。

### 思考题
1. 热加载时如何处理正在进行的请求？
2. 分布式系统的配置一致性如何保证？

---

## 📖 扩展阅读

- **配置中心**：Consul、Etcd、Apollo
- **配置加密**：Vault、AWS Secrets Manager
- **GitOps**：配置即代码
