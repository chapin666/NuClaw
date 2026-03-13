# Step 14: 部署运维 - 让 Agent 上线运行

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

### 核心概念

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
- 轻量级
- 隔离性
- 可移植
```

---

## 第一步：Docker 容器化

### Dockerfile 多阶段构建

**优势：** 减小镜像体积，提高安全性

```dockerfile
# 阶段1：构建
FROM ubuntu:22.04 AS builder

RUN apt-get update && apt-get install -y \
    build-essential cmake libboost-all-dev

WORKDIR /build
COPY . .
RUN mkdir build && cd build && cmake .. && make

# 阶段2：运行（只包含运行时依赖）
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    libboost-system1.74.0 curl

WORKDIR /app
COPY --from=builder /build/build/nuclaw /app/
COPY config.yaml /app/

EXPOSE 8080

HEALTHCHECK --interval=30s \
    CMD curl -f http://localhost:8080/health || exit 1

ENTRYPOINT ["./nuclaw"]
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
    nuclaw:step14
```

---

## 第二步：Docker Compose

### 本地编排

```yaml
version: '3.8'

services:
  nuclaw:
    build: .
    ports:
      - "8080:8080"
    environment:
      - NUCLAW_LLM_API_KEY=${OPENAI_API_KEY}
    volumes:
      - ./config:/app/config
    restart: unless-stopped

  redis:
    image: redis:7-alpine
    volumes:
      - redis-data:/data

volumes:
  redis-data:
```

### 使用

```bash
# 启动所有服务
docker-compose up -d

# 查看日志
docker-compose logs -f nuclaw

# 停止服务
docker-compose down
```

---

## 第三步：Kubernetes

### K8s 核心概念

```
Pod：最小部署单元
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
spec:
  replicas: 3  # 运行3个副本
  selector:
    matchLabels:
      app: nuclaw
  template:
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
```

---

## 第四步：CI/CD

### 自动化流程

```
开发者提交代码
      ↓
GitHub Actions 触发
      ↓
  1. 编译测试
      ↓
  2. 构建镜像
      ↓
  3. 部署上线
      ↓
  部署完成！
```

### GitHub Actions 配置

```yaml
name: CI/CD

on:
  push:
    branches: [main]

jobs:
  build:
    runs-on: ubuntu-22.04
    steps:
      - uses: actions/checkout@v3
      - name: Build
        run: |
          mkdir build && cd build
          cmake .. && make
      
  docker:
    needs: build
    runs-on: ubuntu-22.04
    steps:
      - name: Build and push Docker image
        run: |
          docker build -t nuclaw:${{ github.sha }} .
          docker push nuclaw:${{ github.sha }}
```

---

## 本节总结

### 核心概念

1. **Docker**：应用容器化
2. **Docker Compose**：本地多服务编排
3. **Kubernetes**：生产级容器编排
4. **CI/CD**：自动化构建部署

### 部署流程

```
开发 → Dockerfile → 镜像构建 → 镜像推送 → K8s 部署 → 服务上线
```

---

## 📝 课后练习

### 练习：Helm Chart
将 K8s 配置打包成 Helm Chart。

### 思考题
1. 什么情况下用 Docker Compose，什么情况下用 K8s？
2. 如何保证容器化应用的无状态性？

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
