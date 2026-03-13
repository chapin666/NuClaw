# Step 13: 监控告警 - 了解 Agent 的运行状态

> 目标：实现可观测性，掌握生产环境监控
> 
> 难度：⭐⭐⭐⭐⭐ (困难)
> 
> 代码量：约 950 行
> 
> 预计学习时间：5-6 小时

---

## 📚 前置知识

### 生产环境的挑战

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

### 可观测性三大支柱

```
┌─────────────────────────────────────────────────┐
│              可观测性三大支柱                    │
├─────────────────────────────────────────────────┤
│                                                 │
│  Metrics (指标)    Logging (日志)   Tracing (链路)│
│      │                  │                │      │
│      ▼                  ▼                ▼      │
│  "系统的脉搏"      "系统的记忆"    "系统的足迹"   │
│                                                 │
│  请求数、响应时间    错误日志        请求链路     │
│  错误率、资源使用    访问日志        性能瓶颈     │
│                                                 │
└─────────────────────────────────────────────────┘
```

### 类比：体检报告

```
Metrics = 血压、心率（量化指标）
Logging = 病历记录（事件记录）
Tracing = 检查流程（步骤追踪）
```

---

## 第一步：Metrics 指标

### 指标类型

| 类型 | 特点 | 示例 |
|:---|:---|:---|
| **Counter** | 只增不减 | 请求总数、错误总数 |
| **Gauge** | 可增可减 | 当前连接数、内存使用 |
| **Histogram** | 分布统计 | 响应时间分布 |

### 在 Agent 中埋点

```cpp
class MonitoredChatEngine {
public:
    MonitoredChatEngine() {
        // 初始化指标
        request_counter_ = Metrics::counter("agent_requests_total");
        active_requests_ = Metrics::gauge("agent_active_requests");
        error_counter_ = Metrics::counter("agent_errors_total");
    }
    
    string process(const string& input) {
        request_counter_>increment();
        active_requests_>increment();
        
        auto start = chrono::steady_clock::now();
        
        try {
            string reply = do_process(input);
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

### 日志级别

```
DEBUG   - 调试信息（开发时使用）
INFO    - 正常操作记录
WARN    - 警告，可能的问题
ERROR   - 错误，需要处理
FATAL   - 致命错误
```

### 结构化日志

**非结构化：**
```
2024-01-15 10:30:00 User alice login failed from 192.168.1.1
```

**结构化（JSON）：**
```json
{
  "timestamp": "2024-01-15T10:30:00Z",
  "level": "WARN",
  "event": "login_failed",
  "user": "alice",
  "ip": "192.168.1.1"
}
```

**优势：** 方便检索、聚合、分析

---

## 第三步：监控告警

### 告警规则设计

**分层告警：**
```
P0 - 立即处理：服务不可用
P1 - 尽快处理：错误率 > 5%
P2 - 白天处理：磁盘使用率 > 80%
```

**避免告警疲劳：**
```
❌ 不好的告警：CPU > 50%
✅ 好的告警：CPU > 80% 持续 5 分钟
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

### 练习：自定义业务指标
为你关注的业务指标（如订单量）添加监控。

### 思考题
1. 可观测性和监控有什么区别？
2. 如何在性能和可观测性之间平衡？

---

## 📖 扩展阅读

- **Prometheus + Grafana**：Metrics 监控标配
- **ELK Stack**：日志收集与分析
- **Jaeger**：分布式追踪
- **OpenTelemetry**：统一的可观测性标准
