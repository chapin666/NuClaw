# Step 12: 监控与可观测性

> 目标：实现监控和健康检查，支持 Prometheus 指标收集
> 
003e 难度：⭐⭐⭐ | 代码量：约 1000 行 | 预计学习时间：3-4 小时

---

## 一、问题引入

### 1.1 Step 11 的问题

应用功能完整，但缺少运维能力：
```
问题 1: 无法知道服务运行状态
  - 服务是否健康？
  - 连接数多少？
  - 内存使用情况？

问题 2: 不知道 API 调用量和延迟
  - QPS 多少？
  - 平均响应时间？
  - 错误率多少？

问题 3: 出现问题无法及时发现
  - 服务崩溃才知道
  - 没有告警机制
```

### 1.2 可观测性三大支柱

```
┌─────────────────────────────────────────────┐
│              Observability                  │
│                 可观测性                     │
├─────────────────────────────────────────────┤
│                                             │
│  ┌─────────┐  ┌─────────┐  ┌─────────┐     │
│  │ Metrics │  │  Logs   │  │ Traces  │     │
│  │  指标   │  │  日志   │  │  追踪   │     │
│  │         │  │         │  │         │     │
│  │• QPS    │  │• 请求   │  │• 请求链 │     │
│  │• 延迟   │  │  详情   │  │  路分析 │     │
│  │• 错误率 │  │• 错误   │  │• 性能   │     │
│  │• 资源   │  │  堆栈   │  │  瓶颈   │     │
│  │  使用   │  │• 调试   │  │         │     │
│  │         │  │  信息   │  │         │     │
│  └─────────┘  └─────────┘  └─────────┘     │
│                                             │
└─────────────────────────────────────────────┘
```

### 1.3 本章目标

1. **健康检查接口**：/health, /ready
2. **Prometheus 指标**：QPS、延迟、错误率
3. **日志系统**：结构化日志
4. **性能分析**：API 调用统计

---

## 二、核心概念

### 2.1 Prometheus 指标类型

| 类型 | 说明 | 示例 |
|:---|:---|:---|
| Counter | 只增不减的计数器 | 总请求数、错误数 |
| Gauge | 可增可减的数值 | 当前连接数、内存使用 |
| Histogram | 分布统计 | 请求延迟分布 |
| Summary | 分位数统计 | 99分位延迟 |

### 2.2 健康检查模式

```
/health - 存活检查（Liveness）
  - 返回 200：服务正常运行
  - 返回 500：服务需要重启

/ready - 就绪检查（Readiness）
  - 返回 200：可以接收流量
  - 返回 503：暂时不可接收（如数据库未连接）
```

---

## 三、代码结构详解

### 3.1 指标收集器

```cpp
#include <prometheus/counter.h>
#include <prometheus/gauge.h>
#include <prometheus/histogram.h>
#include <prometheus/exposer.h>
#include <prometheus/registry.h>

class MetricsCollector {
public:
    MetricsCollector(uint16_t port = 9090) {
        // 创建 Registry
        registry_ = std::make_shared<prometheus::Registry>();
        
        // 创建指标族
        request_counter_ = &prometheus::BuildCounter()
            .Name("http_requests_total")
            .Help("Total HTTP requests")
            .Register(*registry_)
            .Add({{"service", "nuclaw"}});
        
        active_connections_ = &prometheus::BuildGauge()
            .Name("active_connections")
            .Help("Number of active connections")
            .Register(*registry_)
            .Add({{"service", "nuclaw"}});
        
        request_duration_ = &prometheus::BuildHistogram()
            .Name("http_request_duration_seconds")
            .Help("HTTP request duration")
            .Register(*registry_)
            .Add({{"service", "nuclaw"}},
                 prometheus::Histogram::BucketBoundaries{
                     0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1, 2.5, 5, 10
                 });
        
        llm_tokens_ = &prometheus::BuildCounter()
            .Name("llm_tokens_total")
            .Help("Total LLM tokens used")
            .Register(*registry_)
            .Add({{"service", "nuclaw"}});
        
        // 启动 HTTP 服务暴露指标
        exposer_ = std::make_unique<prometheus::Exposer>(
            "0.0.0.0:" + std::to_string(port)
        );
        exposer_->RegisterCollectable(registry_);
    }
    
    // 记录请求
    void record_request(const std::string& path, 
                        const std::string& method,
                        int status_code,
                        double duration_seconds) {
        request_counter_.Add({
            {"path", path},
            {"method", method},
            {"status", std::to_string(status_code)}
        }).Increment();
        
        request_duration_.Observe(duration_seconds);
    }
    
    // 连接数管理
    void connection_opened() {
        active_connections_.Increment();
    }
    
    void connection_closed() {
        active_connections_.Decrement();
    }
    
    // 记录 LLM Token 使用
    void record_llm_tokens(size_t tokens) {
        llm_tokens_.Increment(tokens);
    }

private:
    std::shared_ptr<prometheus::Registry> registry_;
    std::unique_ptr<prometheus::Exposer> exposer_;
    
    prometheus::Counter& request_counter_;
    prometheus::Gauge& active_connections_;
    prometheus::Histogram& request_duration_;
    prometheus::Counter& llm_tokens_;
};
```

### 3.2 健康检查端点

```cpp
class HealthChecker {
public:
    struct HealthStatus {
        bool healthy;
        std::string message;
        std::map<std::string, bool> dependencies;
    };
    
    void register_check(const std::string& name,
                       std::function<bool()> check) {
        checks_[name] = check;
    }
    
    HealthStatus check_health() {
        HealthStatus status;
        status.healthy = true;
        
        for (const auto& [name, check] : checks_) {
            bool ok = check();
            status.dependencies[name] = ok;
            if (!ok) {
                status.healthy = false;
                status.message += name + " unhealthy; ";
            }
        }
        
        return status;
    }
    
    // HTTP 处理函数
    void handle_health(http::response<http::string_body>& response) {
        auto status = check_health();
        
        json j;
        j["status"] = status.healthy ? "healthy" : "unhealthy";
        j["message"] = status.message;
        j["dependencies"] = status.dependencies;
        j["timestamp"] = std::time(nullptr);
        
        response.result(status.healthy ? http::status::ok : http::status::service_unavailable);
        response.body() = j.dump();
    }

private:
    std::map<std::string, std::function<bool()>> checks_;
};
```

### 3.3 日志系统

```cpp
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>

class Logger {
public:
    static void init(const std::string& log_level = "info") {
        // 控制台输出
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(spdlog::level::from_str(log_level));
        
        // 文件输出（轮转，最多 5 个文件，每个 10MB）
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            "logs/nuclaw.log", 1024*1024*10, 5
        );
        file_sink->set_level(spdlog::level::debug);
        
        // 组合
        std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};
        auto logger = std::make_shared<spdlog::logger>("nuclaw", sinks.begin(), sinks.end());
        
        spdlog::set_default_logger(logger);
        spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%s:%#] %v");
    }
    
    // 结构化日志
    static void log_request(const std::string& session_id,
                           const std::string& method,
                           const std::string& path,
                           int status_code,
                           double duration_ms) {
        spdlog::info(
            R"({{"event": "request", "session_id": "{}", )"
            R"("method": "{}", "path": "{}", )"
            R"("status": {}, "duration_ms": {:.2f}}})",
            session_id, method, path, status_code, duration_ms
        );
    }
};
```

### 3.4 性能计时器

```cpp
class ScopedTimer {
public:
    ScopedTimer(const std::string& name,
                std::function<void(double)> callback)
        : name_(name), callback_(callback),
          start_(std::chrono::high_resolution_clock::now()) {}
    
    ~ScopedTimer() {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<
            std::chrono::microseconds>(end - start_).count() / 1000.0;
        callback_(duration);
    }

private:
    std::string name_;
    std::function<void(double)> callback_;
    std::chrono::high_resolution_clock::time_point start_;
};

// 使用示例
void process_request() {
    ScopedTimer timer("process_request", [](double ms) {
        spdlog::debug("Request processed in {:.2f}ms", ms);
    });
    
    // 处理逻辑...
}
```

### 3.5 集成到 Server

```cpp
class MonitoredServer : public Server {
public:
    MonitoredServer(io_context& io, uint16_t port)
        : Server(io, port),
          metrics_(9090) {
        
        // 注册健康检查
        health_.register_check("database", [this]() {
            return db_.is_connected();
        });
        
        health_.register_check("llm_api", [this]() {
            return llm_.is_healthy();
        });
    }
    
protected:
    void on_request(http::request<http::string_body>& req,
                   http::response<http::string_body>& resp) override {
        auto start = std::chrono::high_resolution_clock::now();
        
        // 记录连接
        metrics_.connection_opened();
        
        // 处理请求
        if (req.target() == "/health") {
            health_.handle_health(resp);
        } else if (req.target() == "/metrics") {
            // Prometheus 自动处理
        } else {
            Server::on_request(req, resp);
        }
        
        // 计算耗时
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<
            std::chrono::milliseconds>(end - start).count() / 1000.0;
        
        // 记录指标
        metrics_.record_request(
            std::string(req.target()),
            std::string(req.method_string()),
            resp.result_int(),
            duration
        );
        
        // 记录日志
        Logger::log_request(
            "unknown",  // 从 session 获取
            std::string(req.method_string()),
            std::string(req.target()),
            resp.result_int(),
            duration * 1000
        );
        
        metrics_.connection_closed();
    }

private:
    MetricsCollector metrics_;
    HealthChecker health_;
};
```

---

## 四、Grafana 监控面板

```json
{
  "dashboard": {
    "title": "NuClaw Dashboard",
    "panels": [
      {
        "title": "QPS",
        "targets": [{
          "expr": "rate(http_requests_total[5m])"
        }]
      },
      {
        "title": "Latency P99",
        "targets": [{
          "expr": "histogram_quantile(0.99, rate(http_request_duration_seconds_bucket[5m]))"
        }]
      },
      {
        "title": "Active Connections",
        "targets": [{
          "expr": "active_connections"
        }]
      },
      {
        "title": "Error Rate",
        "targets": [{
          "expr": "rate(http_requests_total{status=~\"5..\"}[5m])"
        }]
      }
    ]
  }
}
```

---

## 五、本章总结

- ✅ Prometheus 指标收集
- ✅ 健康检查端点
- ✅ 结构化日志系统
- ✅ 性能计时和分析
- ✅ 代码从 900 行扩展到 1000 行

---

## 六、课后思考

至此，我们已经构建了一个完整的 AI Agent 系统：
- ✅ 异步 I/O 高性能服务器
- ✅ HTTP/WebSocket 通信
- ✅ JSON 数据交互
- ✅ LLM 接入和工具调用
- ✅ 会话管理和用户隔离
- ✅ 短期记忆和长期记忆
- ✅ 多 Agent 协作
- ✅ 安全和 API 管理
- ✅ 数据持久化
- ✅ 监控和可观测性

接下来的章节将介绍：
- **Step 13**: Docker 部署
- **Step 14**: CI/CD 流程
- **Step 15**: 性能优化
- **Step 16**: 最佳实践总结

<details>
<summary>点击查看后续章节 💡</summary>

**Step 13-16 预告：**

- Step 13: Docker 容器化部署
- Step 14: CI/CD 持续集成/部署
- Step 15: 性能优化技巧
- Step 16: 最佳实践总结

</details>
