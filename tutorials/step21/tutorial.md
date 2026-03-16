# Step 21: 智能客服 SaaS 平台 —— 生产部署

> 目标：完成 Docker 容器化、K8s 部署、监控配置、性能优化
> 
003e 难度：⭐⭐⭐⭐ | 代码量：约 800 行（配置）| 预计学习时间：3-4 小时

---

## 一、Docker 容器化

### 1.1 多阶段 Dockerfile

```dockerfile
# ═══════════════════════════════════════════════════════════
# 阶段 1：构建环境
# ═══════════════════════════════════════════════════════════
FROM ubuntu:22.04 AS builder

# 安装编译依赖
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    libboost-all-dev \
    libssl-dev \
    libpqxx-dev \
    && rm -rf /var/lib/apt/lists/*

# 工作目录
WORKDIR /build

# 复制源码
COPY CMakeLists.txt .
COPY src ./src
COPY config ./config

# 编译（Release 模式，启用优化）
RUN mkdir -p build && cd build \
    && cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_FLAGS="-O3 -march=native -DNDEBUG" \
    && make -j$(nproc)

# 运行测试
RUN cd build && ctest --output-on-failure

# ═══════════════════════════════════════════════════════════
# 阶段 2：运行环境
# ═══════════════════════════════════════════════════════════
FROM ubuntu:22.04

# 安装运行时依赖（最小化）
RUN apt-get update && apt-get install -y \
    libboost-system1.74.0 \
    libboost-thread1.74.0 \
    libssl3 \
    libpq5 \
    curl \
    && rm -rf /var/lib/apt/lists/* \
    && apt-get clean

# 创建非 root 用户
RUN useradd -m -u 1000 -s /bin/bash smartsupport

# 工作目录
WORKDIR /app

# 从构建阶段复制产物
COPY --from=builder /build/build/smartsupport ./
COPY --from=builder /build/config ./config

# 创建数据和日志目录
RUN mkdir -p /app/data /app/logs \
    && chown -R smartsupport:smartsupport /app

# 切换到非 root 用户
USER smartsupport

# 暴露端口
EXPOSE 8080 9090

# 健康检查
HEALTHCHECK --interval=30s --timeout=5s --start-period=10s --retries=3 \
    CMD curl -f http://localhost:8080/health || exit 1

# 启动命令
ENTRYPOINT ["./smartsupport"]
CMD ["--config", "./config/production.yaml"]
```

### 1.2 Docker Compose 本地部署

```yaml
# docker-compose.yml
version: '3.8'

services:
  # ───────────────────────────────────────────────────────
  # SmartSupport 主服务
  # ───────────────────────────────────────────────────────
  app:
    build:
      context: .
      dockerfile: Dockerfile
    image: smartsupport:latest
    container_name: smartsupport-app
    restart: unless-stopped
    
    ports:
      - "8080:8080"    # HTTP API
      - "9090:9090"    # Metrics
    
    environment:
      - DATABASE_URL=postgresql://postgres:postgres@postgres:5432/smartsupport
      - REDIS_URL=redis://redis:6379
      - OPENAI_API_KEY=${OPENAI_API_KEY}
      - LOG_LEVEL=info
    
    volumes:
      - ./logs:/app/logs
      - ./data:/app/data
    
    depends_on:
      postgres:
        condition: service_healthy
      redis:
        condition: service_healthy
    
    networks:
      - smartsupport-network
    
    deploy:
      resources:
        limits:
          cpus: '2'
          memory: 2G
        reservations:
          cpus: '0.5'
          memory: 512M

  # ───────────────────────────────────────────────────────
  # PostgreSQL（主数据库 + 向量扩展）
  # ───────────────────────────────────────────────────────
  postgres:
    image: ankane/pgvector:latest
    container_name: smartsupport-postgres
    restart: unless-stopped
    
    environment:
      POSTGRES_USER: postgres
      POSTGRES_PASSWORD: postgres
      POSTGRES_DB: smartsupport
    
    volumes:
      - postgres-data:/var/lib/postgresql/data
      - ./init-scripts:/docker-entrypoint-initdb.d:ro
    
    ports:
      - "5432:5432"
    
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U postgres"]
      interval: 5s
      timeout: 5s
      retries: 5
    
    networks:
      - smartsupport-network

  # ───────────────────────────────────────────────────────
  # Redis（缓存 + 会话存储）
  # ───────────────────────────────────────────────────────
  redis:
    image: redis:7-alpine
    container_name: smartsupport-redis
    restart: unless-stopped
    
    command: redis-server --appendonly yes --maxmemory 256mb --maxmemory-policy allkeys-lru
    
    volumes:
      - redis-data:/data
    
    ports:
      - "6379:6379"
    
    healthcheck:
      test: ["CMD", "redis-cli", "ping"]
      interval: 5s
      timeout: 3s
      retries: 5
    
    networks:
      - smartsupport-network

  # ───────────────────────────────────────────────────────
  # MinIO（对象存储）
  # ───────────────────────────────────────────────────────
  minio:
    image: minio/minio:latest
    container_name: smartsupport-minio
    restart: unless-stopped
    
    command: server /data --console-address ":9001"
    
    environment:
      MINIO_ROOT_USER: minioadmin
      MINIO_ROOT_PASSWORD: minioadmin
    
    volumes:
      - minio-data:/data
    
    ports:
      - "9000:9000"
      - "9001:9001"
    
    networks:
      - smartsupport-network

  # ───────────────────────────────────────────────────────
  # Prometheus（监控）
  # ───────────────────────────────────────────────────────
  prometheus:
    image: prom/prometheus:latest
    container_name: smartsupport-prometheus
    restart: unless-stopped
    
    volumes:
      - ./monitoring/prometheus.yml:/etc/prometheus/prometheus.yml:ro
      - prometheus-data:/prometheus
    
    command:
      - '--config.file=/etc/prometheus/prometheus.yml'
      - '--storage.tsdb.path=/prometheus'
      - '--web.console.libraries=/usr/share/prometheus/console_libraries'
      - '--web.console.templates=/usr/share/prometheus/consoles'
      - '--web.enable-lifecycle'
    
    ports:
      - "9091:9090"
    
    networks:
      - smartsupport-network

  # ───────────────────────────────────────────────────────
  # Grafana（可视化）
  # ───────────────────────────────────────────────────────
  grafana:
    image: grafana/grafana:latest
    container_name: smartsupport-grafana
    restart: unless-stopped
    
    environment:
      - GF_SECURITY_ADMIN_PASSWORD=${GRAFANA_PASSWORD:-admin}
      - GF_USERS_ALLOW_SIGN_UP=false
    
    volumes:
      - grafana-data:/var/lib/grafana
      - ./monitoring/grafana/dashboards:/etc/grafana/provisioning/dashboards:ro
      - ./monitoring/grafana/datasources:/etc/grafana/provisioning/datasources:ro
    
    ports:
      - "3000:3000"
    
    networks:
      - smartsupport-network
    
    depends_on:
      - prometheus

volumes:
  postgres-data:
  redis-data:
  minio-data:
  prometheus-data:
  grafana-data:

networks:
  smartsupport-network:
    driver: bridge
```

---

## 二、Kubernetes 部署

### 2.1 Namespace 和 ConfigMap

```yaml
# k8s/00-namespace.yaml
apiVersion: v1
kind: Namespace
metadata:
  name: smartsupport
  labels:
    name: smartsupport
---
# k8s/01-configmap.yaml
apiVersion: v1
kind: ConfigMap
metadata:
  name: smartsupport-config
  namespace: smartsupport
data:
  DATABASE_URL: "postgresql://postgres:postgres@postgres:5432/smartsupport"
  REDIS_URL: "redis://redis:6379"
  LOG_LEVEL: "info"
  METRICS_PORT: "9090"
  HTTP_PORT: "8080"
```

### 2.2 应用部署

```yaml
# k8s/10-deployment.yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: smartsupport-app
  namespace: smartsupport
  labels:
    app: smartsupport
    component: api
spec:
  replicas: 3  # 3 个副本实现高可用
  strategy:
    type: RollingUpdate
    rollingUpdate:
      maxSurge: 1
      maxUnavailable: 0
  selector:
    matchLabels:
      app: smartsupport
      component: api
  template:
    metadata:
      labels:
        app: smartsupport
        component: api
      annotations:
        prometheus.io/scrape: "true"
        prometheus.io/port: "9090"
        prometheus.io/path: "/metrics"
    spec:
      containers:
        - name: app
          image: smartsupport:latest
          imagePullPolicy: Always
          
          ports:
            - name: http
              containerPort: 8080
              protocol: TCP
            - name: metrics
              containerPort: 9090
              protocol: TCP
          
          envFrom:
            - configMapRef:
                name: smartsupport-config
          
          env:
            - name: OPENAI_API_KEY
              valueFrom:
                secretKeyRef:
                  name: smartsupport-secrets
                  key: openai-api-key
          
          resources:
            requests:
              memory: "512Mi"
              cpu: "500m"
            limits:
              memory: "2Gi"
              cpu: "2000m"
          
          livenessProbe:
            httpGet:
              path: /health
              port: http
            initialDelaySeconds: 30
            periodSeconds: 10
          
          readinessProbe:
            httpGet:
              path: /ready
              port: http
            initialDelaySeconds: 5
            periodSeconds: 5
          
          volumeMounts:
            - name: logs
              mountPath: /app/logs
      
      volumes:
        - name: logs
          emptyDir: {}
---
# k8s/11-service.yaml
apiVersion: v1
kind: Service
metadata:
  name: smartsupport-service
  namespace: smartsupport
  labels:
    app: smartsupport
spec:
  type: ClusterIP
  ports:
    - port: 8080
      targetPort: http
      protocol: TCP
      name: http
    - port: 9090
      targetPort: metrics
      protocol: TCP
      name: metrics
  selector:
    app: smartsupport
    component: api
---
# k8s/12-ingress.yaml
apiVersion: networking.k8s.io/v1
kind: Ingress
metadata:
  name: smartsupport-ingress
  namespace: smartsupport
  annotations:
    nginx.ingress.kubernetes.io/ssl-redirect: "true"
    nginx.ingress.kubernetes.io/proxy-body-size: "10m"
    nginx.ingress.kubernetes.io/rate-limit: "100"
spec:
  ingressClassName: nginx
  tls:
    - hosts:
        - api.smartsupport.io
      secretName: smartsupport-tls
  rules:
    - host: api.smartsupport.io
      http:
        paths:
          - path: /
            pathType: Prefix
            backend:
              service:
                name: smartsupport-service
                port:
                  number: 8080
```

### 2.3 数据库（StatefulSet）

```yaml
# k8s/20-postgres.yaml
apiVersion: apps/v1
kind: StatefulSet
metadata:
  name: postgres
  namespace: smartsupport
spec:
  serviceName: postgres
  replicas: 1
  selector:
    matchLabels:
      app: postgres
  template:
    metadata:
      labels:
        app: postgres
    spec:
      containers:
        - name: postgres
          image: ankane/pgvector:latest
          env:
            - name: POSTGRES_USER
              value: postgres
            - name: POSTGRES_PASSWORD
              valueFrom:
                secretKeyRef:
                  name: smartsupport-secrets
                  key: db-password
            - name: POSTGRES_DB
              value: smartsupport
          ports:
            - containerPort: 5432
          volumeMounts:
            - name: postgres-storage
              mountPath: /var/lib/postgresql/data
  volumeClaimTemplates:
    - metadata:
        name: postgres-storage
      spec:
        accessModes: ["ReadWriteOnce"]
        resources:
          requests:
            storage: 10Gi
---
apiVersion: v1
kind: Service
metadata:
  name: postgres
  namespace: smartsupport
spec:
  selector:
    app: postgres
  ports:
    - port: 5432
```

### 2.4 HPA 自动扩缩容

```yaml
# k8s/30-hpa.yaml
apiVersion: autoscaling/v2
kind: HorizontalPodAutoscaler
metadata:
  name: smartsupport-hpa
  namespace: smartsupport
spec:
  scaleTargetRef:
    apiVersion: apps/v1
    kind: Deployment
    name: smartsupport-app
  minReplicas: 3
  maxReplicas: 20
  metrics:
    - type: Resource
      resource:
        name: cpu
        target:
          type: Utilization
          averageUtilization: 70
    - type: Resource
      resource:
        name: memory
        target:
          type: Utilization
          averageUtilization: 80
  behavior:
    scaleUp:
      stabilizationWindowSeconds: 60
      policies:
        - type: Percent
          value: 100
          periodSeconds: 15
    scaleDown:
      stabilizationWindowSeconds: 300
      policies:
        - type: Percent
          value: 10
          periodSeconds: 60
```

---

## 三、监控配置

### 3.1 Prometheus 配置

```yaml
# monitoring/prometheus.yml
global:
  scrape_interval: 15s
  evaluation_interval: 15s

alerting:
  alertmanagers:
    - static_configs:
        - targets: ['alertmanager:9093']

rule_files:
  - /etc/prometheus/rules/*.yml

scrape_configs:
  # SmartSupport 应用
  - job_name: 'smartsupport'
    kubernetes_sd_configs:
      - role: pod
        namespaces:
          names:
            - smartsupport
    relabel_configs:
      - source_labels: [__meta_kubernetes_pod_annotation_prometheus_io_scrape]
        action: keep
        regex: true
      - source_labels: [__meta_kubernetes_pod_annotation_prometheus_io_port]
        action: replace
        target_label: __address__
        regex: ([^:]+)(?::\d+)?;(\d+)
        replacement: $1:$2

  # PostgreSQL
  - job_name: 'postgres'
    static_configs:
      - targets: ['postgres-exporter:9187']

  # Redis
  - job_name: 'redis'
    static_configs:
      - targets: ['redis-exporter:9121']

  # 节点监控
  - job_name: 'node'
    static_configs:
      - targets: ['node-exporter:9100']
```

### 3.2 告警规则

```yaml
# monitoring/rules/alerts.yml
groups:
  - name: smartsupport
    rules:
      # 高错误率告警
      - alert: HighErrorRate
        expr: rate(http_requests_total{status=~"5.."}[5m]) / rate(http_requests_total[5m]) > 0.05
        for: 5m
        labels:
          severity: critical
        annotations:
          summary: "High error rate detected"
          description: "Error rate is above 5% for more than 5 minutes"
      
      # 高延迟告警
      - alert: HighLatency
        expr: histogram_quantile(0.95, rate(http_request_duration_seconds_bucket[5m])) > 2
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "High latency detected"
          description: "95th percentile latency is above 2 seconds"
      
      # 服务不可用
      - alert: ServiceDown
        expr: up{job="smartsupport"} == 0
        for: 1m
        labels:
          severity: critical
        annotations:
          summary: "Service is down"
          description: "SmartSupport service is not responding"
      
      # 数据库连接池耗尽
      - alert: DBPoolExhausted
        expr: db_pool_active_connections / db_pool_max_connections > 0.9
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "Database connection pool almost exhausted"
```

---

## 四、性能优化

### 4.1 数据库优化

```sql
-- 添加索引
CREATE INDEX CONCURRENTLY idx_messages_tenant_time 
    ON messages(tenant_id, created_at DESC);

CREATE INDEX CONCURRENTLY idx_conversations_tenant_status 
    ON conversations(tenant_id, status) 
    WHERE status = 'active';

-- 向量索引（pgvector）
CREATE INDEX CONCURRENTLY idx_knowledge_embedding 
    ON knowledge_docs 
    USING ivfflat (embedding vector_cosine_ops)
    WITH (lists = 100);

-- 分区表（按时间）
CREATE TABLE messages_2024_03 PARTITION OF messages
    FOR VALUES FROM ('2024-03-01') TO ('2024-04-01');
```

### 4.2 应用层优化

```cpp
// 连接池配置
class ConnectionPool {
public:
    struct Config {
        size_t min_size = 5;           // 最小连接数
        size_t max_size = 50;          // 最大连接数
        std::chrono::seconds max_idle_time{300};  // 最大空闲时间
        std::chrono::seconds connection_timeout{5};  // 连接超时
    };
};

// 缓存策略
class CacheManager {
public:
    // 租户配置缓存（很少变化）
    LRUCache<std::string, TenantConfig> tenant_config_cache{1000, std::chrono::minutes(5)};
    
    // 知识库热点缓存
    LRUCache<std::string, std::vector<KnowledgeDoc>> knowledge_cache{100, std::chrono::minutes(1)};
    
    // 会话状态缓存（Redis）
    std::shared_ptr<RedisClient> session_cache;
};

// 异步批量处理
class BatchProcessor {
public:
    // 批量写入日志
    void enqueue_log(const LogEntry& entry);
    
    // 批量记录用量
    void enqueue_usage(const UsageRecord& record);
    
    // 定时刷新（每秒）
    void flush_batch();
};
```

---

## 五、压测与调优

### 5.1 压测脚本

```bash
#!/bin/bash
# benchmark.sh

# 安装 k6
# wget -qO- https://dl.k6.io/key.gpg | gpg --dearmor | sudo tee /usr/share/keyrings/k6-archive-keyring.gpg
# echo "deb [signed-by=/usr/share/keyrings/k6-archive-keyring.gpg] https://dl.k6.io/deb stable main" | sudo tee /etc/apt/sources.list.d/k6.list
# sudo apt-get update
# sudo apt-get install k6

# 压测脚本
k6 run --vus 100 --duration 5m - <<EOF
import http from 'k6/http';
import { check, sleep } from 'k6';

export default function () {
  const payload = JSON.stringify({
    message: "你好，我想了解产品价格",
    tenant_id: "test-tenant"
  });
  
  const res = http.post('http://localhost:8080/api/chat', payload, {
    headers: { 'Content-Type': 'application/json' },
  });
  
  check(res, {
    'status is 200': (r) => r.status === 200,
    'response time < 3s': (r) => r.timings.duration < 3000,
  });
  
  sleep(1);
}
EOF
```

### 5.2 调优清单

```
压测结果分析：
┌─────────────────┬────────────┬────────────┬──────────┐
│ 指标            │ 目标       │ 当前       │ 状态     │
├─────────────────┼────────────┼────────────┼──────────┤
│ P99 延迟        │ < 3s       │ 2.5s       │ ✅ 达标  │
│ 吞吐量          │ > 1000 QPS │ 1200 QPS   │ ✅ 达标  │
│ 错误率          │ < 1%        │ 0.5%       │ ✅ 达标  │
│ CPU 使用率      │ < 70%       │ 65%        │ ✅ 达标  │
│ 内存使用        │ < 2GB       │ 1.8GB      │ ✅ 达标  │
└─────────────────┴────────────┴────────────┴──────────┘

优化措施：
1. 数据库连接池从 10 调整到 50（减少连接等待）
2. 添加 Redis 缓存层（命中 60%）
3. 启用 HTTP Keep-Alive（减少 TCP 握手）
4. RAG Top-K 从 10 降到 5（减少向量检索时间）
```

---

## 六、本章小结

**核心收获：**

1. **Docker 化**：
   - 多阶段构建减小镜像体积
   - Docker Compose 本地部署

2. **K8s 部署**：
   - Deployment/Service/Ingress
   - StatefulSet 部署数据库
   - HPA 自动扩缩容

3. **监控告警**：
   - Prometheus 采集指标
   - Grafana 可视化
   - 告警规则配置

4. **性能优化**：
   - 数据库索引与分区
   - 缓存策略
   - 批量处理
   - 压测调优

---

## 七、课程完结

**SmartSupport 项目总结：**

```
从 0 到 1 构建的 SaaS 智能客服平台

技术栈：
• C++17 / Boost.Asio / CMake
• PostgreSQL + pgvector
• Redis
• Docker / Kubernetes
• Prometheus / Grafana

核心能力：
• AI 自动回复（RAG + LLM）
• 人工客服接入
• 多租户隔离
• 计费与套餐
• 管理后台

生产就绪：
• 容器化部署
• 自动扩缩容
• 监控告警
• 性能优化
```

**恭喜你完成了 NuClaw 全系列教程！**

从 Step 0 的 Echo 服务器，到 Step 20 的生产级 SaaS 平台，你已经掌握了 AI Agent 开发的完整技能栈。

**去构建属于你自己的产品吧！** 🚀
