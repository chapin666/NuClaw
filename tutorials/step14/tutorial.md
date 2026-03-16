# Step 14: 监控告警 —— 让系统可观测

> 目标：实现监控、日志和指标收集，支持 Prometheus 和 Grafana
> 
003e 难度：⭐⭐⭐ | 代码量：约 950 行 | 预计学习时间：3-4 小时

---

## 一、为什么需要监控？

### 1.1 黑盒问题

没有监控的系统就像飞行没有仪表：

```
场景：用户反馈"系统好慢"

没有监控时：
┌───────────────────────────────────────────────────────┐
│  开发："哪里慢？"                                      │
│  运维："不知道，日志里没异常"                           │
│  开发："什么时候开始的？"                               │
│  运维："不清楚，用户刚反馈的"                           │
│  开发："影响多少用户？"                                 │
│  运维："无法统计..."                                    │
│                                                        │
│  结果：问题无法定位，只能靠猜测                          │
└───────────────────────────────────────────────────────┘

有监控时：
┌───────────────────────────────────────────────────────┐
│  Prometheus：                                          │
│  • QPS 从 1000 降到 100（11:23 开始）                   │
│  • 95% 响应时间从 200ms 升到 2000ms                     │
│                                                        │
│  Grafana：                                             │
│  • 内存使用率 95%，触发 OOM                              │
│  • LLM API 调用成功率从 99% 降到 60%                    │
│                                                        │
│  日志：                                                │
│  • 11:23:15 大量 "Connection timeout"                   │
│  • 11:23:20 开始报错 "Memory limit exceeded"            │
│                                                        │
│  结论：缓存失效导致穿透，请求直接打到 LLM，              │
│        并发过高内存耗尽                                  │
└───────────────────────────────────────────────────────┘
```

### 1.2 可观测性三支柱

```
┌─────────────────────────────────────────────────────────────────────┐
│                     可观测性（Observability）                        │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│   ┌──────────────┐  ┌──────────────┐  ┌──────────────┐             │
│   │   Metrics    │  │    Logs      │  │   Traces     │             │
│   │    指标      │  │    日志      │  │    追踪      │             │
│   ├──────────────┤  ├──────────────┤  ├──────────────┤             │
│   │              │  │              │  │              │             │
│   │ • QPS        │  │ • 请求详情   │  │ • 调用链     │             │
│   │ • 延迟       │  │ • 错误堆栈   │  │ • 服务依赖   │             │
│   │ • 错误率     │  │ • 上下文     │  │ • 性能瓶颈   │             │
│   │ • 资源使用   │  │ • 审计信息   │  │ • 分布式追踪 │             │
│   │              │  │              │  │              │             │
│   │ 特点：       │  │ 特点：       │  │ 特点：       │             │
│   │ 聚合数据     │  │ 离散事件     │  │ 请求链路     │             │
│   │ 趋势分析     │  │ 详细信息     │  │ 跨服务追踪   │             │
│   │ 告警触发     │  │ 问题定位     │  │ 性能分析     │             │
│   └──────────────┘  └──────────────┘  └──────────────┘             │
│           │                │                │                       │
│           └────────────────┼────────────────┘                       │
│                            │                                        │
│                            ▼                                        │
│                 ┌──────────────────────┐                           │
│                 │    告警系统          │                           │
│                 │  • 阈值告警          │                           │
│                 │  • 异常检测          │                           │
│                 │  • 通知渠道          │                           │
│                 └──────────────────────┘                           │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

**三者关系：**
- **Metrics** 告诉你系统有问题（告警）
- **Logs** 告诉你问题是什么（详情）
- **Traces** 告诉你问题在哪里（定位）

---

## 二、指标收集（Metrics）

### 2.1 Prometheus 简介

Prometheus 是云原生监控的标准：

```
┌─────────────────────────────────────────────────────────────────────┐
│                      Prometheus 架构                                │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  ┌──────────────┐     Pull      ┌──────────────────────────┐       │
│  │  Prometheus  │ ◄──────────── │  Your Application        │       │
│  │   Server     │  /metrics     │  ┌────────────────────┐  │       │
│  │              │  (HTTP)       │  │ /metrics endpoint  │  │       │
│  │ • 时序数据库  │               │  │ • counters         │  │       │
│  │ • 告警规则   │               │  │ • gauges           │  │       │
│  │ • 查询语言   │               │  │ • histograms       │  │       │
│  │   (PromQL)  │               │  └────────────────────┘  │       │
│  └──────┬───────┘               └──────────────────────────┘       │
│         │                                                           │
│         │ Query                                                     │
│         ▼                                                           │
│  ┌──────────────┐     Visualize    ┌──────────────┐                │
│  │  AlertManager│ ────────────────▶ │   Grafana    │                │
│  │              │                   │  (Dashboard) │                │
│  │ • 告警路由   │                   │              │                │
│  │ • 抑制/静默  │                   │ • 可视化     │                │
│  │ • 通知渠道   │                   │ • 告警面板   │                │
│  └──────────────┘                   └──────────────┘                │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

**Prometheus 特点：**
- Pull 模式采集（HTTP 拉取）
- 时序数据存储
- 多维数据模型（标签）
- 强大的 PromQL 查询语言

### 2.2 指标类型详解

```cpp
// ═══════════════════════════════════════════════════════════════════
// 1. Counter（计数器）
// ═══════════════════════════════════════════════════════════════════
// 特点：只增不减，适合累计值
// 场景：请求总数、错误总数、处理消息数

class Counter {
public:
    void inc(double value = 1.0) { 
        value_.fetch_add(value, std::memory_order_relaxed); 
    }
    double get() const { return value_.load(); }
    
private:
    std::atomic<double> value_{0};
};

// 使用示例
Counter requests_total;
Counter errors_total;

void handle_request() {
    requests_total.inc();
    
    try {
        process();
    } catch (...) {
        errors_total.inc();  // 错误计数
        throw;
    }
}

// Prometheus 输出格式
// # HELP http_requests_total Total HTTP requests
// # TYPE http_requests_total counter
// http_requests_total{path="/chat",status="200"} 1024
// http_requests_total{path="/chat",status="500"} 5


// ═══════════════════════════════════════════════════════════════════
// 2. Gauge（仪表盘）
// ═══════════════════════════════════════════════════════════════════
// 特点：可增可减，适合瞬时值
// 场景：当前连接数、内存使用量、队列长度

class Gauge {
public:
    void set(double value) { value_.store(value); }
    void inc(double value = 1.0) { value_.fetch_add(value); }
    void dec(double value = 1.0) { value_.fetch_sub(value); }
    double get() const { return value_.load(); }
    
private:
    std::atomic<double> value_{0};
};

// 使用示例
Gauge active_connections;
Gauge memory_usage_mb;

void on_connect() {
    active_connections.inc();
}

void on_disconnect() {
    active_connections.dec();
}

void update_metrics() {
    memory_usage_mb = get_current_memory_usage();
}

// Prometheus 输出格式
// # HELP active_connections Current active connections
// # TYPE active_connections gauge
// active_connections 42


// ═══════════════════════════════════════════════════════════════════
// 3. Histogram（直方图）
// ═══════════════════════════════════════════════════════════════════
// 特点：采样分布，分桶统计
// 场景：请求延迟、响应大小

class Histogram {
public:
    Histogram(std::vector<double> buckets)
        : buckets_(std::move(buckets)), 
          counts_(buckets_.size() + 1) {}  // +1 for +Inf
    
    void observe(double value) {
        // 找到对应的桶
        for (size_t i = 0; i < buckets_.size(); ++i) {
            if (value <= buckets_[i]) {
                counts_[i].fetch_add(1);
                return;
            }
        }
        counts_.back().fetch_add(1);  // +Inf bucket
        
        // 更新总和和计数（用于计算平均值）
        sum_.fetch_add(value);
        count_.fetch_add(1);
    }
    
private:
    std::vector<double> buckets_;
    std::vector<std::atomic<uint64_t>> counts_;
    std::atomic<double> sum_{0};
    std::atomic<uint64_t> count_{0};
};

// 使用示例
Histogram request_duration{
    {0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1, 2.5, 5, 10}
};

void handle_request() {
    auto start = std::chrono::steady_clock::now();
    
    process_request();
    
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - start
    ).count();
    
    request_duration.observe(elapsed);
}

// Prometheus 输出格式
// # HELP http_request_duration_seconds Request duration
// # TYPE http_request_duration_seconds histogram
// http_request_duration_seconds_bucket{le="0.1"} 500
// http_request_duration_seconds_bucket{le="0.5"} 950
// http_request_duration_seconds_bucket{le="1.0"} 990
// http_request_duration_seconds_bucket{le="+Inf"} 1000
// http_request_duration_seconds_sum 45.2
// http_request_duration_seconds_count 1000
```

### 2.3 指标收集器实现

```cpp
class MetricsRegistry {
public:
    // 注册或获取 Counter
    Counter& counter(const std::string& name,
                     const std::map<std::string, std::string>& labels = {}) {
        std::string key = serialize_labels(name, labels);
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = counters_.find(key);
        if (it == counters_.end()) {
            auto [new_it, _] = counters_.emplace(key, std::make_unique<Counter>());
            return *new_it->second;
        }
        return *it->second;
    }
    
    // 注册或获取 Gauge
    Gauge& gauge(const std::string& name,
                 const std::map<std::string, std::string>& labels = {}) {
        std::string key = serialize_labels(name, labels);
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = gauges_.find(key);
        if (it == gauges_.end()) {
            auto [new_it, _] = gauges_.emplace(key, std::make_unique<Gauge>());
            return *new_it->second;
        }
        return *it->second;
    }
    
    // 注册或获取 Histogram
    Histogram& histogram(const std::string& name,
                         const std::vector<double>& buckets,
                         const std::map<std::string, std::string>& labels = {}) {
        std::string key = serialize_labels(name, labels);
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = histograms_.find(key);
        if (it == histograms_.end()) {
            auto [new_it, _] = histograms_.emplace(
                key, std::make_unique<Histogram>(buckets)
            );
            return *new_it->second;
        }
        return *it->second;
    }
    
    // 生成 Prometheus 格式的文本
    std::string serialize() const {
        std::ostringstream out;
        std::lock_guard<std::mutex> lock(mutex_);
        
        // 序列化 Counters
        for (const auto& [key, counter] : counters_) {
            auto [name, labels] = parse_key(key);
            out << "# HELP " << name << "\n";
            out << "# TYPE " << name << " counter\n";
            out << name << labels << " " << counter->get() << "\n";
        }
        
        // 序列化 Gauges...
        // 序列化 Histograms...
        
        return out.str();
    }
    
    // 便捷的请求记录方法
    void record_http_request(const std::string& path,
                             int status_code,
                             std::chrono::milliseconds duration) {
        // 记录总数
        counter("http_requests_total", {{"path", path}, {"status", std::to_string(status_code)}}).inc();
        
        // 记录延迟
        auto& hist = histogram("http_request_duration_seconds", 
                               {0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1, 2.5, 5, 10},
                               {{"path", path}});
        hist.observe(duration.count() / 1000.0);  // 转为秒
    }

private:
    std::string serialize_labels(const std::string& name,
                                  const std::map<std::string, std::string>& labels) {
        if (labels.empty()) return name;
        
        std::ostringstream oss;
        oss << name << "{";
        bool first = true;
        for (const auto& [k, v] : labels) {
            if (!first) oss << ",";
            oss << k << "=\"" << v << "\"";
            first = false;
        }
        oss << "}";
        return oss.str();
    }

    mutable std::mutex mutex_;
    std::map<std::string, std::unique_ptr<Counter>> counters_;
    std::map<std::string, std::unique_ptr<Gauge>> gauges_;
    std::map<std::string, std::unique_ptr<Histogram>> histograms_;
};

// 全局注册表
MetricsRegistry g_metrics;

// 在 HTTP 服务器中暴露 /metrics 端点
void handle_metrics(Request& req, Response& res) {
    res.set_content(g_metrics.serialize(), "text/plain");
}
```

---

## 三、日志系统

### 3.1 日志设计原则

```
好的日志应该：

1. 结构化（JSON）
   {"timestamp":"2024-03-15T10:30:00Z","level":"error","msg":"DB failed","error":"timeout"}
   
   而不是：
   [2024-03-15 10:30:00] ERROR: DB failed, error: timeout

2. 分级（Level）
   DEBUG - 开发调试（详细）
   INFO  - 正常运行信息
   WARN  - 警告，需要注意
   ERROR - 错误，需要处理

3. 包含上下文
   {"user_id":"123","session_id":"abc","request_id":"xyz","msg":"..."}

4. 可采样（高流量时减少日志量）
```

### 3.2 日志实现

```cpp
enum class LogLevel { DEBUG = 0, INFO = 1, WARN = 2, ERROR = 3 };

class Logger {
public:
    struct Options {
        LogLevel min_level = LogLevel::INFO;
        std::string output = "stdout";  // stdout, stderr, file
        std::string file_path;
        bool structured = true;  // JSON 格式
        std::map<std::string, std::string> default_fields;  // 默认字段
    };
    
    static void init(const Options& opts) {
        options_ = opts;
        
        if (opts.output == "file" && !opts.file_path.empty()) {
            file_.open(opts.file_path, std::ios::app);
        }
    }
    
    // 结构化日志记录
    static void log(LogLevel level,
                    const std::string& message,
                    const std::map<std::string, std::string>& fields = {}) {
        if (level < options_.min_level) return;
        
        json entry;
        entry["timestamp"] = get_iso_timestamp();
        entry["level"] = level_to_string(level);
        entry["message"] = message;
        
        // 添加上下文字段
        for (const auto& [k, v] : options_.default_fields) {
            entry[k] = v;
        }
        for (const auto& [k, v] : fields) {
            entry[k] = v;
        }
        
        // 获取当前线程的上下文（使用 thread_local）
        auto& ctx = get_thread_context();
        if (!ctx.request_id.empty()) {
            entry["request_id"] = ctx.request_id;
        }
        if (!ctx.user_id.empty()) {
            entry["user_id"] = ctx.user_id;
        }
        
        output(entry.dump());
    }
    
    // 便捷方法
    template<typename... Args>
    static void info(const std::string& fmt, Args... args) {
        log(LogLevel::INFO, fmt::format(fmt, args...));
    }
    
    template<typename... Args>
    static void error(const std::string& fmt, Args... args) {
        log(LogLevel::ERROR, fmt::format(fmt, args...));
    }
    
    // 设置线程上下文（用于关联日志）
    static void set_context(const std::string& request_id,
                           const std::string& user_id = "") {
        auto& ctx = get_thread_context();
        ctx.request_id = request_id;
        ctx.user_id = user_id;
    }

private:
    struct ThreadContext {
        std::string request_id;
        std::string user_id;
    };
    
    static ThreadContext& get_thread_context() {
        static thread_local ThreadContext ctx;
        return ctx;
    }
    
    static void output(const std::string& line) {
        if (options_.output == "file" && file_.is_open()) {
            file_ << line << "\n";
            file_.flush();
        } else {
            std::cout << line << "\n";
        }
    }
    
    static std::string level_to_string(LogLevel level) {
        switch (level) {
            case LogLevel::DEBUG: return "debug";
            case LogLevel::INFO: return "info";
            case LogLevel::WARN: return "warn";
            case LogLevel::ERROR: return "error";
        }
        return "unknown";
    }
    
    static Options options_;
    static std::ofstream file_;
};

// 使用示例
void process_request(const Request& req) {
    // 设置上下文，后续日志自动包含
    Logger::set_context(req.request_id, req.user_id);
    
    Logger::info("Processing request", {{"path", req.path}});
    
    try {
        auto result = do_process(req);
        Logger::info("Request completed", {
            {"duration_ms", result.duration_ms},
            {"status", "success"}
        });
    } catch (const std::exception& e) {
        Logger::error("Request failed", {
            {"error", e.what()},
            {"status", "failed"}
        });
    }
}
```

---

## 四、健康检查

```cpp
class HealthChecker {
public:
    using CheckFunction = std::function<CheckResult()>;
    
    struct CheckResult {
        enum Status { PASS, FAIL, WARN } status;
        std::string message;
        std::optional<std::chrono::milliseconds> response_time;
    };
    
    struct ComponentHealth {
        std::string name;
        CheckResult result;
        std::chrono::system_clock::time_point checked_at;
    };
    
    void register_check(const std::string& name, CheckFunction check) {
        std::lock_guard<std::mutex> lock(mutex_);
        checks_[name] = std::move(check);
    }
    
    // 执行所有检查
    json check_all() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        json result;
        result["status"] = "healthy";
        result["timestamp"] = get_iso_timestamp();
        
        bool has_failures = false;
        
        for (const auto& [name, check] : checks_) {
            auto start = std::chrono::steady_clock::now();
            auto r = check();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start
            );
            
            json check_result;
            check_result["status"] = status_to_string(r.status);
            check_result["message"] = r.message;
            check_result["response_time_ms"] = elapsed.count();
            
            result["checks"][name] = check_result;
            
            if (r.status == CheckResult::FAIL) {
                has_failures = true;
            }
        }
        
        result["status"] = has_failures ? "unhealthy" : "healthy";
        return result;
    }

private:
    std::string status_to_string(CheckResult::Status s) {
        switch (s) {
            case CheckResult::PASS: return "pass";
            case CheckResult::FAIL: return "fail";
            case CheckResult::WARN: return "warn";
        }
        return "unknown";
    }
    
    std::map<std::string, CheckFunction> checks_;
    std::mutex mutex_;
};

// 注册检查
HealthChecker g_health;

void init_health_checks(Database& db, LLMClient& llm, RedisClient& redis) {
    // 数据库检查
    g_health.register_check("database", [&db]() {
        if (db.ping()) {
            return HealthChecker::CheckResult{
                .status = HealthChecker::CheckResult::PASS,
                .message = "Connected"
            };
        }
        return HealthChecker::CheckResult{
            .status = HealthChecker::CheckResult::FAIL,
            .message = "Connection failed"
        };
    });
    
    // LLM API 检查
    g_health.register_check("llm_api", [&llm]() {
        auto resp = llm.ping();
        if (resp.success) {
            return HealthChecker::CheckResult{
                .status = HealthChecker::CheckResult::PASS,
                .message = "OK"
            };
        }
        return HealthChecker::CheckResult{
            .status = HealthChecker::CheckResult::FAIL,
            .message = "Timeout"
        };
    });
    
    // Redis 检查
    g_health.register_check("cache", [&redis]() {
        if (redis.ping()) {
            return HealthChecker::CheckResult{
                .status = HealthChecker::CheckResult::PASS,
                .message = "Connected"
            };
        }
        return HealthChecker::CheckResult{
            .status = HealthChecker::CheckResult::FAIL,
            .message = "Connection failed"
        };
    });
    
    // 磁盘空间检查
    g_health.register_check("disk", []() {
        auto free_space = get_free_disk_space("/app/data");
        if (free_space < 1 * 1024 * 1024 * 1024) {  // 小于 1GB
            return HealthChecker::CheckResult{
                .status = HealthChecker::CheckResult::WARN,
                .message = "Low disk space: " + std::to_string(free_space / 1024 / 1024) + "MB"
            };
        }
        return HealthChecker::CheckResult{
            .status = HealthChecker::CheckResult::PASS,
            .message = "OK"
        };
    });
}

// HTTP 端点
void handle_health(Request& req, Response& res) {
    auto result = g_health.check_all();
    
    int status_code = (result["status"] == "healthy") ? 200 : 503;
    res.status = status_code;
    res.set_content(result.dump(2), "application/json");
}
```

---

## 五、本章小结

**核心收获：**

1. **可观测性三支柱**：
   - Metrics：聚合指标，趋势分析
   - Logs：离散事件，详情记录
   - Traces：请求链路，性能分析

2. **Prometheus 指标**：
   - Counter：只增不减（请求数）
   - Gauge：可增可减（连接数）
   - Histogram：分布统计（延迟）

3. **结构化日志**：
   - JSON 格式便于解析
   - 分级控制日志量
   - 上下文关联

4. **健康检查**：
   - 组件状态检查
   - HTTP 端点暴露
   - 支持负载均衡

---

## 六、引出的问题

### 6.1 部署问题

监控和日志系统已经就绪，但还缺少标准化的部署流程：

```
问题：
• 不同环境配置不同，手动管理容易出错
• 依赖环境差异导致"在我机器上能跑"
• 发布流程手工操作，容易出错
• 无法快速回滚

需要：容器化 + CI/CD 自动化
```

**下一章预告（Step 14）：**

我们将实现**部署运维**：
- Docker 容器化（多阶段构建）
- Docker Compose 编排
- GitHub Actions CI/CD
- 生产部署最佳实践

监控已经就绪，接下来要让系统可部署、可运维。
