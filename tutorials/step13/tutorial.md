# Step 13: Docker 容器化部署

> 目标：使用 Docker 容器化应用，支持一键部署
> 
003e 难度：⭐⭐⭐ | 预计学习时间：2-3 小时

---

## 一、问题引入

### 1.1 部署痛点

```
开发环境：一切正常 ✓
            ↓
生产环境：编译失败、依赖缺失、配置错误 💥

常见问题：
- 服务器缺少 Boost 库
- GCC 版本不兼容
- 配置文件路径不对
- 环境变量未设置
```

### 1.2 Docker 优势

```
┌─────────────────────────────────────────────┐
│           Docker Container                  │
│                                             │
│  ┌─────────────────────────────────────┐    │
│  │         Application                 │    │
│  │           (NuClaw)                  │    │
│  ├─────────────────────────────────────┤    │
│  │           Dependencies              │    │
│  │  • Boost, OpenSSL, SQLite...        │    │
│  ├─────────────────────────────────────┤    │
│  │         Operating System            │    │
│  │         (Ubuntu/Alpine)             │    │
│  └─────────────────────────────────────┘    │
│                                             │
│  一次构建，到处运行                          │
└─────────────────────────────────────────────┘
```

---

## 二、Dockerfile 编写

### 2.1 多阶段构建 Dockerfile

```dockerfile
# Step 1: 构建阶段
FROM ubuntu:22.04 AS builder

# 安装依赖
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    libboost-all-dev \
    libssl-dev \
    libsqlite3-dev \
    git \
    && rm -rf /var/lib/apt/lists/*

# 设置工作目录
WORKDIR /build

# 复制源码
COPY . .

# 编译
RUN mkdir -p build && cd build \
    && cmake .. -DCMAKE_BUILD_TYPE=Release \
    && make -j$(nproc)

# Step 2: 运行阶段
FROM ubuntu:22.04

# 安装运行时依赖
RUN apt-get update && apt-get install -y \
    libboost-system1.74.0 \
    libssl3 \
    libsqlite3-0 \
    && rm -rf /var/lib/apt/lists/*

# 创建应用用户
RUN useradd -m -u 1000 nuclaw

# 设置工作目录
WORKDIR /app

# 从构建阶段复制可执行文件
COPY --from=builder /build/build/nuclaw .
COPY --from=builder /build/config ./config

# 创建数据目录
RUN mkdir -p /app/data /app/logs && chown -R nuclaw:nuclaw /app

# 切换用户
USER nuclaw

# 暴露端口
EXPOSE 8080 9090

# 健康检查
HEALTHCHECK --interval=30s --timeout=3s --start-period=5s --retries=3 \
    CMD curl -f http://localhost:8080/health || exit 1

# 启动命令
ENTRYPOINT ["./nuclaw"]
CMD ["--config", "./config/production.json"]
```

### 2.2 精简版 Dockerfile（Alpine）

```dockerfile
FROM alpine:3.18 AS builder

RUN apk add --no-cache \
    build-base \
    cmake \
    boost-dev \
    openssl-dev \
    sqlite-dev

WORKDIR /build
COPY . .
RUN mkdir build && cd build \
    && cmake .. -DCMAKE_BUILD_TYPE=Release \
    && make -j$(nproc)

FROM alpine:3.18

RUN apk add --no-cache \
    libstdc++ \
    boost-system \
    openssl \
    sqlite-libs

WORKDIR /app
COPY --from=builder /build/build/nuclaw .

EXPOSE 8080

ENTRYPOINT ["./nuclaw"]
```

---

## 三、Docker Compose 配置

### 3.1 docker-compose.yml

```yaml
version: '3.8'

services:
  nuclaw:
    build: .
    ports:
      - "8080:8080"  # 应用端口
      - "9090:9090"  # 监控端口
    environment:
      - OPENAI_API_KEY=${OPENAI_API_KEY}
      - LOG_LEVEL=info
    volumes:
      - ./data:/app/data
      - ./logs:/app/logs
      - ./config:/app/config:ro
    restart: unless-stopped
    healthcheck:
      test: ["CMD", "curl", "-f", "http://localhost:8080/health"]
      interval: 30s
      timeout: 10s
      retries: 3
      start_period: 40s

  # 可选：Prometheus 监控
  prometheus:
    image: prom/prometheus:latest
    ports:
      - "9091:9090"
    volumes:
      - ./monitoring/prometheus.yml:/etc/prometheus/prometheus.yml:ro
    command:
      - '--config.file=/etc/prometheus/prometheus.yml'
    depends_on:
      - nuclaw

  # 可选：Grafana 可视化
  grafana:
    image: grafana/grafana:latest
    ports:
      - "3000:3000"
    volumes:
      - ./monitoring/grafana/dashboards:/etc/grafana/provisioning/dashboards:ro
      - ./monitoring/grafana/datasources:/etc/grafana/provisioning/datasources:ro
    environment:
      - GF_SECURITY_ADMIN_PASSWORD=admin
    depends_on:
      - prometheus
```

### 3.2 环境变量配置

**.env 文件：**
```bash
# API Keys
OPENAI_API_KEY=sk-xxxxxxxxxxxx

# 服务器配置
SERVER_PORT=8080
METRICS_PORT=9090
LOG_LEVEL=info

# 数据库
DB_PATH=/app/data/nuclaw.db

# 性能
MAX_CONNECTIONS=1000
RATE_LIMIT=100
```

---

## 四、部署流程

### 4.1 本地构建运行

```bash
# 1. 构建镜像
docker build -t nuclaw:latest .

# 2. 运行容器
docker run -d \
  --name nuclaw \
  -p 8080:8080 \
  -p 9090:9090 \
  -e OPENAI_API_KEY=$OPENAI_API_KEY \
  -v $(pwd)/data:/app/data \
  nuclaw:latest

# 3. 查看日志
docker logs -f nuclaw

# 4. 停止容器
docker stop nuclaw
```

### 4.2 使用 Docker Compose

```bash
# 1. 启动所有服务
docker-compose up -d

# 2. 查看状态
docker-compose ps

# 3. 查看日志
docker-compose logs -f nuclaw

# 4. 停止服务
docker-compose down

# 5. 完全清理（包括数据卷）
docker-compose down -v
```

### 4.3 生产部署

```bash
# 使用 docker swarm 或 kubernetes
# 示例：docker stack deploy

docker stack deploy -c docker-compose.yml nuclaw

# 扩容
docker service scale nuclaw_nuclaw=3
```

---

## 五、本章总结

- ✅ Dockerfile 多阶段构建
- ✅ Docker Compose 编排
- ✅ 健康检查配置
- ✅ 生产部署流程

---

## 六、课后思考

手动构建部署容易出错，如何实现自动化？

<details>
<summary>点击查看下一章 💡</summary>

**Step 14: CI/CD 持续集成/部署**

我们将学习：
- GitHub Actions 工作流
- 自动化测试
- 自动构建镜像
- 自动部署

</details>
