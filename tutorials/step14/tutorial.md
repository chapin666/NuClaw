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

### 💡 理论知识：Docker 多阶段构建

**为什么要用多阶段构建？**

```
单阶段构建的问题：
┌─────────────────────────────────────┐
│  编译依赖 (gcc, cmake, boost-dev)   │  ← 500MB
│  源代码                              │
│  构建产物                            │
│  运行时依赖                          │
└─────────────────────────────────────┘
           最终镜像：800MB

多阶段构建：
┌──────────────────┐    ┌──────────────────┐
│   Builder 阶段    │ →  │   Runtime 阶段   │
│  编译依赖 + 源码  │    │  只拷贝二进制    │
│  输出：可执行文件 │    │  基础镜像：alpine│
└──────────────────┘    └──────────────────┘
        丢弃               最终镜像：50MB
```

**镜像优化技巧：**

| 技巧 | 效果 | 原理 |
|:---|:---|:---|
| 多阶段构建 | 减少 90% 体积 | 不打包编译工具 |
| Alpine 基础镜像 | 减少 80% 体积 | 精简系统库 |
| 层缓存优化 | 加速构建 | 不经常变动的放上层 |
| .dockerignore | 减少上下文 | 排除不需要的文件 |

**容器健康检查：**
```dockerfile
HEALTHCHECK --interval=30s --timeout=3s --retries=3 \
    CMD curl -f http://localhost:8080/health || exit 1

# 原理：
# - interval=30s：每 30 秒检查一次
# - timeout=3s：超时 3 秒认为失败
# - retries=3：连续 3 次失败才标记为 unhealthy
# - 编排系统（K8s/Docker Swarm）会自动重启 unhealthy 容器
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

### 💡 理论知识：K8s 核心概念

**Pod vs Container：**
```
Container（容器）：
- 单一进程的运行环境
- 类比：一个 Docker 容器

Pod（Pod）：
- 一个或多个紧密关联的容器
- 共享网络和存储
- 类比：一个主机上的进程组

为什么需要 Pod？
- sidecar 模式：主容器 + 日志收集/监控容器
- 共享 localhost：容器间通过 127.0.0.1 通信
- 原子调度：同生共死
```

**Deployment vs Service：**
```
Deployment：
- 声明：我要运行 3 个副本
- 职责：Pod 的创建、更新、扩缩容
- 类比：管理员

Service：
- 声明：这些 Pod 提供一个服务
- 职责：负载均衡、服务发现
- 类比：前台接待

关系：
用户 → Service（稳定入口）→ 分发到 3 个 Pod（由 Deployment 管理）
```

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
