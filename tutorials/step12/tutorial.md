# Step 12: 配置管理 —— 让系统可配置

> 目标：实现 YAML/JSON 配置管理，支持热加载
> 
003e 难度：⭐⭐⭐ | 代码量：约 900 行 | 预计学习时间：2-3 小时

---

## 一、为什么需要配置管理？

### 1.1 硬编码的问题

```cpp
// 硬代码的配置
const std::string API_KEY = "sk-xxxxxxxxxxxx";  // 泄露风险！
const int PORT = 8080;                           // 无法动态调整
const std::string DB_PATH = "/app/data/db.sqlite"; // 环境差异

// 问题：
// 1. 修改配置需要重新编译
// 2. 不同环境（开发/测试/生产）需要不同代码分支
// 3. 敏感信息容易泄露到代码仓库
// 4. 无法运行时调整
```

### 1.2 配置管理的目标

- **外部化**：配置与代码分离
- **环境适配**：不同环境不同配置
- **热加载**：不重启更新配置
- **敏感保护**：API Key 等加密存储

---

## 二、配置设计

### 2.1 配置分层

```yaml
# config.yaml
server:
  host: "0.0.0.0"
  port: 8080
  workers: 4

llm:
  provider: "openai"
  model: "gpt-4"
  api_key: "${OPENAI_API_KEY}"  # 从环境变量读取
  temperature: 0.7
  max_tokens: 2000

tools:
  - name: "get_weather"
    type: "weather"
    config:
      api_key: "${WEATHER_API_KEY}"
      timeout_ms: 5000

  - name: "calculate"
    type: "calculator"
    
security:
  rate_limit: 100  # 每分钟请求数
  max_request_size: 1048576  # 1MB
  allowed_origins:
    - "http://localhost:3000"
    - "https://myapp.com"

logging:
  level: "info"  # debug/info/warn/error
  output: "stdout"  # stdout/file
  file_path: "/var/log/nuclaw.log"
```

### 2.2 配置类设计

```cpp
class Config {
public:
    struct ServerConfig {
        std::string host = "0.0.0.0";
        int port = 8080;
        int workers = 4;
    };
    
    struct LLMConfig {
        std::string provider = "openai";
        std::string model = "gpt-4";
        std::string api_key;
        float temperature = 0.7;
        int max_tokens = 2000;
    };
    
    struct ToolConfig {
        std::string name;
        std::string type;
        json config;
    };
    
    struct SecurityConfig {
        int rate_limit = 100;
        size_t max_request_size = 1024 * 1024;
        std::vector<std::string> allowed_origins;
    };
    
    struct LoggingConfig {
        std::string level = "info";
        std::string output = "stdout";
        std::string file_path;
    };
    
    // 加载配置
    static Config load(const std::string& path);
    
    // 从 YAML/JSON 解析
    void parse_yaml(const std::string& content);
    void parse_json(const std::string& content);
    
    // 获取配置值
    ServerConfig server;
    LLMConfig llm;
    std::vector<ToolConfig> tools;
    SecurityConfig security;
    LoggingConfig logging;

private:
    // 替换环境变量 ${VAR} → 实际值
    std::string expand_env_vars(const std::string& value);
};
```

---

## 三、配置加载实现

```cpp
Config Config::load(const std::string& path) {
    Config config;
    
    // 读取文件
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open config file: " + path);
    }
    
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    
    // 根据扩展名选择解析器
    if (path.ends_with(".yaml") || path.ends_with(".yml")) {
        config.parse_yaml(content);
    } else if (path.ends_with(".json")) {
        config.parse_json(content);
    } else {
        throw std::runtime_error("Unknown config format: " + path);
    }
    
    // 验证必需配置
    config.validate();
    
    return config;
}

void Config::parse_yaml(const std::string& content) {
    // 使用 yaml-cpp 解析
    YAML::Node root = YAML::Load(content);
    
    // 解析 server 段
    if (root["server"]) {
        auto server_node = root["server"];
        server.host = server_node["host"].as<std::string>(server.host);
        server.port = server_node["port"].as<int>(server.port);
        server.workers = server_node["workers"].as<int>(server.workers);
    }
    
    // 解析 llm 段
    if (root["llm"]) {
        auto llm_node = root["llm"];
        llm.provider = llm_node["provider"].as<std::string>(llm.provider);
        llm.model = llm_node["model"].as<std::string>(llm.model);
        llm.api_key = expand_env_vars(
            llm_node["api_key"].as<std::string>("")
        );
        llm.temperature = llm_node["temperature"].as<float>(llm.temperature);
        llm.max_tokens = llm_node["max_tokens"].as<int>(llm.max_tokens);
    }
    
    // 解析 tools 段
    if (root["tools"] && root["tools"].IsSequence()) {
        for (const auto& tool_node : root["tools"]) {
            ToolConfig tool;
            tool.name = tool_node["name"].as<std::string>();
            tool.type = tool_node["type"].as<std::string>();
            if (tool_node["config"]) {
                tool.config = yaml_to_json(tool_node["config"]);
            }
            tools.push_back(tool);
        }
    }
    
    // 解析其他段...
}

std::string Config::expand_env_vars(const std::string& value) {
    std::string result = value;
    size_t pos = 0;
    
    while ((pos = result.find("${", pos)) != std::string::npos) {
        size_t end = result.find("}", pos);
        if (end == std::string::npos) break;
        
        std::string var_name = result.substr(pos + 2, end - pos - 2);
        const char* var_value = std::getenv(var_name.c_str());
        
        if (var_value) {
            result.replace(pos, end - pos + 1, var_value);
        } else {
            throw std::runtime_error("Environment variable not found: " + var_name);
        }
        
        pos += strlen(var_value);
    }
    
    return result;
}
```

---

## 四、热加载实现

```cpp
class ConfigWatcher {
public:
    ConfigWatcher(asio::io_context& io, const std::string& config_path)
        : io_(io), config_path_(config_path) {
        
        // 记录文件修改时间
        last_modified_ = get_last_modified();
        
        // 启动定时检查
        timer_ = std::make_unique<asio::steady_timer>(io_);
        schedule_check();
    }
    
    void on_config_changed(std::function<void(const Config&)> callback) {
        callback_ = callback;
    }

private:
    void schedule_check() {
        timer_>expires_after(std::chrono::seconds(5));
        timer_>async_wait([this](error_code ec) {
            if (ec) return;
            
            check_for_changes();
            schedule_check();
        });
    }
    
    void check_for_changes() {
        auto current_modified = get_last_modified();
        
        if (current_modified > last_modified_) {
            last_modified_ = current_modified;
            
            try {
                Config new_config = Config::load(config_path_);
                if (callback_) {
                    callback_(new_config);
                }
            } catch (const std::exception& e) {
                std::cerr << "Failed to reload config: " << e.what() << "\n";
            }
        }
    }
    
    std::chrono::system_clock::time_point get_last_modified() {
        namespace fs = std::filesystem;
        return fs::last_write_time(config_path_);
    }

    asio::io_context& io_;
    std::string config_path_;
    std::unique_ptr<asio::steady_timer> timer_;
    std::chrono::system_clock::time_point last_modified_;
    std::function<void(const Config&)> callback_;
};
```

---

## 五、本章小结

**核心收获：**

1. **配置分层**：服务器、LLM、工具、安全、日志分开配置
2. **环境变量**：敏感信息通过环境变量注入
3. **热加载**：定时检查文件变化，动态更新配置

---

## 六、引出的问题

配置可以动态调整了，但系统运行状态不可见。

**下一章（Step 13）：监控告警**
