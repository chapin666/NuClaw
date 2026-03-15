# Step 14: 部署运维 —— 生产就绪

> 目标：实现 Docker 容器化、CI/CD 流程，支持生产部署
> 
003e 难度：⭐⭐⭐ | 代码量：约 1000 行 | 预计学习时间：2-3 小时

---

## 一、Docker 容器化

### 1.1 Dockerfile

```dockerfile
# 构建阶段
FROM ubuntu:22.04 AS builder

RUN apt-get update && apt-get install -y \
    build-essential cmake \
    libboost-all-dev libssl-dev libsqlite3-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build
COPY . .
RUN mkdir build && cd build \
    && cmake .. -DCMAKE_BUILD_TYPE=Release \
    && make -j$(nproc)

# 运行阶段
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    libboost-system1.74.0 libssl3 libsqlite3-0 \
    && rm -rf /var/lib/apt/lists/*

RUN useradd -m -u 1000 nuclaw

WORKDIR /app
COPY --from=builder /build/build/nuclaw .
COPY --from=builder /build/config ./config

RUN mkdir -p /app/data /app/logs && chown -R nuclaw:nuclaw /app

USER nuclaw
EXPOSE 8080 9090

HEALTHCHECK --interval=30s --timeout=3s \
    CMD curl -f http://localhost:8080/health || exit 1

ENTRYPOINT ["./nuclaw"]
CMD ["--config", "./config/production.yaml"]
```

### 1.2 Docker Compose

```yaml
version: '3.8'

services:
  nuclaw:
    build: .
    ports:
      - "8080:8080"
      - "9090:9090"
    environment:
      - OPENAI_API_KEY=${OPENAI_API_KEY}
    volumes:
      - ./data:/app/data
      - ./logs:/app/logs
    restart: unless-stopped

  prometheus:
    image: prom/prometheus:latest
    ports:
      - "9091:9090"
    volumes:
      - ./monitoring/prometheus.yml:/etc/prometheus/prometheus.yml:ro

  grafana:
    image: grafana/grafana:latest
    ports:
      - "3000:3000"
```

---

## 二、CI/CD 流程

### 2.1 GitHub Actions

```yaml
# .github/workflows/ci.yml
name: CI

on: [push, pull_request]

jobs:
  build:
    runs-on: ubuntu-22.04
    steps:
      - uses: actions/checkout@v4
      
      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y build-essential cmake \
            libboost-all-dev libssl-dev libsqlite3-dev
      
      - name: Build
        run: |
          mkdir build && cd build
          cmake .. -DCMAKE_BUILD_TYPE=Release
          make -j$(nproc)
      
      - name: Test
        run: cd build && ctest --output-on-failure

  docker:
    needs: build
    runs-on: ubuntu-22.04
    if: github.ref == 'refs/heads/main'
    steps:
      - uses: actions/checkout@v4
      
      - name: Build and push
        uses: docker/build-push-action@v5
        with:
          push: true
          tags: ghcr.io/${{ github.repository }}:latest
```

---

## 三、本章小结

- **Docker**：多阶段构建，最小化镜像
- **CI/CD**：GitHub Actions 自动化
- **监控集成**：Prometheus + Grafana

---

至此，NuClaw AI Agent 教程全部完成！
