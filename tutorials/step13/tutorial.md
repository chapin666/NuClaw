# Step 13: 监控告警 - Metrics, Logging, Tracing

> 目标：实现可观测性三大支柱，掌握生产环境监控
> 
> 难度：⭐⭐⭐⭐⭐ (困难)
> 
> 代码量：约 950 行
> 
> 预计学习时间：5-6 小时

---

## 🏗️ 工程化目录结构

**延续工程化结构，新增可观测性模块：**

```
src/step13/
├── CMakeLists.txt          # 构建配置
├── configs/
│   └── config.yaml         # 配置（新增监控配置项）
├── include/nuclaw/         # 头文件目录
│   ├── metrics.hpp         # ★ 新增：指标监控
│   ├── logger.hpp          # ★ 新增：日志系统
│   ├── config.hpp          # 从 Step 12 继承
│   ├── agent.hpp           # 从 Step 12 继承
│   └── ...                 # 其他继承文件
├── src/
│   └── main.cpp            # 修改：集成监控埋点
└── tests/
```

**Step 13 新增文件说明：**
- `metrics.hpp` - 指标收集（Counter、Gauge、Histogram）
- `logger.hpp` - 结构化日志（JSON 格式）

**代码演进：**
```cpp
// Step 12: 普通 Agent
class ChatEngine {
    std::string process(const std::string& input) {
        return do_process(input);
    }
};

// Step 13: 带监控的 Agent
class MonitoredChatEngine {
    std::string process(const std::string& input) {
        METRICS_COUNTER("requests_total")->increment();
        LOG_INFO("Processing request: " + input);
        
        auto start = std::chrono::steady_clock::now();
        std::string reply = do_process(input);
        
        auto elapsed = std::chrono::steady_clock::now() - start;
        METRICS_GAUGE("latency_ms")->set(elapsed.count());
        
        return reply;
    }
};
```

---

## 📚 前置知识

### 为什么需要可观测性？

**生产环境的挑战：**

**场景 1：用户反馈服务慢**
```
问题：哪里慢？为什么慢？
没有监控：只能猜
有监控：看响应时间指标，定位瓶颈
```

**场景 2：服务突然崩溃**
```
问题：什么时候开始的？什么原因？
没有监控：只能翻日志
有监控：看错误率曲线，结合日志分析
```

**场景 3：流量激增**
```
问题：当前负载如何？需要扩容吗？
没有监控：只能凭感觉
有监控：看 QPS、CPU、内存指标
```

### 可观测性三大支柱

```
┌─────────────────────────────────────────────────────────────┐
│                    可观测性三大支柱                          │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐        │
│  │   Metrics   │  │   Logging   │  │  Tracing    │        │
│  │   (指标)    │  │   (日志)    │  │  (链路追踪)  │        │
│  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘        │
│         │                │                │               │
│         ▼                ▼                ▼               │
│    "系统的脉搏"      "系统的记忆"      "系统的足迹"        │
│                                                             │
│  • CPU/内存/请求数   • 错误日志        • 请求链路          │
│  • 响应时间/成功率   • 访问日志        • 服务依赖          │
│  • 业务指标          • 调试日志        • 性能瓶颈          │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 类比：体检报告

```
Metrics = 血压、心率（量化指标）
Logging = 病历记录（事件记录）
Tracing = 检查流程（步骤追踪）
```

---

## 第一步：Metrics 指标监控

### 指标类型

| 类型 | 特点 | 示例 |
|:---|:---|:---|
| **Counter** | 只增不减 | 请求总数、错误总数 |
| **Gauge** | 可增可减 | 当前连接数、内存使用 |
| **Histogram** | 分布统计 | 响应时间分布 |

### Metrics 实现

```cpp
// metrics.hpp
#pragma once

#include <string>
#include <map>
#include <math>
#include <mutex>
#include <sstream>
#include <atomic>

// Counter 计数器
class Counter {
public:
    void increment(double value = 1.0) {
        value_.fetch_add(value);
    }
    
    double get() const {
        return value_.load();
    }

private:
    std::atomic<double> value_{0};
};

// Gauge 仪表盘
class Gauge {
public:
    void set(double value) {
        value_.store(value);
    }
    
    void increment(double value = 1.0) {
        value_.fetch_add(value);
    }
    
    void decrement(double value = 1.0) {
        value_.fetch_sub(value);
    }
    
    double get() const {
        return value_.load();
    }

private:
    std::atomic<double> value_{0};
};

// Metrics 管理器
class Metrics {
public:
    static Metrics& instance() {
        static Metrics instance;
        return instance;
    }
    
    Counter* counter(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (counters_.find(name) == counters_.end()) {
            counters_[name] = std::make_unique<Counter>();
        }
        return counters_[name].get();
    }
    
    Gauge* gauge(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (gauges_.find(name) == gauges_.end()) {
            gauges_[name] = std::make_unique<Gauge>();
        }
        return gauges_[name].get();
    }
    
    // Prometheus 格式导出
    std::string export_prometheus() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::stringstream ss;
        
        for (const auto& [name, c] : counters_) {
            ss << "# TYPE " << name << " counter\n";
            ss << name << " " << c->get() << "\n";
        }
        
        for (const auto& [name, g] : gauges_) {
            ss << "# TYPE " << name << " gauge\n";
            ss << name << " " << g->get() << "\n";
        }
        
        return ss.str();
    }
    
    void print_all() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::cout << "=== Metrics ===\n";
        for (const auto& [name, c] : counters_) {
            std::cout << name << ": " << c->get() << "\n";
        }
        for (const auto& [name, g] : gauges_) {
            std::cout << name << ": " << g->get() << "\n";
        }
    }

private:
    mutable std::mutex mutex_;
    std::map<std::string, std::unique_ptr<Counter>> counters_;
    std::map<std::string, std::unique_ptr<Gauge>> gauges_;
};

#define METRICS_COUNTER(name) Metrics::instance().counter(name)
#define METRICS_GAUGE(name) Metrics::instance().gauge(name)
```

### 💡 理论知识：监控指标设计原则

**USE 方法（Brendan Gregg）：**
```
U - Utilization（使用率）：资源有多忙？
    例：CPU 使用率、内存使用率
    
S - Saturation（饱和度）：有多少工作在排队？
    例：队列长度、等待时间
    
E - Errors（错误）：有多少错误发生？
    例：请求失败率、异常数量
```

**RED 方法（适用于服务）：**
```
R - Rate（请求率）：每秒请求数
E - Errors（错误率）：错误请求占比
D - Duration（延迟）：请求处理时间
```

**指标命名规范：**
```
{subsystem}_{metric}_{unit}

✅ 好的命名：
   llm_requests_total        # 计数器，只增不减
   llm_request_duration_ms   # 延迟，单位明确
   rag_documents_indexed     # 业务指标，语义清晰

❌ 差的命名：
   request_count             # 缺少子系统前缀
   latency                   # 缺少单位
   num_docs                  # 缩写不清晰
```

### 在 Agent 中埋点

```cpp
// monitored_chat_engine.hpp
class MonitoredChatEngine {
public:
    MonitoredChatEngine() {
        // 初始化指标
        request_counter_ = METRICS_COUNTER("agent_requests_total");
        request_duration_ = METRICS_COUNTER("agent_request_duration_ms");
        active_requests_ = METRICS_GAUGE("agent_active_requests");
        error_counter_ = METRICS_COUNTER("agent_errors_total");
    }
    
    std::string process(const std::string& input) {
        METRICS_GAUGE("agent_active_requests")->increment();
        
        LOG_INFO("开始处理请求");
        
        auto start = std::chrono::steady_clock::now();
        
        try {
            std::string reply = do_process(input);
            
            auto elapsed = std::chrono::steady_clock::now() - start;
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed);
            request_duration_>increment(ms.count());
            
            LOG_INFO("请求处理成功，耗时: " + std::to_string(ms.count()) + "ms");
            
            return reply;
        } catch (const std::exception& e) {
            error_counter_>increment();
            LOG_ERROR("请求处理失败: " + std::string(e.what()));
            throw;
        }
        
        active_requests_>decrement();
    }

private:
    Counter* request_counter_;
    Counter* request_duration_;
    Counter* error_counter_;
    Gauge* active_requests_;
};
```

---

## 第二步：Logging 日志系统

### 日志级别

```
DEBUG   - 调试信息，开发时使用
INFO    - 正常操作记录
WARN    - 警告，可能的问题
ERROR   - 错误，需要处理
FATAL   - 致命错误，程序退出
```

### 结构化日志实现

```cpp
// logger.hpp
#pragma once

#include <string>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <mutex>
#include <chrono>

enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARN = 2,
    ERROR = 3,
    FATAL = 4
};

inline std::string level_to_string(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO";
        case LogLevel::WARN:  return "WARN";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::FATAL: return "FATAL";
        default: return "UNKNOWN";
    }
}

class Logger {
public:
    static Logger& instance() {
        static Logger instance;
        return instance;
    }
    
    void set_level(LogLevel level) {
        min_level_ = level;
    }
    
    void log(LogLevel level, const std::string& message,
             const std::string& file = "", int line = 0) {
        if (level < min_level_) return;
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        
        std::cout << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S");
        std::cout << " [" << level_to_string(level) << "] ";
        std::cout << message;
        
        if (!file.empty()) {
            std::cout << " (" << file << ":" << line << ")";
        }
        
        std::cout << std::endl;
    }

private:
    Logger() = default;
    LogLevel min_level_ = LogLevel::INFO;
    std::mutex mutex_;
};

#define LOG_DEBUG(msg) Logger::instance().log(LogLevel::DEBUG, msg, __FILE__, __LINE__)
#define LOG_INFO(msg)  Logger::instance().log(LogLevel::INFO, msg, __FILE__, __LINE__)
#define LOG_WARN(msg)  Logger::instance().log(LogLevel::WARN, msg, __FILE__, __LINE__)
#define LOG_ERROR(msg) Logger::instance().log(LogLevel::ERROR, msg, __FILE__, __LINE__)
```

---

## 第三步：监控告警

### 告警规则设计

```cpp
// alert_manager.hpp
class AlertManager {
public:
    void check_metrics() {
        // 错误率告警
        double error_rate = calculate_error_rate();
        if (error_rate > 0.05) {  // 5%
            send_alert("HighErrorRate", "错误率超过 5%: " + std::to_string(error_rate));
        }
        
        // 响应时间告警
        double p99_latency = calculate_p99_latency();
        if (p99_latency > 1000) {  // 1秒
            send_alert("HighLatency", "P99 延迟超过 1s: " + std::to_string(p99_latency));
        }
    }

private:
    void send_alert(const std::string& name, const std::string& message) {
        std::cerr << "[🚨 ALERT] " << name << ": " << message << std::endl;
        // 实际实现：发送邮件、短信、Slack 等
    }
};
```

---

## 本节总结

### 核心概念

1. **Metrics**：量化指标，用于监控和告警
2. **Logging**：事件记录，用于故障排查
3. **Tracing**：请求链路，用于性能分析

### 使用场景

| 问题 | 使用什么 |
|:---|:---|
| 服务是否健康？ | Metrics |
| 为什么出错？ | Logging |
| 哪里慢？ | Tracing |

---

## 📝 课后练习

### 练习 1：自定义指标
为你关注的业务指标（如订单量、用户活跃度）添加监控。

### 练习 2：日志聚合
实现日志发送到远程收集器（如 ELK）。

### 练习 3：告警模板
设计告警通知模板，包含问题描述、影响范围、处理建议。

### 思考题
1. 可观测性和监控有什么区别？
2. 采样策略如何设计？全量采集有什么问题？
3. 如何在性能和可观测性之间平衡？

---

## 📖 扩展阅读

### 可观测性平台

- **Prometheus + Grafana**：Metrics 监控标配
- **ELK Stack**：日志收集与分析
- **Jaeger**：分布式追踪
- **OpenTelemetry**：统一的可观测性标准

### 最佳实践

- **RED 方法**：Rate, Errors, Duration - 服务监控
- **USE 方法**：Utilization, Saturation, Errors - 资源监控
- **SLO/SLI**：服务级别目标/指标

---

**下一步：** Step 14 部署运维
