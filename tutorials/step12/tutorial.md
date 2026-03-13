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

**配置管理的好处：**
```cpp
// 好的做法 - 外部配置
class LLMClient {
    void load_config(const Config& cfg) {
        api_key_ = cfg.get_string("llm.api_key");
        model_ = cfg.get_string("llm.model", "gpt-4");
        timeout_ = cfg.get_int("llm.timeout", 30);
    }
};
```

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

### 配置管理器

```cpp
// config.hpp
#pragma once
#include <string>
#include <map>
#include <vector>
#include <any>
#include <mutex>
#include <functional>
#include <boost/json.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/yaml_parser.hpp>

namespace pt = boost::property_tree;

class Config {
public:
    // ========== 加载配置 ==========
    
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
        // 读取所有以 prefix 开头的环境变量
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
    
    // ========== 读取配置 ==========
    
    // 获取字符串（带默认值）
    std::string get_string(const std::string& key, 
                           const std::string& default_val = "") const {
        std::lock_guard<std::mutex> lock(mutex_);
        return get_value(key, default_val);
    }
    
    // 获取整数
    int get_int(const std::string& key, int default_val = 0) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return get_value(key, default_val);
    }
    
    // 获取浮点数
    double get_double(const std::string& key, double default_val = 0.0) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return get_value(key, default_val);
    }
    
    // 获取布尔值
    bool get_bool(const std::string& key, bool default_val = false) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return get_value(key, default_val);
    }
    
    // 获取字符串数组
    std::vector<std::string> get_string_array(const std::string& key) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> result;
        
        auto range = data_.equal_range(key);
        for (auto it = range.first; it != range.second; ++it) {
            result.push_back(it->second);
        }
        
        return result;
    }
    
    // ========== 设置配置 ==========
    
    void set(const std::string& key, const std::string& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        data_[key] = value;
    }
    
    // ========== 配置监听 ==========
    
    using ConfigChangeCallback = std::function<void(const std::string& key, 
                                                       const std::string& old_val,
                                                       const std::string& new_val)>;
    
    void on_change(ConfigChangeCallback callback) {
        change_callbacks_.push_back(callback);
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
                // 叶子节点
                data_[full_key] = value.get_value<std::string>();
            } else {
                // 递归处理子树
                merge_tree(value, full_key);
            }
        }
    }
    
    mutable std::mutex mutex_;
    std::map<std::string, std::string> data_;
    std::vector<ConfigChangeCallback> change_callbacks_;
    std::string config_file_;
};
```

---

## 第二步：配置文件示例

### YAML 配置

```yaml
# config.yaml
server:
  host: "0.0.0.0"
  port: 8080
  workers: 4

llm:
  provider: "openai"  # openai, anthropic, local
  api_key: "${OPENAI_API_KEY}"  # 从环境变量读取
  model: "gpt-4"
  timeout: 30
  max_tokens: 2000
  temperature: 0.7

tools:
  enabled:
    - weather
    - time
    - calculator
    - http_get
  
  weather:
    api_key: "${WEATHER_API_KEY}"
    default_city: "北京"
  
  http_get:
    timeout: 10
    max_response_size: 1048576  # 1MB
    allowed_hosts:
      - "api.github.com"
      - "api.openweathermap.org"

rag:
  enabled: true
  embedding_model: "text-embedding-3-small"
  vector_db:
    type: "memory"  # memory, milvus, chroma
    dimension: 1536
  top_k: 3

logging:
  level: "info"  # debug, info, warn, error
  format: "json"  # text, json
  output: "stdout"  # stdout, file, both
  file:
    path: "/var/log/nuclaw/app.log"
    max_size: 100  # MB
    max_files: 7

security:
  sandbox:
    enabled: true
    blocked_hosts:
      - "localhost"
      - "127.0.0.1"
    blocked_paths:
      - ".."
      - "/etc"
  rate_limit:
    requests_per_minute: 60
```

### JSON 配置

```json
{
  "server": {
    "host": "0.0.0.0",
    "port": 8080,
    "workers": 4
  },
  "llm": {
    "provider": "openai",
    "api_key": "${OPENAI_API_KEY}",
    "model": "gpt-4",
    "timeout": 30
  },
  "tools": {
    "enabled": ["weather", "time", "calculator"]
  }
}
```

---

## 第三步：热加载实现

### 文件监听器

```cpp
// config_watcher.hpp
#pragma once
#include "config.hpp"
#include <thread>
#include <atomic>
#include <sys/inotify.h>
#include <unistd.h>

class ConfigWatcher {
public:
    ConfigWatcher(Config& config) : config_(config), running_(false) {}
    
    ~ConfigWatcher() {
        stop();
    }
    
    // 开始监视配置文件
    void watch(const std::string& path) {
        if (running_) return;
        
        watch_path_ = path;
        running_ = true;
        
        watcher_thread_ = std::thread([this]() {
            run_watcher();
        });
    }
    
    // 停止监视
    void stop() {
        running_ = false;
        if (watcher_thread_.joinable()) {
            watcher_thread_.join();
        }
    }

private:
    void run_watcher() {
        // 创建 inotify 实例
        int fd = inotify_init();
        if (fd < 0) {
            std::cerr << "inotify_init 失败" << std::endl;
            return;
        }
        
        // 添加监视
        int wd = inotify_add_watch(fd, watch_path_.c_str(), IN_MODIFY);
        if (wd < 0) {
            std::cerr << "inotify_add_watch 失败" << std::endl;
            close(fd);
            return;
        }
        
        char buffer[1024];
        
        while (running_) {
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(fd, &fds);
            
            struct timeval tv;
            tv.tv_sec = 1;  // 1 秒超时
            tv.tv_usec = 0;
            
            int ret = select(fd + 1, &fds, nullptr, nullptr, &tv);
            
            if (ret > 0 && FD_ISSET(fd, &fds)) {
                ssize_t len = read(fd, buffer, sizeof(buffer));
                if (len > 0) {
                    std::cout << "[♻️] 配置文件变更，重新加载..." << std::endl;
                    
                    // 重新加载配置
                    if (watch_path_.find(".yaml") != std::string::npos ||
                        watch_path_.find(".yml") != std::string::npos) {
                        config_.load_yaml(watch_path_);
                    } else {
                        config_.load_json(watch_path_);
                    }
                    
                    // 触发回调
                    // config_.notify_reload();
                }
            }
        }
        
        inotify_rm_watch(fd, wd);
        close(fd);
    }
    
    Config& config_;
    std::atomic<bool> running_;
    std::string watch_path_;
    std::thread watcher_thread_;
};
```

### 轮询方式（跨平台）

```cpp
// config_watcher_poll.hpp
#pragma once
#include "config.hpp"
#include <filesystem>
#include <thread>
#include <atomic>

namespace fs = std::filesystem;

class ConfigWatcherPoll {
public:
    ConfigWatcherPoll(Config& config) : config_(config), running_(false) {}
    
    ~ConfigWatcherPoll() {
        stop();
    }
    
    void watch(const std::string& path, int interval_seconds = 5) {
        if (running_) return;
        
        watch_path_ = path;
        interval_ = interval_seconds;
        running_ = true;
        
        // 记录文件修改时间
        last_modified_ = get_modified_time(path);
        
        watcher_thread_ = std::thread([this]() {
            run_watcher();
        });
    }
    
    void stop() {
        running_ = false;
        if (watcher_thread_.joinable()) {
            watcher_thread_.join();
        }
    }

private:
    void run_watcher() {
        while (running_) {
            std::this_thread::sleep_for(std::chrono::seconds(interval_));
            
            if (!running_) break;
            
            auto current_modified = get_modified_time(watch_path_);
            
            if (current_modified > last_modified_) {
                std::cout << "[♻️] 配置文件变更，重新加载..." << std::endl;
                
                // 重新加载
                reload_config();
                
                last_modified_ = current_modified;
            }
        }
    }
    
    void reload_config() {
        if (watch_path_.find(".yaml") != std::string::npos ||
            watch_path_.find(".yml") != std::string::npos) {
            config_.load_yaml(watch_path_);
        } else {
            config_.load_json(watch_path_);
        }
    }
    
    std::time_t get_modified_time(const std::string& path) {
        try {
            return fs::last_write_time(path);
        } catch (...) {
            return 0;
        }
    }
    
    Config& config_;
    std::atomic<bool> running_;
    std::string watch_path_;
    int interval_;
    std::time_t last_modified_;
    std::thread watcher_thread_;
};
```

---

## 第四步：组件集成配置

### LLMClient 配置化

```cpp
// llm_client.hpp
#pragma once
#include "config.hpp"

class LLMClient {
public:
    void init(const Config& config) {
        api_key_ = config.get_string("llm.api_key");
        model_ = config.get_string("llm.model", "gpt-3.5-turbo");
        timeout_ = config.get_int("llm.timeout", 30);
        max_tokens_ = config.get_int("llm.max_tokens", 2000);
        temperature_ = config.get_double("llm.temperature", 0.7);
        
        if (api_key_.empty()) {
            throw std::runtime_error("LLM API key 未配置");
        }
        
        std::cout << "[LLM] 配置加载完成: model=" << model_ << std::endl;
    }
    
    // 配置变更时更新
    void on_config_change(const std::string& key, const std::string& value) {
        if (key == "llm.model") {
            model_ = value;
            std::cout << "[LLM] 模型已更新: " << model_ << std::endl;
        } else if (key == "llm.timeout") {
            timeout_ = std::stoi(value);
            std::cout << "[LLM] 超时已更新: " << timeout_ << std::endl;
        }
    }

private:
    std::string api_key_;
    std::string model_;
    int timeout_;
    int max_tokens_;
    double temperature_;
};
```

### Server 配置化

```cpp
// server.hpp
#pragma once
#include "config.hpp"

class Server {
public:
    void init(const Config& config) {
        host_ = config.get_string("server.host", "0.0.0.0");
        port_ = config.get_int("server.port", 8080);
        workers_ = config.get_int("server.workers", 4);
        
        std::cout << "[Server] 配置: " << host_ << ":" << port_ << std::endl;
    }
    
    void start() {
        // 根据配置启动服务器
        // ...
    }

private:
    std::string host_;
    int port_;
    int workers_;
};
```

---

## 第五步：主程序集成

### 应用启动流程

```cpp
// main.cpp
#include "config.hpp"
#include "config_watcher_poll.hpp"
#include "llm_client.hpp"
#include "server.hpp"
#include "tool_registry.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    // 1. 解析命令行参数
    std::string config_path = "config.yaml";
    if (argc > 1) {
        config_path = argv[1];
    }
    
    // 2. 加载配置
    Config config;
    
    // 2.1 加载配置文件
    if (!config.load_yaml(config_path)) {
        std::cerr << "加载配置文件失败: " << config_path << std::endl;
        return 1;
    }
    
    // 2.2 环境变量覆盖（如 NUCLAW_LLM_API_KEY）
    config.load_env("NUCLAW_");
    
    // 2.3 命令行参数覆盖
    // ...
    
    std::cout << "[✓] 配置加载完成" << std::endl;
    
    // 3. 初始化组件
    LLMClient llm;
    llm.init(config);
    
    Server server;
    server.init(config);
    
    // 4. 启动配置热加载
    ConfigWatcherPoll watcher(config);
    watcher.watch(config_path, 5);  // 每 5 秒检查一次
    
    // 5. 注册配置变更回调
    config.on_change([&llm](const std::string& key, 
                             const std::string& old_val,
                             const std::string& new_val) {
        llm.on_config_change(key, new_val);
    });
    
    // 6. 启动服务器
    server.start();
    
    return 0;
}
```

---

## 本节总结

### 核心概念

1. **配置分层**：配置文件 → 环境变量 → 命令行参数
2. **热加载**：文件监听 + 配置重载，无需重启
3. **类型安全**：配置值类型转换和验证
4. **变更通知**：配置更新时通知相关组件

### 配置优先级（从高到低）

```
1. 命令行参数（最高优先级）
2. 环境变量
3. 配置文件
4. 代码默认值（最低优先级）
```

### 代码演进

```
Step 11: 多 Agent (850行)
   ↓ + 配置管理
Step 12: 900行
   + config.hpp: 配置管理器
   + config_watcher.hpp: 热加载
   ~ 各组件: 支持配置化初始化
```

---

## 📝 课后练习

### 练习 1：配置验证
添加配置 Schema 验证：
```cpp
bool validate_config(const Config& config) {
    // 检查必需的配置项
    // 检查配置值范围
}
```

### 练习 2：配置加密
敏感配置（如 API key）加密存储：
```cpp
std::string decrypt_value(const std::string& encrypted);
```

### 练习 3：远程配置
从配置中心（如 Consul、Etcd）加载配置：
```cpp
class RemoteConfigLoader {
    Config load_from_consul(const std::string& addr);
};
```

### 思考题
1. 配置热加载时如何处理正在进行的请求？
2. 如何保证配置的一致性（多个节点）？
3. 敏感配置如何安全地管理和分发？

---

## 📖 扩展阅读

### 配置管理工具

- **Consul**：HashiCorp 的配置中心
- **Etcd**：CoreOS 的分布式键值存储
- **Spring Cloud Config**：Java 生态的配置管理

### 配置最佳实践

- **12-Factor App**：配置管理原则
- **GitOps**：配置即代码，版本化管理

---

**恭喜！** 你的 Agent 现在支持配置管理和热加载了。下一章我们将添加监控和告警功能。
