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

**传统调试的局限：**
- 本地无法复现生产问题
- 日志太多，找不到关键信息
- 分布式系统，请求链路复杂

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
│  • 量化系统状态      • 记录关键事件      • 追踪请求链路    │
│  • 发现趋势异常      • 故障复盘分析      • 定位性能瓶颈    │
│  • 驱动决策优化      • 安全审计追踪      • 理解系统依赖    │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

**类比：体检报告**
```
Metrics = 血压、心率（量化指标）
Logging = 病历记录（事件记录）
Tracing = 检查流程（步骤追踪）
```

### 三大支柱对比

| 维度 | Metrics | Logging | Tracing |
|:---|:---|:---|:---|
| **数据类型** | 数值（可聚合） | 文本（离散事件） | 结构化（调用链） |
| **存储成本** | 低（聚合后） | 高（原始文本） | 中（采样存储） |
| **查询方式** | 时序查询 | 全文检索 | 链路查询 |
| **使用场景** | 监控告警 | 故障排查 | 性能分析 |
| **保留时间** | 长期（压缩） | 中期（分档） | 短期（采样） |

---

## 第一步：Metrics（指标监控）

### 指标类型

**1. Counter（计数器）**
```
特点：只增不减，可重置
用途：请求总数、错误总数

示例：
  http_requests_total{method="GET"} 1000
  http_errors_total{code="500"} 5
```

**2. Gauge（仪表盘）**
```
特点：可增可减
用途：当前连接数、内存使用量

示例：
  active_connections 50
  memory_usage_bytes 1024000
```

**3. Histogram（直方图）**
```
特点：分布统计，分桶计数
用途：响应时间分布

示例：
  http_request_duration_bucket{le="0.1"} 100
  http_request_duration_bucket{le="0.5"} 800
  http_request_duration_bucket{le="1.0"} 950
  http_request_duration_sum 450.5
  http_request_duration_count 1000
```

### 关键指标设计

**RED 方法（面向服务）：**
```
Rate（请求率）：每秒请求数
Errors（错误率）：错误请求占比
Duration（延迟）：请求处理时间
```

**USE 方法（面向资源）：**
```
Utilization（使用率）：资源使用百分比
Saturation（饱和度）：排队/等待的负载
Errors（错误）：错误计数
```

---

## 第二步：Logging（日志系统）

### 日志级别

```
DEBUG   - 调试信息，开发时使用
INFO    - 正常操作记录
WARN    - 警告，可能的问题
ERROR   - 错误，需要处理
FATAL   - 致命错误，程序退出
```

**使用建议：**
- 生产环境：INFO 及以上
- 调试环境：DEBUG 及以上
- 错误日志必须包含上下文

### 结构化日志

**非结构化（难解析）：**
```
2024-01-15 10:30:00 User alice login failed from 192.168.1.1
```

**结构化（易解析）：**
```json
{
  "timestamp": "2024-01-15T10:30:00Z",
  "level": "WARN",
  "event": "login_failed",
  "user": "alice",
  "ip": "192.168.1.1",
  "reason": "invalid_password"
}
```

**优势：**
- 方便检索：`event=login_failed`
- 方便聚合：统计登录失败次数
- 方便分析：关联用户行为

### 日志最佳实践

**1. 包含关键字段**
```json
{
  "timestamp": "ISO8601格式",
  "level": "日志级别",
  "service": "服务名",
  "trace_id": "追踪ID",
  "message": "日志消息",
  "context": { "key": "value" }
}
```

**2. 避免记录敏感信息**
```
❌ 不要记录：密码、token、身份证号
✅ 脱敏后记录：phone=138****8888
```

**3. 适当的日志量**
```
❌ 太多：每个循环都打印
❌ 太少：关键路径无日志
✅ 平衡：关键节点记录，可配置详细度
```

---

## 第三步：Tracing（链路追踪）

### 为什么需要链路追踪？

**微服务场景：**
```
用户请求 → API Gateway → Service A → Service B → Database
                ↓              ↓           ↓
           日志分散在各个服务，如何串联？

解决方案：每个请求分配唯一 Trace ID，贯穿所有服务
```

### 核心概念

**Trace（追踪）：**
```
完整的请求链路，包含多个 Span
示例：用户下单流程
```

**Span（跨度）：**
```
链路中的一个操作单元
包含：开始时间、结束时间、操作名、标签

示例：
  Span: 查询数据库
  - 开始：10:00:00.000
  - 结束：10:00:00.050
  - 耗时：50ms
```

**Trace Context：**
```
Trace ID: 贯穿整个链路的唯一标识
Span ID: 当前操作的标识
Parent Span ID: 父操作的标识
```

### 链路示例

```
Trace: order_123
├── Span: API Gateway (0ms - 5ms)
│   └── Span: Auth Check (1ms - 3ms)
├── Span: Order Service (5ms - 45ms)
│   ├── Span: Validate Request (6ms - 8ms)
│   ├── Span: Query User (8ms - 15ms)
│   ├── Span: Query Inventory (8ms - 20ms)
│   └── Span: Save Order (20ms - 45ms)
└── Span: Payment Service (45ms - 100ms)
    └── Span: Process Payment (46ms - 95ms)

分析：
- 总耗时：100ms
- 瓶颈：Payment Service（55ms）
- 可优化：Query Inventory（12ms）和 Save Order（25ms）
```

---

## 第四步：监控告警策略

### 告警设计原则

**1. 分层告警**
```
P0 - 立即处理：服务不可用
P1 - 尽快处理：错误率 > 5%
P2 - 白天处理：磁盘使用率 > 80%
P3 - 观察：性能轻微下降
```

**2. 避免告警疲劳**
```
❌ 不好的告警：
   - CPU > 50%（太敏感，频繁触发）
   - 瞬时错误（可能是网络抖动）

✅ 好的告警：
   - CPU > 80% 持续 5 分钟
   - 错误率 > 1% 持续 3 分钟
```

**3. 告警可行动**
```
❌ 不好的告警："服务异常"
✅ 好的告警："订单服务错误率 10%，建议检查数据库连接"
```

### 常用告警规则

```yaml
# 响应时间告警
alert: HighLatency
expr: histogram_quantile(0.95, http_request_duration) > 1
for: 5m
severity: warning

# 错误率告警
alert: HighErrorRate
expr: rate(http_errors_total[5m]) / rate(http_requests_total[5m]) > 0.05
for: 3m
severity: critical

# 资源告警
alert: HighMemoryUsage
expr: memory_usage / memory_limit > 0.85
for: 10m
severity: warning
```

---

## 第五节：可观测性平台

### 开源方案

**Metrics：**
- Prometheus（收集 + 存储）
- Grafana（可视化）

**Logging：**
- ELK Stack（Elasticsearch + Logstash + Kibana）
- Loki（轻量级，与 Grafana 集成）

**Tracing：**
- Jaeger（Uber 开源）
- Zipkin（Twitter 开源）
- Tempo（Grafana 出品）

### 统一方案：OpenTelemetry

**OpenTelemetry = OpenTracing + OpenCensus**

**优势：**
- 统一标准，避免厂商锁定
- 支持自动埋点
- 与各种后端集成

---

## 本节总结

### 核心概念

1. **Metrics**：量化指标，用于监控和告警
2. **Logging**：事件记录，用于故障排查
3. **Tracing**：请求链路，用于性能分析

### 使用场景

| 问题 | 使用什么 | 例子 |
|:---|:---|:---|
| 服务是否健康？ | Metrics | CPU、内存、QPS |
| 为什么出错？ | Logging | 错误日志、堆栈 |
| 哪里慢？ | Tracing | 调用链路耗时 |

### 最佳实践

1. **Metrics**：关注关键指标，避免过多维度
2. **Logging**：结构化输出，分级记录
3. **Tracing**：采样策略，避免全量采集
4. **统一 Trace ID**：关联 Metrics、Logs、Traces

---

## 📝 课后练习

### 练习 1：自定义指标
为你关注的业务指标（如订单量、用户活跃度）添加监控。

### 练习 2：日志关联
确保所有日志包含 Trace ID，实现 Metrics → Logs → Traces 跳转。

### 练习 3：告警模板
设计告警通知模板，包含问题描述、影响范围、处理建议。

### 思考题
1. 可观测性和监控有什么区别？
2. 采样策略如何设计？全量采集有什么问题？
3. 如何在性能和可观测性之间平衡？

---

**恭喜！** 你的 Agent 现在具备了完整的可观测性。下一章我们将学习部署和运维。
