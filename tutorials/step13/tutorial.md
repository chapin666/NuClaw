# Step 13: 监控告警 —— 让系统可观测

> 目标：实现监控、日志和指标收集，支持 Prometheus 和 Grafana
> 
003e 难度：⭐⭐⭐ | 代码量：约 950 行 | 预计学习时间：3-4 小时

---

## 一、为什么需要监控？

### 1.1 黑盒问题

没有监控的系统就像飞行没有仪表：

```
用户反馈："系统好慢"

没有监控时：
- 不知道哪里慢
- 不知道何时开始慢
- 不知道影响范围

有监控时：
- QPS 从 1000 降到 100
- 数据库响应时间从 10ms 升到 500ms
- 内存使用率 95%
```

### 1.2 可观测性三支柱

```
┌─────────────────────────────────────────────┐
│              Observability                  │
├─────────────────────────────────────────────┤
│  ┌─────────┐  ┌─────────┐  ┌─────────┐     │
│  │ Metrics │  │  Logs   │  │ Traces  │     │
│  │  指标   │  │  日志   │  │  追踪   │     │
│  │         │  │         │  │         │     │
│  │• QPS    │  │• 请求   │  │• 调用链 │     │
│  │• 延迟   │  │  详情   │  │• 依赖   │     │
│  │• 错误率 │  │• 错误   │  │• 瓶颈   │     │
│  │• 资源   │  │  堆栈   │  │         │     │
└────────┬────────┴────────┴────────┬────────┘
         │                          │
         └──────────┬───────────────┘
                    │
         ┌──────────┴──────────┐
         │   AlertManager      │
         │   告警通知（邮件/短信/钉钉）│
         └─────────────────────┘
```

---

## 二、指标收集（Metrics）

### 2.1 Prometheus 指标类型

```cpp
// Counter: 只增不减的计数器
class Counter {
public:
    void inc(double value = 1.0) { value_ += value; }
    double get() const { return value_; }
private:
    std::atomic<double> value_{0};
};

// Gauge: 可增可减的数值
class Gauge {
public:
    void set(double value) { value_ = value; }
    void inc(double value = 1.0) { value_ += value; }
    void dec(double value = 1.0) { value_ -= value; }
private:
    std::atomic<double> value_{0};
};

// Histogram: 分布统计
class Histogram {
public:
    void observe(double value);
    // 输出分位数：0.5, 0.9, 0.99
private:
    std::vector<double> buckets_;
    std::vector<std::atomic<uint64_t>> counts_;
};
```

### 2.2 指标收集器

```cpp
class MetricsCollector {
public:
    MetricsCollector(uint16_t port = 9090);
    
    // 注册指标
    Counter& counter(const std::string& name, 
                     const std::map<std::string, std::string>& labels = {});
    
    Gauge& gauge(const std::string& name,
                 const std::map<std::string, std::string>& labels = {});
    
    Histogram& histogram(const std::string& name,
                         const std::vector<double>& buckets,
                         const std::map<std::string, std::string>& labels = {});
    
    // 记录事件
    void record_request(const std::string& path, 
                        int status_code,
                        std::chrono::milliseconds duration);
    
    void record_llm_call(const std::string& model,
                         int prompt_tokens,
                         int completion_tokens);

private:
    // 暴露 Prometheus 格式数据
    void expose_metrics();
    
    std::map<std::string, std::unique_ptr<Counter>> counters_;
    std::map<std::string, std::unique_ptr<Gauge>> gauges_;
    std::map<std::string, std::unique_ptr<Histogram>> histograms_;
};

// 使用示例
MetricsCollector metrics;

void handle_request(const Request& req) {
    auto start = std::chrono::steady_clock::now();
    
    // 处理请求...
    int status = process(req);
    
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start
    );
    
    // 记录指标
    metrics.record_request(req.path, status, elapsed);
}
```

### 2.3 Prometheus 输出格式

```
# HELP http_requests_total Total HTTP requests
# TYPE http_requests_total counter
http_requests_total{path="/chat",status="200"} 1024
http_requests_total{path="/chat",status="500"} 5

# HELP http_request_duration_seconds HTTP request duration
# TYPE http_request_duration_seconds histogram
http_request_duration_seconds_bucket{le="0.1"} 500
http_request_duration_seconds_bucket{le="0.5"} 950
http_request_duration_seconds_bucket{le="1.0"} 990
http_request_duration_seconds_sum 45.2
http_request_duration_seconds_count 1024

# HELP active_connections Current active connections
# TYPE active_connections gauge
active_connections 42
```

---

## 三、日志系统

```cpp
enum class LogLevel { DEBUG, INFO, WARN, ERROR };

class Logger {
public:
    static void init(const LoggingConfig& config);
    
    template<typename... Args>
    static void log(LogLevel level, const std::string& format, Args... args) {
        if (level < min_level_) return;
        
        std::string msg = fmt::format(format, args...);
        
        json log_entry = {
            {"timestamp", get_timestamp()},
            {"level", level_to_string(level)},
            {"message", msg}
        };
        
        output(log_entry.dump());
    }
    
    // 便捷方法
    template<typename... Args>
    static void info(const std::string& fmt, Args... args) {
        log(LogLevel::INFO, fmt, args...);
    }
    
    template<typename... Args>
    static void error(const std::string& fmt, Args... args) {
        log(LogLevel::ERROR, fmt, args...);
    }

private:
    static LogLevel min_level_;
    static std::ofstream file_;
};

// 使用
Logger::info("Request processed: path={}, duration={}ms", path, duration);
Logger::error("Database connection failed: {}", error_msg);
```

---

## 四、健康检查

```cpp
class HealthChecker {
public:
    struct CheckResult {
        bool healthy;
        std::string message;
    };
    
    void register_check(const std::string& name,
                       std::function<CheckResult()> check);
    
    json check_all() {
        json result;
        result["status"] = "healthy";
        
        for (const auto& [name, check] : checks_) {
            auto r = check();
            result["checks"][name] = {
                {"status", r.healthy ? "pass" : "fail"},
                {"message", r.message}
            };
            if (!r.healthy) {
                result["status"] = "unhealthy";
            }
        }
        
        return result;
    }

private:
    std::map<std::string, std::function<CheckResult()>> checks_;
};

// 注册检查
HealthChecker health;

health.register_check("database", []() {
    if (db_.is_connected()) {
        return {true, "Connected"};
    }
    return {false, "Disconnected"};
});

health.register_check("llm_api", []() {
    auto resp = llm_.ping();
    if (resp.success) {
        return {true, "OK"};
    }
    return {false, "Timeout"};
});
```

---

## 五、本章小结

- **Metrics**：Prometheus 格式，计数器/仪表盘/直方图
- **Logs**：结构化日志，分级输出
- **Health**：组件健康检查

---

**下一章（Step 14）：部署运维**
