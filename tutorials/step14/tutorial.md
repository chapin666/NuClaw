# Step 14: 部署运维 - Docker, K8s, CI/CD

> 目标：掌握容器化部署和自动化运维
> 
> 难度：⭐⭐⭐⭐ (较难)
> 
> 代码量：配置文件为主
> 
> 预计学习时间：3-4 小时

---

## 📚 前置知识

### 为什么需要容器化？

**传统部署的问题：**

```
开发环境："在我机器上能跑"
测试环境：缺少依赖库
生产环境：配置文件不同
```

**容器化解决方案：**
```
Docker：一次构建，到处运行
- 打包应用 + 依赖 + 配置
- 环境一致性保证
```

### 容器化核心概念

**镜像（Image）：**
```
只读模板，包含：
- 应用程序
- 运行时环境
- 系统工具
- 配置文件
```

**容器（Container）：**
```
镜像的运行实例，特点：
- 轻量级（共享宿主机内核）
- 隔离性（进程、网络、文件系统）
- 可移植（任何支持 Docker 的平台）
```

**编排（Orchestration）：**
```
管理多个容器的：
- 部署
- 伸缩
- 负载均衡
- 故障恢复
```

---

## 第一步：Docker 容器化

### Dockerfile 编写

```dockerfile
# Dockerfile - NuClaw Step 14
# 多阶段构建，减小镜像体积

# 阶段1：构建
FROM ubuntu:22.04 AS builder

# 安装依赖
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    libboost-all-dev \
    && rm -rf /var/lib/apt/lists/*

# 复制源码
WORKDIR /build
COPY . .

# 编译
RUN mkdir build && cd build \
    && cmake .. \
    && make -j$(nproc)

# 阶段2：运行
FROM ubuntu:22.04

# 安装运行时依赖（只安装必要的）
RUN apt-get update && apt-get install -y \
    libboost-system1.74.0 \
    libboost-json1.74.0 \
    && rm -rf /var/lib/apt/lists/*

# 创建应用目录
WORKDIR /app

# 从构建阶段复制二进制文件
COPY --from=builder /build/build/nuclaw_step09 /app/

# 复制配置文件
COPY config.yaml /app/

# 暴露端口
EXPOSE 8080

# 健康检查
HEALTHCHECK --interval=30s --timeout=3s --start-period=5s --retries=3 \
    CMD curl -f http://localhost:8080/health || exit 1

# 运行命令
ENTRYPOINT ["./nuclaw_step09"]
CMD ["config.yaml"]
```

### 构建和运行

```bash
# 构建镜像
docker build -t nuclaw:step14 .

# 运行容器
docker run -d \
    --name nuclaw \
    -p 8080:8080 \
    -e NUCLAW_LLM_API_KEY=sk-xxx \
    -v /host/config:/app/config \
    nuclaw:step14

# 查看日志
docker logs -f nuclaw

# 停止容器
docker stop nuclaw
```

### 多阶段构建优势

```
单阶段构建：
  镜像大小：500MB+（包含编译工具）

多阶段构建：
  构建阶段：500MB（有 gcc, cmake 等）
  运行阶段：50MB（只有运行时库）
  
优势：
  - 镜像体积小
  - 启动速度快
  - 攻击面小（无编译工具）
```

---

## 第二步：Docker Compose 本地编排

### docker-compose.yml

```yaml
version: '3.8'

services:
  nuclaw:
    build: .
    container_name: nuclaw-app
    ports:
      - "8080:8080"
    environment:
      - NUCLAW_LLM_API_KEY=${OPENAI_API_KEY}
      - NUCLAW_LOG_LEVEL=info
    volumes:
      - ./config:/app/config
      - ./logs:/app/logs
    restart: unless-stopped
    healthcheck:
      test: ["CMD", "curl", "-f", "http://localhost:8080/health"]
      interval: 30s
      timeout: 10s
      retries: 3
    networks:
      - nuclaw-network

  # 可选：配合 Redis 做缓存
  redis:
    image: redis:7-alpine
    container_name: nuclaw-redis
    ports:
      - "6379:6379"
    volumes:
      - redis-data:/data
    networks:
      - nuclaw-network

  # 可选：配合 Nginx 做反向代理
  nginx:
    image: nginx:alpine
    container_name: nuclaw-nginx
    ports:
      - "80:80"
      - "443:443"
    volumes:
      - ./nginx.conf:/etc/nginx/nginx.conf
      - ./ssl:/etc/nginx/ssl
    depends_on:
      - nuclaw
    networks:
      - nuclaw-network

volumes:
  redis-data:

networks:
  nuclaw-network:
    driver: bridge
```

### 使用 Docker Compose

```bash
# 启动所有服务
docker-compose up -d

# 查看服务状态
docker-compose ps

# 查看日志
docker-compose logs -f nuclaw

# 停止服务
docker-compose down

# 重启单个服务
docker-compose restart nuclaw
```

---

## 第三步：Kubernetes 部署

### K8s 核心概念

```
Pod：最小部署单元（一个或多个容器）
Deployment：管理 Pod 的副本和更新
Service：提供稳定的网络访问
ConfigMap：存储配置数据
Secret：存储敏感信息
Ingress：HTTP/HTTPS 路由
```

### 部署配置

**deployment.yaml：**
```yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: nuclaw
  labels:
    app: nuclaw
spec:
  replicas: 3  # 运行3个副本
  selector:
    matchLabels:
      app: nuclaw
  template:
    metadata:
      labels:
        app: nuclaw
    spec:
      containers:
      - name: nuclaw
        image: nuclaw:step14
        ports:
        - containerPort: 8080
        env:
        - name: NUCLAW_LLM_API_KEY
          valueFrom:
            secretKeyRef:
              name: nuclaw-secrets
              key: openai-api-key
        resources:
          requests:
            memory: "128Mi"
            cpu: "100m"
          limits:
            memory: "512Mi"
            cpu: "500m"
        livenessProbe:
          httpGet:
            path: /health
            port: 8080
          initialDelaySeconds: 10
          periodSeconds: 30
        readinessProbe:
          httpGet:
            path: /ready
            port: 8080
          initialDelaySeconds: 5
          periodSeconds: 10
```

**service.yaml：**
```yaml
apiVersion: v1
kind: Service
metadata:
  name: nuclaw
spec:
  selector:
    app: nuclaw
  ports:
    - protocol: TCP
      port: 80
      targetPort: 8080
  type: ClusterIP
```

**secret.yaml（敏感信息）：**
```yaml
apiVersion: v1
kind: Secret
metadata:
  name: nuclaw-secrets
type: Opaque
stringData:
  openai-api-key: sk-your-key-here
```

**ingress.yaml（路由）：**
```yaml
apiVersion: networking.k8s.io/v1
kind: Ingress
metadata:
  name: nuclaw-ingress
  annotations:
    nginx.ingress.kubernetes.io/rewrite-target: /
spec:
  rules:
  - host: nuclaw.example.com
    http:
      paths:
      - path: /
        pathType: Prefix
        backend:
          service:
            name: nuclaw
            port:
              number: 80
  tls:
  - hosts:
    - nuclaw.example.com
    secretName: nuclaw-tls
```

### K8s 操作命令

```bash
# 应用配置
kubectl apply -f secret.yaml
kubectl apply -f deployment.yaml
kubectl apply -f service.yaml
kubectl apply -f ingress.yaml

# 查看状态
kubectl get pods
kubectl get svc
kubectl get ingress

# 查看日志
kubectl logs -f deployment/nuclaw

# 扩容
kubectl scale deployment nuclaw --replicas=5

# 更新镜像
kubectl set image deployment/nuclaw nuclaw=nuclaw:v2.0

# 进入容器调试
kubectl exec -it pod/nuclaw-xxx -- /bin/bash
```

---

## 第四步：CI/CD 自动化

### GitHub Actions 工作流

**.github/workflows/ci.yml：**
```yaml
name: CI/CD

on:
  push:
    branches: [main, master]
  pull_request:
    branches: [main, master]

jobs:
  # 阶段1：构建和测试
  build:
    runs-on: ubuntu-22.04
    
    steps:
    - name: Checkout code
      uses: actions/checkout@v3
      
    - name: Install dependencies
      run: |
        sudo apt-get update
        sudo apt-get install -y cmake libboost-all-dev
        
    - name: Build
      run: |
        mkdir build
        cd build
        cmake ..
        make -j$(nproc)
        
    - name: Test
      run: |
        cd build
        ctest --output-on-failure

  # 阶段2：构建 Docker 镜像
  docker:
    needs: build
    runs-on: ubuntu-22.04
    
    steps:
    - name: Checkout code
      uses: actions/checkout@v3
      
    - name: Set up Docker Buildx
      uses: docker/setup-buildx-action@v2
      
    - name: Login to Docker Hub
      uses: docker/login-action@v2
      with:
        username: ${{ secrets.DOCKER_USERNAME }}
        password: ${{ secrets.DOCKER_PASSWORD }}
        
    - name: Build and push
      uses: docker/build-push-action@v4
      with:
        context: .
        push: true
        tags: |
          ${{ secrets.DOCKER_USERNAME }}/nuclaw:${{ github.sha }}
          ${{ secrets.DOCKER_USERNAME }}/nuclaw:latest

  # 阶段3：部署到 K8s
  deploy:
    needs: docker
    runs-on: ubuntu-22.04
    if: github.ref == 'refs/heads/main'
    
    steps:
    - name: Checkout code
      uses: actions/checkout@v3
      
    - name: Setup kubectl
      uses: azure/setup-kubectl@v3
      
    - name: Configure kubectl
      run: |
        echo "${{ secrets.KUBE_CONFIG }}" | base64 -d > kubeconfig
        export KUBECONFIG=kubeconfig
        
    - name: Update image
      run: |
        export KUBECONFIG=kubeconfig
        kubectl set image deployment/nuclaw \
          nuclaw=${{ secrets.DOCKER_USERNAME }}/nuclaw:${{ github.sha }}
        kubectl rollout status deployment/nuclaw
```

### CI/CD 流程图

```
开发者提交代码
      ↓
GitHub Actions 触发
      ↓
┌─────────────────┐
│   1. 编译测试    │
│   - 构建项目     │
│   - 运行单元测试 │
└────────┬────────┘
         ↓
┌─────────────────┐
│  2. 构建镜像     │
│   - Docker 构建  │
│   - 推送到仓库   │
└────────┬────────┘
         ↓
┌─────────────────┐
│  3. 部署上线     │
│   - K8s 更新    │
│   - 健康检查     │
└─────────────────┘
         ↓
    部署完成！
```

---

## 第五步：监控与日志收集

### Prometheus + Grafana 监控

**prometheus-config.yaml：**
```yaml
apiVersion: v1
kind: ConfigMap
metadata:
  name: prometheus-config
data:
  prometheus.yml: |
    global:
      scrape_interval: 15s
    
    scrape_configs:
    - job_name: 'nuclaw'
      static_configs:
      - targets: ['nuclaw:8080']
      metrics_path: /metrics
```

### ELK 日志收集

**filebeat-config.yaml：**
```yaml
filebeat.inputs:
- type: log
  enabled: true
  paths:
    - /app/logs/*.log

output.elasticsearch:
  hosts: ["elasticsearch:9200"]

logging.level: info
```

---

## 本节总结

### 核心概念

1. **Docker**：应用容器化，环境一致性
2. **Docker Compose**：本地多服务编排
3. **Kubernetes**：生产级容器编排
4. **CI/CD**：自动化构建、测试、部署

### 部署流程

```
开发 → Dockerfile → 镜像构建 → 镜像推送 → K8s 部署 → 服务上线
            ↓              ↓            ↓
        本地测试       Docker Hub    滚动更新
```

### 最佳实践

1. **镜像优化**：多阶段构建，减小体积
2. **健康检查**：配置 liveness/readiness probe
3. **资源限制**：设置 CPU/内存 limits
4. **配置分离**：使用 ConfigMap 和 Secret
5. **日志收集**：统一输出到 stdout，外部收集
6. **监控告警**：Prometheus + Grafana

---

## 📝 课后练习

### 练习 1：Helm Chart
将 K8s 配置打包成 Helm Chart，支持参数化部署。

### 练习 2：灰度发布
实现金丝雀发布（Canary Deployment），逐步切换流量。

### 练习 3：自动伸缩
配置 HPA（Horizontal Pod Autoscaler），根据 CPU 自动扩缩容。

### 思考题
1. 什么情况下应该用 Docker Compose，什么情况下用 K8s？
2. 如何保证容器化应用的无状态性？
3. CI/CD 中如何做好回滚策略？

---

**恭喜！** 你完成了 NuClaw 的全部 15 章教程！从 89 行的 Echo 服务器到完整的生产级 AI Agent，你已经掌握了构建现代 Agent 系统的核心技术。
