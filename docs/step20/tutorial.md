# Step 20: 生产部署 —— Docker 与 Kubernetes

> 目标：将 Step 19 的 SaaS 平台部署到生产环境
> 难度：⭐⭐⭐⭐⭐
> 代码量：约 800 行（配置文件为主）

---

## 问题引入

### Step 19 的局限

Step 19 实现了完整的 6 服务 SaaS 平台，但只能在开发环境运行：

```bash
# 开发环境：手动启动
./step19_demo
```

**问题：**
1. **单点故障** - 进程崩溃服务就中断
2. **无法扩展** - 用户量增长时只能换更强的机器（垂直扩展）
3. **配置混乱** - 数据库连接、API Key 等配置硬编码
4. **没有监控** - 服务是否健康？性能如何？一无所知

### 本章目标

**生产级部署**：
- **Docker 容器化** - 标准化运行环境
- **Docker Compose** - 本地一键启动所有服务
- **Kubernetes** - 生产环境水平扩展、高可用
- **监控告警** - Prometheus + Grafana

---

## 解决方案

### 部署架构演进

```
开发环境（Step 19）:
┌─────────────────────────────────────────┐
│  手动运行 ./step19_demo                  │
│  - 单进程                                │
│  - 本地数据库                            │
│  - 无监控                                │
└─────────────────────────────────────────┘

容器化（Docker Compose）:
┌─────────────────────────────────────────────────────────────┐
│  docker-compose up                                          │
│                                                             │
│  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌───────┐ ┌─────────┐│
│  │  Chat   │ │   AI    │ │Knowledge│ │Human  │ │ Billing ││
│  │ Service │ │ Service │ │ Service │ │Service│ │ Service ││
│  └────┬────┘ └────┬────┘ └────┬────┘ └───┬───┘ └────┬────┘│
│       └─────────────┴──────────┴──────────┴───────────┘    │
│                         │                                   │
│                    ┌────┴────┐                              │
│                    │  Nginx  │  ← 网关                       │
│                    └────┬────┘                              │
│                         │                                   │
│       ┌─────────┐  ┌────┴────┐  ┌─────────┐                │
│       │PostgreSQL│  │  Redis  │  │Prometheus│                │
│       └─────────┘  └─────────┘  └─────────┘                │
│                                                             │
└─────────────────────────────────────────────────────────────┘

生产环境（Kubernetes）:
┌─────────────────────────────────────────────────────────────────┐
│  K8s Cluster                                                    │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │  Namespace: nuclaw                                       │   │
│  │                                                          │   │
│  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐     │   │
│  │  │ Chat Svc    │  │ AI Svc      │  │ Knowledge   │     │   │
│  │  │ Replica: 3  │  │ Replica: 2  │  │ Replica: 1  │     │   │
│  │  │ HPA: 3-10   │  │ HPA: 2-20   │  │             │     │   │
│  │  └─────────────┘  └─────────────┘  └─────────────┘     │   │
│  │                                                          │   │
│  │  ┌──────────────────────────────────────────────────┐   │   │
│  │  │  Ingress Controller (Nginx)                      │   │   │
│  │  │  - SSL 终止                                       │   │   │
│  │  │  - 负载均衡                                       │   │   │
│  │  │  - 路由分发                                       │   │   │
│  │  └──────────────────────────────────────────────────┘   │   │
│  │                                                          │   │
│  │  ┌──────────────┐  ┌──────────────┐  ┌─────────────┐   │   │
│  │  │ PostgreSQL   │  │ Redis        │  │ Prometheus  │   │   │
│  │  │ StatefulSet  │  │ StatefulSet  │  │ + Grafana   │   │   │
│  │  └──────────────┘  └──────────────┘  └─────────────┘   │   │
│  │                                                          │   │
│  └─────────────────────────────────────────────────────────┘   │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 容器化

#### Dockerfile（多阶段构建）

```dockerfile
# 阶段 1：构建
FROM ubuntu:22.04 AS builder
RUN apt-get update && apt-get install -y build-essential cmake libboost-all-dev

WORKDIR /build
COPY . .
RUN mkdir -p build && cd build \
    && cmake .. -DCMAKE_BUILD_TYPE=Release \
    && make -j$(nproc)

# 阶段 2：运行（体积更小）
FROM ubuntu:22.04 AS runtime
RUN apt-get update && apt-get install -y \
    libboost-system1.74.0 libboost-thread1.74.0 libboost-json1.74.0

# 非 root 用户运行
RUN useradd -m -s /bin/bash nuclaw
WORKDIR /app
COPY --from=builder /build/build/step20_demo /app/
RUN chown -R nuclaw:nuclaw /app
USER nuclaw

EXPOSE 8080
CMD ["./step20_demo"]
```

**构建：**

```bash
# 构建镜像
docker build -t nuclaw/step20:latest .

# 运行容器
docker run -d \
  --name nuclaw-chat \
  -p 8080:8080 \
  -e SERVICE_NAME=chat \
  -e SERVICE_PORT=8080 \
  nuclaw/step20:latest
```

### Docker Compose

```yaml
version: '3.8'

services:
  chat-service:
    build: .
    ports:
      - "8081:8080"
    environment:
      - SERVICE_NAME=chat
    depends_on:
      - redis
      - postgres

  ai-service:
    build: .
    ports:
      - "8082:8080"
    environment:
      - SERVICE_NAME=ai
      - OPENAI_API_KEY=${OPENAI_API_KEY}

  postgres:
    image: postgres:15-alpine
    environment:
      - POSTGRES_PASSWORD=postgres
    volumes:
      - postgres-data:/var/lib/postgresql/data

  redis:
    image: redis:7-alpine
    volumes:
      - redis-data:/data

  nginx:
    image: nginx:alpine
    ports:
      - "80:80"
    volumes:
      - ./nginx.conf:/etc/nginx/nginx.conf:ro

volumes:
  postgres-data:
  redis-data:
```

**启动：**

```bash
# 一键启动所有服务
docker-compose up -d

# 查看状态
docker-compose ps

# 查看日志
docker-compose logs -f chat-service

# 停止
docker-compose down
```

### Kubernetes

#### 核心概念

| K8s 资源 | 作用 | 对应服务 |
|:---|:---|:---|
| **Namespace** | 资源隔离 | `nuclaw` |
| **Deployment** | 无状态应用部署 | Chat/AI/Knowledge Service |
| **StatefulSet** | 有状态应用部署 | PostgreSQL, Redis |
| **Service** | 服务发现和负载均衡 | 所有服务 |
| **Ingress** | HTTP 路由 | Nginx Ingress |
| **HPA** | 自动水平扩展 | Chat/AI Service |
| **ConfigMap** | 配置管理 | 应用配置 |
| **Secret** | 敏感信息 | API Key, 密码 |

#### 部署文件

```yaml
# 05-chat-service.yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: chat-service
  namespace: nuclaw
spec:
  replicas: 3  # 3 个副本
  selector:
    matchLabels:
      app: chat-service
  template:
    metadata:
      labels:
        app: chat-service
    spec:
      containers:
        - name: chat
          image: nuclaw/step20:latest
          ports:
            - containerPort: 8080
          resources:
            requests:
              memory: "256Mi"
              cpu: "250m"
            limits:
              memory: "512Mi"
              cpu: "500m"
---
# 自动扩缩容
apiVersion: autoscaling/v2
kind: HorizontalPodAutoscaler
metadata:
  name: chat-service-hpa
spec:
  scaleTargetRef:
    apiVersion: apps/v1
    kind: Deployment
    name: chat-service
  minReplicas: 3
  maxReplicas: 10
  metrics:
    - type: Resource
      resource:
        name: cpu
        target:
          averageUtilization: 70
```

**部署：**

```bash
# 创建命名空间
kubectl create namespace nuclaw

# 部署所有资源
kubectl apply -f k8s/

# 查看状态
kubectl get pods -n nuclaw
kubectl get svc -n nuclaw
kubectl get ingress -n nuclaw

# 扩容
kubectl scale deployment chat-service --replicas=5 -n nuclaw

# 查看日志
kubectl logs -f deployment/chat-service -n nuclaw
```

### 监控

```yaml
# Prometheus 抓取配置
scrape_configs:
  - job_name: 'nuclaw-services'
    static_configs:
      - targets: 
        - 'chat-service:8080'
        - 'ai-service:8080'
        - 'knowledge-service:8080'
```

**关键指标：**

| 指标 | 说明 | 告警阈值 |
|:---|:---|:---|
| `http_requests_total` | 总请求数 | - |
| `http_request_duration_seconds` | 请求延迟 | > 500ms |
| `cpu_usage_percent` | CPU 使用率 | > 70% |
| `memory_usage_percent` | 内存使用率 | > 80% |
| `active_sessions` | 活跃会话数 | - |
| `queue_depth` | 消息队列深度 | > 1000 |

---

## 完整源码

### 目录结构

```
src/step20/
├── Dockerfile                      # 多阶段构建
├── docker-compose.yml              # 本地编排
├── k8s/
│   ├── 01-namespace.yaml           # 命名空间
│   ├── 02-configmap.yaml           # 配置
│   ├── 03-postgres.yaml            # 数据库
│   ├── 04-redis.yaml               # 缓存
│   ├── 05-chat-service.yaml        # Chat Service
│   ├── 06-ai-service.yaml          # AI Service
│   ├── 07-knowledge-service.yaml   # Knowledge Service
│   ├── 08-human-service.yaml       # Human Service
│   ├── 09-tenant-service.yaml      # Tenant Service
│   ├── 10-billing-service.yaml     # Billing Service
│   └── 11-ingress.yaml             # 网关
├── nginx.conf                      # Nginx 配置
├── prometheus.yml                  # 监控配置
└── src/
    └── main.cpp                    # 服务入口
```

---

## 部署操作

### Docker Compose（开发/测试）

```bash
# 1. 构建镜像
cd src/step20
docker build -t nuclaw/step20:latest .

# 2. 启动服务
docker-compose up -d

# 3. 验证
curl http://localhost:8081/health
curl http://localhost:8082/health

# 4. 停止
docker-compose down
```

### Kubernetes（生产）

```bash
# 1. 创建集群（使用 kind 本地测试）
kind create cluster --name nuclaw

# 2. 加载镜像到集群
kind load docker-image nuclaw/step20:latest --name nuclaw

# 3. 部署
cd src/step20/k8s
kubectl apply -f .

# 4. 验证
kubectl get pods -n nuclaw
kubectl get svc -n nuclaw

# 5. 端口转发测试
kubectl port-forward svc/chat-service 8081:80 -n nuclaw

# 6. 压力测试
kubectl run load-test --image=williamyeh/wrk \
  -- -t4 -c100 -d30s http://chat-service.nuclaw.svc.cluster.local
```

---

## 运维命令速查

```bash
# ===== Docker =====
docker ps                              # 查看运行容器
docker logs -f <container>           # 查看日志
docker exec -it <container> bash     # 进入容器
docker stats                           # 资源使用

# ===== Kubectl =====
kubectl get pods -n nuclaw -w          # 实时监控 Pod
kubectl describe pod <pod> -n nuclaw  # Pod 详情
kubectl logs -f <pod> -n nuclaw       # 查看日志
kubectl top pod -n nuclaw              # 资源使用
kubectl scale deploy/chat-service --replicas=5 -n nuclaw  # 扩容
kubectl rollout restart deploy/chat-service -n nuclaw     # 滚动更新
kubectl delete -f k8s/                 # 删除所有资源
```

---

## 本章总结

- ✅ **完成了生产级部署**：
  - **Docker**：多阶段构建，减小镜像体积 80%
  - **Docker Compose**：本地一键启动 10+ 服务
  - **Kubernetes**：水平扩展、自动恢复、滚动更新
  - **监控**：Prometheus + Grafana 全方位监控

- ✅ **实现了高可用架构**：
  - 服务多副本部署
  - 数据库持久化存储
  - 自动水平扩缩容
  - 健康检查和自动重启

- ✅ **NuClaw 教程全部完成**：
  - Step 1-14：基础技能（工具调用、LLM、记忆等）
  - Step 15-16：产品化（IM 接入、状态管理）
  - Step 17-20：实战项目（智能客服 SaaS 平台）

---

## 课后思考

你已经完成了一个完整的 AI Agent SaaS 平台！

**下一步可以做什么？**

<details>
<summary>点击查看扩展方向 💡</summary>

**1. 多语言支持**
- 添加国际化（i18n）支持
- 支持中文、英文、日文等多语言 Agent

**2. APM 链路追踪**
- 接入 Jaeger 或 Zipkin
- 追踪跨服务的请求链路

**3. CI/CD 流水线**
- GitHub Actions 自动构建
- ArgoCD GitOps 部署

**4. 安全加固**
- OAuth2/JWT 认证
- API 限流和防护
- 审计日志

**5. 数据分析**
- 用户行为分析
- 对话质量评估
- 业务报表

**6. 模型优化**
- 接入本地 LLM（Llama、ChatGLM）
- Fine-tuning 领域模型
- 模型量化加速

---

**恭喜！你已经掌握了构建 AI Agent 系统的完整技能栈！** 🎉

</details>
