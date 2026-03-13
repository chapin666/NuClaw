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
// ❌ 不好的做法 - 硬编码
class LLMClient {
    std::string api_key_ = "sk-abc123";  // 密钥泄露在代码中！
    std::string model_ = "gpt-4";        // 改模型要重新编译！
    int timeout_ = 30;                   // 无法动态调整！
};
```

**实际痛点：**

| 场景 | 问题 | 影响 |
|:---|:---|:---|
| **密钥泄露** | API key 在代码中 | 安全风险 |
| **环境切换** | 开发/生产配置不同 | 频繁改代码 |
| **参数调优** | 调整超时时间 | 需重启服务 |
| **团队协作** | 个人配置冲突 | 代码冲突 |

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

### 配置管理的目标

**1. 配置与代码分离**
```
配置 → 配置文件/环境变量
代码 → 读取配置

好处：改配置不碰代码
```

**2. 多环境支持**
```
开发环境：config.dev.yaml
测试环境：config.test.yaml
生产环境：config.prod.yaml
```

**3. 动态调整**
```
运行中修改配置 → 自动生效 → 无需重启
```

### 配置优先级

**优先级从高到低：**

```
1. 命令行参数（最优先）
   --port=8080

2. 环境变量
   export NUCLAW_PORT=8080

3. 配置文件
   port: 8080

4. 代码默认值（最低）
   int port = 8080;
```

**为什么这样设计？**
- 命令行：临时覆盖，一次性
- 环境变量：容器化部署常用
- 配置文件：主要配置方式
- 代码默认值：保底，确保能运行

### 配置文件格式选择

| 格式 | 优点 | 缺点 | 推荐场景 |
|:---|:---|:---|:---|
| **YAML** | 可读性好，支持注释 | 缩进敏感 | 复杂配置 |
| **JSON** | 通用，解析快 | 不支持注释 | 机器交互 |
| **TOML** | 简洁，明确 | 较新 | Rust 项目 |
| **INI** | 简单 | 不支持嵌套 | 简单配置 |

**推荐：** YAML 为主，JSON 为辅

---

## 第一步：配置分层设计

### 配置结构设计

```yaml
# config.yaml - 完整配置示例

# 服务器配置
server:
  host: "0.0.0.0"
  port: 8080
  workers: 4

# LLM 配置
llm:
  provider: "openai"      # openai, anthropic, azure
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

# RAG 配置
rag:
  enabled: true
  embedding_model: "text-embedding-3-small"
  top_k: 3

# 日志配置
logging:
  level: "info"           # debug, info, warn, error
  format: "json"          # text, json
  output: "stdout"        # stdout, file
```

### 配置读取方式

```cpp
// 读取嵌套配置
std::string api_key = config.get_string("llm.api_key");
int timeout = config.get_int("llm.timeout", 30);  // 带默认值
double temp = config.get_double("llm.temperature", 0.7);
bool rag_enabled = config.get_bool("rag.enabled", true);

// 读取数组
std::vector<std::string> tools = config.get_array("tools.enabled");
```

---

## 第二步：配置类实现

### 完整配置管理器代码

```cpp
// config.hpp
#pragma once

#include <string>
#include <map>
#include <vector>
#include <mutex>
#include <fstream>
#include <sstream>
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
    
    // 从环境变量加载（NUCLAW_ 前缀）
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
    
    // ========== 读取配置 ==========
    
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
    
    double get_double(const std::string& key, double default_val = 0.0) const {
        std::string val = get_string(key, "");
        if (val.empty()) return default_val;
        try {
            return std::stod(val);
        } catch (...) {
            return default_val;
        }
    }
    
    bool get_bool(const std::string& key, bool default_val = false) const {
        std::string val = get_string(key, "");
        if (val == "true" || val == "1") return true;
        if (val == "false" || val == "0") return false;
        return default_val;
    }
    
    // 设置配置
    void set(const std::string& key, const std::string& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        data_[key] = value;
    }
    
    // 打印所有配置
    void print_all() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::cout << "=== 当前配置 ===\n";
        for (const auto& [k, v] : data_) {
            std::cout << k << " = " << v << "\n";
        }
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
    std::string config_file_;
};
```

---

## 第三步：热加载机制

### 配置文件监听

```cpp
// config_watcher.hpp
#pragma once
#include "config.hpp"
#include <thread>
#include <atomic>
#include <filesystem>

namespace fs = std::filesystem;

class ConfigWatcher {
public:
    ConfigWatcher(Config& config) : config_(config), running_(false) {}
    
    ~ConfigWatcher() {
        stop();
    }
    
    void watch(const std::string& path, int interval_seconds = 5) {
        if (running_) return;
        
        watch_path_ = path;
        interval_ = interval_seconds;
        last_modified_ = get_modified_time(path);
        running_ = true;
        
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
                
                if (watch_path_.find(".yaml") != std::string::npos) {
                    config_.load_yaml(watch_path_);
                } else {
                    config_.load_json(watch_path_);
                }
                
                last_modified_ = current_modified;
            }
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

## 第四步：集成到 Agent

### 完整示例

```cpp
// main.cpp
#include "config.hpp"
#include "config_watcher.hpp"
#include <iostream>
#include <fstream>

// 创建示例配置文件
void create_config_file() {
    std::ofstream file("config.yaml");
    file << R"(
server:
  host: "0.0.0.0"
  port: 8080

llm:
  model: "gpt-4"
  timeout: 30
  temperature: 0.7

logging:
  level: "info"
  format: "json"
)";
}

int main() {
    std::cout << "========================================\n";
    std::cout << "   NuClaw Step 12: 配置管理\n";
    std::cout << "========================================\n\n";
    
    // 1. 创建示例配置
    create_config_file();
    std::cout << "[1] 创建配置文件 config.yaml\n\n";
    
    // 2. 加载配置
    Config config;
    if (!config.load_yaml("config.yaml")) {
        std::cerr << "加载配置失败\n";
        return 1;
    }
    std::cout << "[2] 加载配置完成\n";
    config.print_all();
    
    // 3. 读取配置
    std::cout << "\n[3] 读取配置项:\n";
    std::cout << "  server.host = " << config.get_string("server.host") << "\n";
    std::cout << "  server.port = " << config.get_int("server.port") << "\n";
    std::cout << "  llm.model = " << config.get_string("llm.model") << "\n";
    std::cout << "  llm.timeout = " << config.get_int("llm.timeout") << "\n";
    
    // 4. 程序内修改
    std::cout << "\n[4] 程序内修改配置\n";
    config.set("app.name", "NuClaw");
    config.set("app.version", "1.0.0");
    
    std::cout << "\n[5] 最终配置:\n";
    config.print_all();
    
    // 清理
    std::remove("config.yaml");
    
    std::cout << "\n========================================\n";
    std::cout << "配置管理演示完成！\n";
    std::cout << "========================================\n";
    
    return 0;
}
```

---

## 本节总结

### 核心概念

1. **配置分层**：配置文件 → 环境变量 → 命令行参数
2. **外部化**：配置与代码分离
3. **热加载**：运行中更新配置，无需重启

### 配置管理流程

```
开发阶段：编写 config.yaml 模板
部署阶段：填充环境特定值
运行阶段：监听变更，热加载
```

### 最佳实践

- ✅ 敏感信息使用环境变量
- ✅ 提供合理的默认值
- ✅ 配置变更审计日志
- ✅ 敏感配置加密存储

---

## 📝 课后练习

### 练习 1：配置验证
实现配置 Schema 验证，检查必需的配置项。

### 练习 2：配置加密
敏感配置（如 API key）加密存储。

### 练习 3：远程配置
从配置中心（如 Consul、Etcd）加载配置。

### 思考题
1. 热加载时如何处理正在进行的请求？
2. 分布式系统的配置一致性如何保证？
3. 配置变更如何通知所有相关节点？

---

**下一步：** Step 13 监控告警
