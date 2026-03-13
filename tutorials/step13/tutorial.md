# Step 13: 监控告警 - Metrics, Logging, Tracing

> 目标：实现可观测性，掌握生产监控
> 
> 难度：⭐⭐⭐⭐⭐ (困难)
> 
> 代码量：约 950 行
> 
> 预计学习时间：5-6 小时

---

## 🎯 Agent 开发知识点

**本节核心问题：** 如何了解 Agent 的运行状态和性能？

**Agent 可观测性需求：**
```
Metrics：请求量、响应时间、错误率、工具调用次数
Logging：请求日志、错误日志、调试日志
Tracing：请求链路、工具调用链
```

---

## 📚 理论基础 + 代码实现

### 1. Metrics 指标收集

**理论：** 量化 Agent 运行状态

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
    
    // Prometheus 格式导出
    std::string export_prometheus() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::stringstream ss;
        
        for (const auto& [name, c] : counters_) {
            ss << name << " " << c->get() << "\n";
        }
        for (const auto& [name, g] : gauges_) {
            ss << name << " " << g->get() << "\n";
        }
        return ss.str();
    }

private:
    mutable std::mutex mutex_;
    std::map<std::string, std::unique_ptr<Counter>> counters_;
    std::map<std::string, std::unique_ptr<Gauge>> gauges_;
};

#define METRICS_COUNTER(name) Metrics::instance().counter(name)
#define METRICS_GAUGE(name) Metrics::instance().gauge(name)
```

### 2. 在 Agent 中埋点

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
        auto start = std::chrono::steady_clock::now();
        
        request_counter_>increment();
        active_requests_>increment();
        
        try {
            std::string reply = do_process(input);
            return reply;
        } catch (const std::exception& e) {
            error_counter_>increment();
            LOG_ERROR("处理失败: " + std::string(e.what()));
            throw;
        }
        
        active_requests_>decrement();
        
        auto elapsed = std::chrono::steady_clock::now() - start;
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed);
        request_duration_>increment(ms.count());
    }

private:
    Counter* request_counter_;
    Counter* request_duration_;
    Counter* error_counter_;
    Gauge* active_requests_;
};
```

### 3. 结构化日志

```cpp
// logger.hpp
enum class LogLevel { DEBUG, INFO, WARN, ERROR };

class Logger {
public:
    static Logger& instance() {
        static Logger instance;
        return instance;
    }
    
    void log(LogLevel level, const std::string& msg,
             const std::string& file = "", int line = 0) {
        if (level < min_level_) return;
        
        json::object obj;
        obj["timestamp"] = get_timestamp();
        obj["level"] = level_to_string(level);
        obj["message"] = msg;
        
        if (!file.empty()) {
            obj["file"] = file;
            obj["line"] = line;
        }
        
        std::cout << json::serialize(obj) << std::endl;
    }

private:
    LogLevel min_level_ = LogLevel::INFO;
};

#define LOG_INFO(msg) Logger::instance().log(LogLevel::INFO, msg, __FILE__, __LINE__)
#define LOG_ERROR(msg) Logger::instance().log(LogLevel::ERROR, msg, __FILE__, __LINE__)
```

---

## 📋 Agent 开发检查清单

- [ ] 关键路径是否都有指标？
- [ ] 错误是否都记录日志？
- [ ] 日志是否结构化？
- [ ] 是否有健康检查端点？

---

**下一步：** Step 14 部署运维
