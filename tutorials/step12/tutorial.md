# Step 12: 配置管理 - YAML/JSON 配置与热加载

> 目标：实现外部化配置，支持运行时调整
> 
> 难度：⭐⭐⭐⭐ (较难)
> 
> 代码量：约 900 行
> 
> 预计学习时间：4-5 小时

---

## 🎯 Agent 开发知识点

**本节核心问题：** 如何让 Agent 行为可配置，无需重启即可调整？

**Agent 中的配置场景：**
```
LLM 参数：温度、最大 token、超时时间
工具配置：API key、调用限制
业务规则：响应模板、敏感词过滤
运行时开关：功能开关、日志级别
```

---

## 📚 理论基础 + 代码实现

### 1. 配置类设计

**理论：** 分层配置（文件 → 环境变量 → 命令行）

```cpp
// config.hpp
class Config {
public:
    // 加载 YAML
    bool load_yaml(const std::string& path) {
        try {
            pt::ptree tree;
            pt::read_yaml(path, tree);
            merge_tree(tree);
            return true;
        } catch (const std::exception& e) {
            std::cerr << "加载失败: " << e.what() << std::endl;
            return false;
        }
    }
    
    // 加载环境变量
    void load_env(const std::string& prefix = "NUCLAW_") {
        extern char** environ;
        for (char** env = environ; *env; ++env) {
            std::string env_str(*env);
            size_t pos = env_str.find('=');
            if (pos == std::string::npos) continue;
            
            std::string key = env_str.substr(0, pos);
            std::string value = env_str.substr(pos + 1);
            
            if (key.find(prefix) == 0) {
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
        auto it = data_.find(key);
        return (it != data_.end()) ? it->second : default_val;
    }
    
    int get_int(const std::string& key, int default_val = 0) const {
        std::string val = get_string(key, "");
        if (val.empty()) return default_val;
        try {
            return std::stoi(val);
        } catch (...) {
            return default_val;
        }
    }
    
    void set(const std::string& key, const std::string& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        data_[key] = value;
    }

private:
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
};
```

### 2. Agent 配置示例

```yaml
# config.yaml

# LLM 配置
llm:
  provider: "openai"
  api_key: "${OPENAI_API_KEY}"  # 从环境变量读取
  model: "gpt-4"
  timeout: 30
  temperature: 0.7
  max_tokens: 2000

# Agent 行为配置
agent:
  welcome_message: "你好！我是 AI 助手"
  max_history: 10
  fallback_enabled: true

# 工具配置
tools:
  weather:
    api_key: "${WEATHER_API_KEY}"
    default_city: "北京"
  
  calculator:
    max_precision: 10

# 日志配置
logging:
  level: "info"
  format: "json"
```

### 3. 集成到 ChatEngine

```cpp
// config_manager.hpp
class ConfigManager {
public:
    bool load(const std::string& path) {
        // 1. 加载配置文件
        if (!config_.load_yaml(path)) {
            return false;
        }
        
        // 2. 环境变量覆盖
        config_.load_env("NUCLAW_");
        
        return true;
    }
    
    const Config& get() const { return config_; }
    Config& get() { return config_; }

private:
    Config config_;
};

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
    // 加载配置
    ConfigManager config_mgr;
    if (!config_mgr.load("config.yaml")) {
        std::cerr << "配置加载失败\n";
        return 1;
    }
    
    // 初始化 Agent
    ChatEngine engine;
    engine.init(config_mgr.get());
    
    return 0;
}
```

---

## 📋 Agent 开发检查清单

- [ ] 敏感配置是否使用环境变量？
- [ ] 配置是否有合理的默认值？
- [ ] 配置变更是否需要重启？
- [ ] 配置是否验证合法性？

---

**下一步：** Step 13 监控告警
