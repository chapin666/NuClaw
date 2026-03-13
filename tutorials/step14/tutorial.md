# Step 14: 部署运维 - Docker, K8s, CI/CD

> 目标：掌握容器化部署和自动化运维
> 
> 难度：⭐⭐⭐⭐ (较难)
> 
> 代码量：配置文件为主
> 
> 预计学习时间：3-4 小时

---

## 🏗️ 工程化目录结构

**最终生产级项目结构：**

```
src/step14/
├── CMakeLists.txt          # 构建配置
├── Dockerfile              # ★ 新增：Docker 构建文件
├── docker-compose.yml      # ★ 新增：Docker Compose 编排
├── configs/
│   └── config.yaml         # 配置文件
├── include/nuclaw/         # 头文件目录（全部继承）
│   ├── metrics.hpp
│   ├── logger.hpp
│   ├── config.hpp
│   ├── agent.hpp
│   └── ...                 # 所有功能模块
├── src/
│   └── main.cpp            # 程序入口
├── tests/                  # 测试目录
└── k8s/                    # ★ 新增：Kubernetes 配置
    ├── deployment.yaml
    ├── service.yaml
    └── ingress.yaml
```

**Step 14 新增文件说明：**
- `Dockerfile` - 多阶段构建，生成最小镜像
- `docker-compose.yml` - 本地多服务编排
- `k8s/*.yaml` - Kubernetes 生产部署配置

**部署演进：**
```bash
# Step 13 及之前：本地运行
./nuclaw

# Step 14：容器化运行
docker build -t nuclaw .
docker run -p 8080:8080 nuclaw

# Step 14：Kubernetes 部署
kubectl apply -f k8s/
```

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

---

## 第一步：Docker 容器化

### Dockerfile

```dockerfile
# 阶段1：构建
FROM ubuntu:22.04 AS builder

RUN apt-get update && apt-get install -y \
    build-essential cmake libboost-all-dev

WORKDIR /build
COPY . .
RUN mkdir build && cd build && cmake .. && make

# 阶段2：运行
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

### Docker Compose

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
```

---

## 第二步：Kubernetes 部署

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
```

---

## 第三步：CI/CD

```yaml
# .github/workflows/ci.yml
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
      - name: Build and push
        run: |
          docker build -t nuclaw:${{ github.sha }} .
          docker push nuclaw:${{ github.sha }}
```

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
