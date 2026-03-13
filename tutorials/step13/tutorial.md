# Step 13: 监控告警 - Metrics, Logging, Tracing

> 目标：实现可观测性三大支柱，掌握生产环境监控
> 
> 难度：⭐⭐⭐⭐⭐ (困难)
> 
> 代码量：约 950 行
> 
> 预计学习时间：5-6 小时

---

## 📚 前置知识

### 为什么需要可观测性？

**生产环境的挑战：**

```
场景1：用户反馈服务慢
问题：哪里慢？为什么慢？

场景2：服务突然崩溃
问题：什么时候开始的？什么原因？

场景3：流量激增
问题：当前负载如何？需要扩容吗？
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

---

## 第一步：Metrics 指标

### 指标类型

```cpp
// metrics.hpp
class Counter {
public:
    void increment(double value = 1.0) {
        value_.fetch_add(value);
    }
    double get() const { return value_.load(); }
private:
    std::atomic<double> value_{0};
};

class Gauge {
public:
    void set(double value) { value_.store(value); }
    void increment(double v = 1.0) { value_.fetch_add(v); }
    void decrement(double v = 1.0) { value_.fetch_sub(v); }
    double get() const { return value_.load(); }
private:
    std::atomic<double> value_{0};
};

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

private:
    mutable std::mutex mutex_;
    std::map<std::string, std::unique_ptr<Counter>> counters_;
    std::map<std::string, std::unique_ptr<Gauge>> gauges_;
};

#define METRICS_COUNTER(name) Metrics::instance().counter(name)
#define METRICS_GAUGE(name) Metrics::instance().gauge(name)
```

### 在 Agent 中埋点

```cpp
class MonitoredChatEngine {
public:
    MonitoredChatEngine() {
        request_counter_ = METRICS_COUNTER("agent_requests_total");
        active_requests_ = METRICS_GAUGE("agent_active_requests");
        error_counter_ = METRICS_COUNTER("agent_errors_total");
    }
    
    std::string process(const std::string& input) {
        request_counter_>increment();
        active_requests_>increment();
        
        try {
            std::string reply = do_process(input);
            return reply;
        } catch (...) {
            error_counter_>increment();
            throw;
        }
        
        active_requests_>decrement();
    }
};
```

---

## 第二步：Logging 日志

```cpp
// logger.hpp
enum class LogLevel { DEBUG, INFO, WARN, ERROR };

class Logger {
public:
    static Logger& instance() {
        static Logger instance;
        return instance;
    }
    
    void log(LogLevel level, const std::string& msg) {
        if (level < min_level_) return;
        
        json::object obj;
        obj["timestamp"] = get_timestamp();
        obj["level"] = level_to_string(level);
        obj["message"] = msg;
        
        std::cout << json::serialize(obj) << std::endl;
    }

private:
    LogLevel min_level_ = LogLevel::INFO;
};

#define LOG_INFO(msg) Logger::instance().log(LogLevel::INFO, msg)
#define LOG_ERROR(msg) Logger::instance().log(LogLevel::ERROR, msg)
```

---

## 本节总结

### 核心概念

1. **Metrics**：系统指标，用于监控和告警
2. **Logging**：事件记录，用于故障排查
3. **Tracing**：请求链路，用于性能分析

---

**下一步：** Step 14 部署运维
