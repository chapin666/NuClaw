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

### 什么是可观测性？

**可观测性（Observability）**：通过系统的外部输出了解其内部状态的能力。

**三大支柱：**

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

### 为什么重要？

**生产环境的挑战：**
1. **问题定位困难**：不知道哪里出了问题
2. **性能瓶颈**：不知道系统瓶颈在哪里
3. **故障恢复慢**：无法快速定位和修复
4. **容量规划**：不知道何时需要扩容

**可观测性解决：**
- **Metrics**：实时监控，异常早发现
- **Logging**：问题追溯，故障分析
- **Tracing**：链路分析，性能优化

---

## 第一步：Metrics（指标监控）

### 指标类型

| 类型 | 说明 | 示例 |
|:---|:---|:---|
| **Counter** | 计数器，只增不减 | 请求总数、错误总数 |
| **Gauge** | 仪表盘，可增可减 | 当前连接数、内存使用量 |
| **Histogram** | 直方图，分布统计 | 响应时间分布 |
| **Summary** | 摘要，百分位统计 | P99 响应时间 |

### Metrics 收集器

```cpp
// metrics.hpp
#pragma once
#include <string>
#include <map>
#include <vector>
#include <mutex>
#include <chrono>
#include <atomic>

// 指标值基类
class MetricValue {
public:
    virtual ~MetricValue() = default;
    virtual std::string to_string() const = 0;
    virtual void reset() = 0;
};

// Counter 计数器
class Counter : public MetricValue {
public:
    void increment(double value = 1.0) {
        value_.fetch_add(value);
    }
    
    double get() const {
        return value_.load();
    }
    
    std::string to_string() const override {
        return std::to_string(get());
    }
    
    void reset() override {
        value_.store(0);
    }

private:
    std::atomic<double> value_{0};
};

// Gauge 仪表盘
class Gauge : public MetricValue {
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
    
    std::string to_string() const override {
        return std::to_string(get());
    }
    
    void reset() override {
        value_.store(0);
    }

private:
    std::atomic<double> value_{0};
};

// Histogram 直方图
class Histogram : public MetricValue {
public:
    explicit Histogram(std::vector<double> buckets = 
        {0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1, 2.5, 5, 10}) 
        : buckets_(buckets), counts_(buckets.size() + 1) {}
    
    void observe(double value) {
        sum_.fetch_add(value);
        count_.fetch_add(1);
        
        for (size_t i = 0; i < buckets_.size(); ++i) {
            if (value <= buckets_[i]) {
                counts_[i].fetch_add(1);
                return;
            }
        }
        counts_.back().fetch_add(1);  // +Inf bucket
    }
    
    double get_sum() const { return sum_.load(); }
    uint64_t get_count() const { return count_.load(); }
    
    std::string to_string() const override {
        std::stringstream ss;
        ss << "count=" << get_count() << " sum=" << get_sum();
        return ss.str();
    }
    
    void reset() override {
        sum_.store(0);
        count_.store(0);
        for (auto& c : counts_) {
            c.store(0);
        }
    }

private:
    std::vector<double> buckets_;
    std::vector<std::atomic<uint64_t>> counts_;
    std::atomic<double> sum_{0};
    std::atomic<uint64_t> count_{0};
};

// Metrics 管理器
class Metrics {
public:
    // 获取单例
    static Metrics& instance() {
        static Metrics instance;
        return instance;
    }
    
    // 创建或获取 Counter
    Counter* counter(const std::string& name, 
                     const std::map<std::string, std::string>& labels = {}) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string key = build_key(name, labels);
        
        if (counters_.find(key) == counters_.end()) {
            counters_[key] = std::make_unique<Counter>();
        }
        return counters_[key].get();
    }
    
    // 创建或获取 Gauge
    Gauge* gauge(const std::string& name,
                 const std::map<std::string, std::string>& labels = {}) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string key = build_key(name, labels);
        
        if (gauges_.find(key) == gauges_.end()) {
            gauges_[key] = std::make_unique<Gauge>();
        }
        return gauges_[key].get();
    }
    
    // 创建或获取 Histogram
    Histogram* histogram(const std::string& name,
                         const std::map<std::string, std::string>& labels = {}) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string key = build_key(name, labels);
        
        if (histograms_.find(key) == histograms_.end()) {
            histograms_[key] = std::make_unique<Histogram>();
        }
        return histograms_[key].get();
    }
    
    // 导出为 Prometheus 格式
    std::string export_prometheus() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::stringstream ss;
        
        // Counter
        for (const auto& [key, counter] : counters_) {
            ss << key << " " << counter->get() << "\n";
        }
        
        // Gauge
        for (const auto& [key, gauge] : gauges_) {
            ss << key << " " << gauge->get() << "\n";
        }
        
        // Histogram
        for (const auto& [key, hist] : histograms_) {
            ss << key << "_count " << hist->get_count() << "\n";
            ss << key << "_sum " << hist->get_sum() << "\n";
        }
        
        return ss.str();
    }
    
    // 导出为 JSON
    std::string export_json() const {
        std::lock_guard<std::mutex> lock(mutex_);
        json::object result;
        
        for (const auto& [key, counter] : counters_) {
            result[key] = counter->get();
        }
        
        for (const auto& [key, gauge] : gauges_) {
            result[key] = gauge->get();
        }
        
        return json::serialize(result);
    }

private:
    std::string build_key(const std::string& name,
                          const std::map<std::string, std::string>& labels) {
        if (labels.empty()) {
            return name;
        }
        
        std::stringstream ss;
        ss << name;
        ss << "{";
        bool first = true;
        for (const auto& [k, v] : labels) {
            if (!first) ss << ",";
            ss << k << "=\"" << v << "\"";
            first = false;
        }
        ss << "}";
        return ss.str();
    }
    
    mutable std::mutex mutex_;
    std::map<std::string, std::unique_ptr<Counter>> counters_;
    std::map<std::string, std::unique_ptr<Gauge>> gauges_;
    std::map<std::string, std::unique_ptr<Histogram>> histograms_;
};

// 便捷宏
#define METRICS_COUNTER(name) Metrics::instance().counter(name)
#define METRICS_GAUGE(name) Metrics::instance().gauge(name)
#define METRICS_HISTOGRAM(name) Metrics::instance().histogram(name)
```

### 指标收集示例

```cpp
// 在代码中埋点
void handle_request(const Request& req) {
    auto start = std::chrono::steady_clock::now();
    
    // 请求计数
    METRICS_COUNTER("http_requests_total")->increment();
    METRICS_COUNTER("http_requests_total", {{"method", req.method}})->increment();
    
    // 当前连接数
    METRICS_GAUGE("active_connections")->increment();
    
    // 处理请求
    process_request(req);
    
    // 响应时间
    auto elapsed = std::chrono::steady_clock::now() - start;
    double seconds = std::chrono::duration<double>(elapsed).count();
    METRICS_HISTOGRAM("http_request_duration_seconds")->observe(seconds);
    
    METRICS_GAUGE("active_connections")->decrement();
}
```

---

## 第二步：Logging（日志系统）

### 日志级别

```cpp
// logger.hpp
#pragma once
#include <string>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <boost/json.hpp>

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
```

### 日志记录器

```cpp
class Logger {
public:
    struct LogEntry {
        std::chrono::system_clock::time_point timestamp;
        LogLevel level;
        std::string message;
        std::string file;
        int line;
        std::map<std::string, std::string> fields;
    };
    
    static Logger& instance() {
        static Logger instance;
        return instance;
    }
    
    void init(const std::string& level = "info",
              const std::string& format = "text",
              const std::string& output = "stdout",
              const std::string& file_path = "") {
        
        min_level_ = parse_level(level);
        format_ = format;
        output_ = output;
        file_path_ = file_path;
        
        if (output_ == "file" || output_ == "both") {
            file_stream_.open(file_path_, std::ios::app);
        }
    }
    
    void log(LogLevel level, const std::string& message,
             const std::string& file = "", int line = 0,
             const std::map<std::string, std::string>& fields = {}) {
        
        if (level < min_level_) return;
        
        LogEntry entry;
        entry.timestamp = std::chrono::system_clock::now();
        entry.level = level;
        entry.message = message;
        entry.file = file;
        entry.line = line;
        entry.fields = fields;
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::string formatted = format_entry(entry);
        
        // 输出到控制台
        if (output_ == "stdout" || output_ == "both") {
            std::cout << formatted << std::endl;
        }
        
        // 输出到文件
        if ((output_ == "file" || output_ == "both") && file_stream_.is_open()) {
            file_stream_ << formatted << std::endl;
        }
    }
    
    // 便捷方法
    void debug(const std::string& msg, 
               const std::string& file = "", int line = 0) {
        log(LogLevel::DEBUG, msg, file, line);
    }
    
    void info(const std::string& msg,
              const std::string& file = "", int line = 0) {
        log(LogLevel::INFO, msg, file, line);
    }
    
    void warn(const std::string& msg,
              const std::string& file = "", int line = 0) {
        log(LogLevel::WARN, msg, file, line);
    }
    
    void error(const std::string& msg,
               const std::string& file = "", int line = 0) {
        log(LogLevel::ERROR, msg, file, line);
    }

private:
    std::string format_entry(const LogEntry& entry) {
        if (format_ == "json") {
            return format_json(entry);
        } else {
            return format_text(entry);
        }
    }
    
    std::string format_text(const LogEntry& entry) {
        std::stringstream ss;
        
        auto time_t = std::chrono::system_clock::to_time_t(entry.timestamp);
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        ss << " [" << level_to_string(entry.level) << "] ";
        ss << entry.message;
        
        if (!entry.file.empty()) {
            ss << " (" << entry.file << ":" << entry.line << ")";
        }
        
        return ss.str();
    }
    
    std::string format_json(const LogEntry& entry) {
        json::object obj;
        
        auto time_t = std::chrono::system_clock::to_time_t(entry.timestamp);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
        obj["timestamp"] = ss.str();
        obj["level"] = level_to_string(entry.level);
        obj["message"] = entry.message;
        
        if (!entry.file.empty()) {
            obj["file"] = entry.file;
            obj["line"] = entry.line;
        }
        
        for (const auto& [k, v] : entry.fields) {
            obj[k] = v;
        }
        
        return json::serialize(obj);
    }
    
    LogLevel parse_level(const std::string& level) {
        if (level == "debug") return LogLevel::DEBUG;
        if (level == "info") return LogLevel::INFO;
        if (level == "warn") return LogLevel::WARN;
        if (level == "error") return LogLevel::ERROR;
        if (level == "fatal") return LogLevel::FATAL;
        return LogLevel::INFO;
    }
    
    LogLevel min_level_ = LogLevel::INFO;
    std::string format_ = "text";
    std::string output_ = "stdout";
    std::string file_path_;
    std::ofstream file_stream_;
    std::mutex mutex_;
};

// 便捷宏
#define LOG_DEBUG(msg) Logger::instance().debug(msg, __FILE__, __LINE__)
#define LOG_INFO(msg) Logger::instance().info(msg, __FILE__, __LINE__)
#define LOG_WARN(msg) Logger::instance().warn(msg, __FILE__, __LINE__)
#define LOG_ERROR(msg) Logger::instance().error(msg, __FILE__, __LINE__)
```

---

## 第三步：Tracing（链路追踪）

### Span 和 Trace

```cpp
// tracer.hpp
#pragma once
#include <string>
#include <map>
#include <vector>
#include <memory>
#include <chrono>

// Span 表示一个操作单元
class Span {
public:
    Span(const std::string& name, 
         const std::string& trace_id,
         const std::string& parent_id = "")
        : name_(name), trace_id_(trace_id), parent_id_(parent_id) {
        
        id_ = generate_id();
        start_time_ = std::chrono::steady_clock::now();
    }
    
    ~Span() {
        finish();
    }
    
    void finish() {
        if (finished_) return;
        
        end_time_ = std::chrono::steady_clock::now();
        finished_ = true;
        
        // 上报或存储
        Tracer::instance().report_span(this);
    }
    
    void set_tag(const std::string& key, const std::string& value) {
        tags_[key] = value;
    }
    
    void set_error(const std::string& error) {
        error_ = error;
        tags_["error"] = "true";
    }
    
    std::chrono::microseconds duration() const {
        auto end = finished_ ? end_time_ : std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(
            end - start_time_);
    }
    
    // Getters
    const std::string& get_id() const { return id_; }
    const std::string& get_trace_id() const { return trace_id_; }
    const std::string& get_name() const { return name_; }

private:
    std::string generate_id() {
        static std::atomic<uint64_t> counter{0};
        return "span_" + std::to_string(++counter);
    }
    
    std::string name_;
    std::string id_;
    std::string trace_id_;
    std::string parent_id_;
    std::map<std::string, std::string> tags_;
    std::string error_;
    
    std::chrono::steady_clock::time_point start_time_;
    std::chrono::steady_clock::time_point end_time_;
    bool finished_ = false;
};

// Tracer 管理器
class Tracer {
public:
    static Tracer& instance() {
        static Tracer instance;
        return instance;
    }
    
    // 开始一个新的 Trace
    std::shared_ptr<Span> start_span(const std::string& name,
                                       const std::string& parent_id = "") {
        std::string trace_id = parent_id.empty() ? generate_trace_id() : current_trace_id_;
        
        auto span = std::make_shared<Span>(name, trace_id, parent_id);
        active_spans_[span->get_id()] = span;
        
        if (parent_id.empty()) {
            current_trace_id_ = trace_id;
        }
        
        return span;
    }
    
    // 上报 Span
    void report_span(Span* span) {
        // 存储或发送到追踪系统（如 Jaeger, Zipkin）
        // 简化：这里只是打印
        std::cout << "[Trace] " << span->get_trace_id() << " " << span->get_name()
                  << " duration=" << span->duration().count() << "us"
                  << std::endl;
        
        active_spans_.erase(span->get_id());
    }

private:
    std::string generate_trace_id() {
        static std::atomic<uint64_t> counter{0};
        return "trace_" + std::to_string(++counter);
    }
    
    std::string current_trace_id_;
    std::map<std::string, std::weak_ptr<Span>> active_spans_;
};

// 自动 Span（RAII）
class AutoSpan {
public:
    AutoSpan(const std::string& name, const std::string& parent_id = "")
        : span_(Tracer::instance().start_span(name, parent_id)) {}
    
    ~AutoSpan() {
        if (span_) {
            span_>finish();
        }
    }
    
    std::shared_ptr<Span> get() { return span_; }

private:
    std::shared_ptr<Span> span_;
};

#define TRACE_SPAN(name) AutoSpan __span(name)
```

---

## 第四步：监控集成

### 集成到 Agent 系统

```cpp
// monitored_chat_engine.hpp
#pragma once
#include "chat_engine.hpp"
#include "metrics.hpp"
#include "logger.hpp"
#include "tracer.hpp"

class MonitoredChatEngine {
public:
    MonitoredChatEngine(ChatEngine& engine) : engine_(engine) {
        // 初始化指标
        request_counter_ = METRICS_COUNTER("chat_requests_total");
        request_duration_ = METRICS_HISTOGRAM("chat_request_duration_seconds");
        active_sessions_ = METRICS_GAUGE("chat_active_sessions");
    }
    
    std::string process(const std::string& user_input, ChatContext& ctx) {
        TRACE_SPAN("chat_process");
        
        // 记录日志
        LOG_INFO("处理用户请求: " + user_input);
        
        // 指标记录
        request_counter_>increment();
        active_sessions_>increment();
        
        auto start = std::chrono::steady_clock::now();
        
        try {
            // 实际处理
            std::string reply = engine_.process(user_input, ctx);
            
            // 记录成功
            auto elapsed = std::chrono::steady_clock::now() - start;
            double seconds = std::chrono::duration<double>(elapsed).count();
            request_duration_>observe(seconds);
            
            LOG_INFO("请求处理成功，耗时: " + std::to_string(seconds) + "s");
            
            return reply;
            
        } catch (const std::exception& e) {
            // 记录失败
            METRICS_COUNTER("chat_errors_total")->increment();
            LOG_ERROR("请求处理失败: " + std::string(e.what()));
            throw;
        }
        
        active_sessions_>decrement();
    }

private:
    ChatEngine& engine_;
    Counter* request_counter_;
    Histogram* request_duration_;
    Gauge* active_sessions_;
};
```

---

## 第五步：监控 Dashboard

### HTTP 指标接口

```cpp
// metrics_endpoint.hpp
#pragma once
#include "metrics.hpp"
#include <boost/beast.hpp>

class MetricsEndpoint {
public:
    void handle_request(http::request<http::string_body>& req,
                        http::response<http::string_body>& res) {
        
        if (req.target() == "/metrics") {
            // Prometheus 格式
            res.result(http::status::ok);
            res.set(http::field::content_type, "text/plain");
            res.body() = Metrics::instance().export_prometheus();
        }
        else if (req.target() == "/metrics/json") {
            // JSON 格式
            res.result(http::status::ok);
            res.set(http::field::content_type, "application/json");
            res.body() = Metrics::instance().export_json();
        }
        else if (req.target() == "/health") {
            // 健康检查
            res.result(http::status::ok);
            res.set(http::field::content_type, "application/json");
            res.body() = R"({"status": "healthy"})";
        }
    }
};
```

---

## 本节总结

### 核心概念

1. **Metrics**：系统指标，如请求数、响应时间、错误率
2. **Logging**：结构化日志，便于问题追溯
3. **Tracing**：请求链路追踪，分析性能瓶颈

### 代码演进

```
Step 12: 配置管理 (900行)
   ↓ + 可观测性
Step 13: 950行
   + metrics.hpp: 指标收集
   + logger.hpp: 日志系统
   + tracer.hpp: 链路追踪
   ~ 各组件: 集成监控埋点
```

### 监控工具链

| 类型 | 开源方案 | 云服务 |
|:---|:---|:---|
| Metrics | Prometheus | CloudWatch |
| Logging | ELK Stack | CloudWatch Logs |
| Tracing | Jaeger, Zipkin | AWS X-Ray |
| Dashboard | Grafana | CloudWatch Dashboard |

---

## 📝 课后练习

### 练习 1：告警规则
实现基于指标的告警：
```cpp
class AlertManager {
    void check_rules() {
        // 错误率 > 5% 告警
        // 响应时间 > 1s 告警
    }
};
```

### 练习 2：采样追踪
实现按概率采样的追踪（减少开销）：
```cpp
bool should_sample() {
    return random() < sample_rate;
}
```

### 练习 3：日志聚合
实现日志发送到远程收集器：
```cpp
class RemoteLogAppender {
    void send(const LogEntry& entry);
};
```

### 思考题
1. Metrics、Logging、Tracing 各自适合什么场景？
2. 如何平衡监控的详细程度与系统开销？
3. 分布式追踪中如何传递 Trace ID？

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

**恭喜！** 你的 Agent 现在具备了完整的可观测性。下一章我们将学习部署和运维。
