# Step 12: 配置管理 - YAML/JSON 配置与热加载

> 目标：实现外部化配置，支持热加载，无需重启修改参数
> 
> 难度：⭐⭐⭐⭐ (较难)
> 
> 代码量：约 900 行
> 
> 预计学习时间：4-5 小时

---

## 📚 前置知识

### 为什么需要配置管理？

**硬编码配置的问题：**

```cpp
// 不好的做法 - 硬编码
class LLMClient {
    std::string api_key_ = "sk-xxx";  // 硬编码密钥！
    std::string model_ = "gpt-4";      // 硬编码模型！
    int timeout_ = 30;                 // 硬编码超时！
};
```

**问题：**
1. **安全风险**：密钥泄露在代码中
2. **环境差异**：开发/测试/生产环境需要不同配置
3. **修改困难**：每次改配置都要重新编译
4. **无法动态调整**：运行中不能修改参数

### 配置管理的核心需求

| 需求 | 说明 | 实现方式 |
|:---|:---|:---|
| **外部化** | 配置与代码分离 | YAML/JSON/环境变量 |
| **分层** | 不同环境不同配置 | 配置文件覆盖机制 |
| **热加载** | 运行中更新配置 | 文件监听 + 配置重载 |
| **类型安全** | 配置值类型检查 | Schema 验证 |
| **默认值** | 配置缺失时使用默认值 | 代码中设置 fallback |

### 配置文件格式对比

| 格式 | 优点 | 缺点 | 适用场景 |
|:---|:---|:---|:---|
| **YAML** | 可读性好，支持注释 | 解析稍慢，缩进敏感 | 复杂配置首选 |
| **JSON** | 通用，解析快 | 不支持注释，冗长 | 机器交互、简单配置 |
| **TOML** | 简洁，明确 | 相对较新 | Rust 项目常用 |
| **INI** | 简单 | 不支持嵌套 | 简单配置 |

---

## 第一步：配置类设计

### 配置管理器实现

```cpp
// config.hpp
#pragma once
#include <string>
#include <map>
#include <vector>
#include <any>
#include <mutex>
#include <functional>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/yaml_parser.hpp>

namespace pt = boost::property_tree;

class Config {
public:
    // 从 YAML 文件加载
    bool load_yaml(const std::string& path) {
        try {
            pt::ptree tree;
            pt::read_yaml(path, tree);
            merge_tree(tree);
            config_file_ = path;
            return true;
        } catch (const std::exception& e) {
            std::cerr << "加载 YAML 失败: " << e.what() << std::endl;
            return false;
        }
    }
    
    // 从 JSON 文件加载
    bool load_json(const std::string& path) {
        try {
            pt::ptree tree;
            pt::read_json(path, tree);
            merge_tree(tree);
            config_file_ = path;
            return true;
        } catch (const std::exception& e) {
            std::cerr << "加载 JSON 失败: " << e.what() << std::endl;
            return false;
        }
    }
    
    // 从环境变量加载（覆盖已有配置）
    void load_env(const std::string& prefix = "NUCLAW_") {
        extern char** environ;
        for (char** env = environ; *env; ++env) {
            std::string env_str(*env);
            size_t pos = env_str.find('=');
            if (pos == std::string::npos) continue;
            
            std::string key = env_str.substr(0, pos);
            std::string value = env_str.substr(pos + 1);
            
            if (key.find(prefix) == 0) {
                // NUCLAW_LLM_API_KEY → llm.api_key
                std::string config_key = key.substr(prefix.length());
                std::transform(config_key.begin(), config_key.end(), 
                              config_key.begin(), ::tolower);
                std::replace(config_key.begin(), config_key.end(), '_', '.');
                
                set(config_key, value);
            }
        }
    }
    
    // 读取配置（带默认值）
    std::string get_string(const std::string& key, 
                           const std::string& default_val = "") const {
        std::lock_guard<std::mutex> lock(mutex_);
        return get_value(key, default_val);
    }
    
    int get_int(const std::string& key, int default_val = 0) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return get_value(key, default_val);
    }
    
    double get_double(const std::string& key, double default_val = 0.0) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return get_value(key, default_val);
    }
    
    bool get_bool(const std::string& key, bool default_val = false) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return get_value(key, default_val);
    }
    
    // 设置配置
    void set(const std::string& key, const std::string& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        data_[key] = value;
    }

private:
    template<typename T>
    T get_value(const std::string& key, const T& default_val) const {
        auto it = data_.find(key);
        if (it == data_.end()) {
            return default_val;
        }
        
        try {
            if constexpr (std::is_same_v<T, int>) {
                return std::stoi(it->second);
            } else if constexpr (std::is_same_v<T, double>) {
                return std::stod(it->second);
            } else if constexpr (std::is_same_v<T, bool>) {
                return it->second == "true" || it->second == "1";
            } else {
                return it->second;
            }
        } catch (...) {
            return default_val;
        }
    }
    
    void merge_tree(const pt::ptree& tree, const std::string& prefix = "") {
        for (const auto& [key, value] : tree) {
            std::string full_key = prefix.empty() ? key : prefix + "." + key;
            
            if (value.empty()) {
                data_[full_key] = value.get_value<std::string>();
            } else {
                merge_tree(value, full_key);
            }
        }
    }
    
    mutable std::mutex mutex_;
    std::map<std::string, std::string> data_;
    std::string config_file_;
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
  workers: 4

# LLM 配置
llm:
  provider: "openai"  # openai, anthropic, local
  api_key: "${OPENAI_API_KEY}"  # 从环境变量读取
  model: "gpt-4"
  timeout: 30
  max_tokens: 2000
  temperature: 0.7

# 工具配置
tools:
  enabled:
    - weather
    - time
    - calculator
  
  weather:
    api_key: "${WEATHER_API_KEY}"
    default_city: "北京"

# 日志配置
logging:
  level: "info"  # debug, info, warn, error
  format: "json"  # text, json
  output: "stdout"  # stdout, file
```

---

## 第三步：集成到 Agent

### ChatEngine 配置化

```cpp
// chat_engine.hpp
class ChatEngine {
public:
    void init(const Config& config) {
        // 从配置初始化 LLM
        llm_.set_api_key(config.get_string("llm.api_key"));
        llm_.set_model(config.get_string("llm.model", "gpt-3.5-turbo"));
        llm_.set_timeout(config.get_int("llm.timeout", 30));
        
        // 从配置初始化行为
        welcome_msg_ = config.get_string("agent.welcome_message");
        max_history_ = config.get_int("agent.max_history", 10);
    }

private:
    LLMClient llm_;
    std::string welcome_msg_;
    int max_history_;
};

// main.cpp
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

1. **配置分层**：配置文件 → 环境变量 → 命令行参数
2. **外部化**：配置与代码分离
3. **动态调整**：运行中更新配置，无需重启

### 配置优先级

```
1. 命令行参数（最高优先级）
2. 环境变量
3. 配置文件
4. 代码默认值（最低优先级）
```

---

## 📝 课后练习

### 练习：配置热加载
实现配置文件变更后自动重载。

### 思考题
1. 热加载时如何处理正在进行的请求？
2. 分布式系统的配置一致性如何保证？

---

**下一步：** Step 13 监控告警
