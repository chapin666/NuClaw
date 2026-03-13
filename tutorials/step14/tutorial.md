# Step 14: 部署运维 - Docker, K8s, CI/CD

> 目标：掌握容器化部署和自动化运维
> 
> 难度：⭐⭐⭐⭐ (较难)
> 
> 代码量：配置文件为主
> 
> 预计学习时间：3-4 小时

---

## 🎯 Agent 开发知识点

**本节核心问题：** 如何将 Agent 部署到生产环境？

**Agent 部署特点：**
```
有状态：对话历史、用户会话
配置多：API key、模型参数、业务规则
需持久化：日志、指标、知识库
```

---

## 📚 理论基础 + 配置代码

### 1. Dockerfile 多阶段构建

**理论：** 减小镜像体积，提高安全性

```dockerfile
# 阶段1：构建
FROM ubuntu:22.04 AS builder

RUN apt-get update && apt-get install -y \
    build-essential cmake libboost-all-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build
COPY . .

RUN mkdir build && cd build \
    && cmake .. \
    && make -j$(nproc)

# 阶段2：运行（只包含运行时依赖）
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    libboost-system1.74.0 \
    curl \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=builder /build/build/nuclaw /app/
COPY config.yaml /app/

EXPOSE 8080

HEALTHCHECK --interval=30s --timeout=3s \
    CMD curl -f http://localhost:8080/health || exit 1

ENTRYPOINT ["./nuclaw"]
CMD ["config.yaml"]
```

### 2. Docker Compose 本地部署

```yaml
version: '3.8'

services:
  nuclaw:
    build: .
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

  redis:
    image: redis:7-alpine
    volumes:
      - redis-data:/data

volumes:
  redis-data:
```

### 3. Kubernetes 生产部署

```yaml
# deployment.yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: nuclaw
spec:
  replicas: 3
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
        image: nuclaw:latest
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
```

---

## 📋 Agent 开发检查清单

- [ ] 配置是否外部化？
- [ ] 健康检查端点是否实现？
- [ ] 日志是否输出到 stdout？
- [ ] 镜像是否多阶段构建？
- [ ] 敏感信息是否用 Secret？

---

## 🎉 课程完成

恭喜！你已完成 NuClaw 全部 15 章课程！

**技能掌握：**
- C++ 网络编程
- Agent Loop 设计
- LLM 集成
- 工具系统
- RAG 检索
- 多 Agent 协作
- 配置管理
- 监控告警
- 容器化部署
