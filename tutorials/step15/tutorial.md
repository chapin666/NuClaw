# Step 15: 部署运维 —— 生产就绪

> 目标：实现 Docker 容器化、CI/CD 流程，支持生产部署
> 
003e 难度：⭐⭐⭐ | 代码量：约 1000 行 | 预计学习时间：3-4 小时

---

## 一、为什么要容器化？

### 1.1 传统部署的问题

```
传统部署方式：

开发环境（MacBook）
    代码编译通过 ✅
    功能测试通过 ✅
    打包发布
         │
         ▼
测试环境（Ubuntu 20.04）
    编译失败 ❌（依赖版本不同）
    修依赖问题
         │
         ▼
生产环境（CentOS 7）
    运行报错 ❌（glibc 版本不兼容）
    配置文件缺失 ❌
    端口冲突 ❌

问题总结：
1. 环境差异导致"在我机器上能跑"
2. 依赖管理复杂，版本冲突难解决
3. 配置散落在各处，难以管理
4. 回滚困难，出现问题难以恢复
```

### 1.2 容器化的价值

```
容器化部署：

┌─────────────────────────────────────────────────────────────┐
│                    容器镜像（不可变基础设施）                  │
│  ┌───────────────────────────────────────────────────────┐  │
│  │  应用代码                                              │  │
│  │  ┌─────────────────────────────────────────────────┐  │  │
│  │  │  运行时环境（glibc, libssl, etc）                │  │  │
│  │  │  ┌───────────────────────────────────────────┐  │  │  │
│  │  │  │  基础系统（Ubuntu/Debian/Alpine）          │  │  │  │
│  │  │  └───────────────────────────────────────────┘  │  │  │
│  │  └─────────────────────────────────────────────────┘  │  │
│  └───────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
         │
         │ 镜像可以在任何环境运行（开发/测试/生产）
         │
    ┌────┴────┬─────────┬─────────┐
    ▼         ▼         ▼         ▼
 MacBook   测试服务器  生产集群   云服务器

核心价值：
• 环境一致性：一次构建，到处运行
• 依赖隔离：应用之间互不影响
• 快速部署：秒级启动
• 弹性伸缩：快速扩缩容
• 版本管理：镜像即版本，回滚简单
```

---

## 二、Docker 核心概念

### 2.1 镜像 vs 容器

```
┌─────────────────────────────────────────────────────────────┐
│                    Docker 核心概念                           │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  镜像（Image） ────────▶ 类（Class）                          │
│  • 只读模板                                                  │
│  • 包含应用 + 依赖 + 系统                                    │
│  • 分层存储（Layers）                                        │
│                                                             │
│  容器（Container） ────▶ 对象（Object）                       │
│  • 镜像的运行实例                                            │
│  • 可写层（Writable Layer）                                  │
│  • 隔离的进程空间                                            │
│                                                             │
│  分层存储示意图：                                            │
│  ┌─────────────────────────────────────────┐                 │
│  │  可写层（Container Layer）               │  运行时修改      │
│  ├─────────────────────────────────────────┤                 │
│  │  应用层（Application）                   │  编译产物        │
│  ├─────────────────────────────────────────┤                 │
│  │  依赖层（Boost, OpenSSL）                │  第三方库        │
│  ├─────────────────────────────────────────┤                 │
│  │  基础层（Ubuntu 22.04）                  │  操作系统        │
│  └─────────────────────────────────────────┘                 │
│                                                             │
│  分层优势：                                                  │
│  • 共享基础层，节省存储                                       │
│  • 缓存未改变的层，加速构建                                   │
│  • 只传输差异，加速分发                                       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 Dockerfile 指令详解

```dockerfile
# ─────────────────────────────────────────────────────────────
# 1. 基础指令
# ─────────────────────────────────────────────────────────────

# FROM：指定基础镜像
FROM ubuntu:22.04 AS builder
#     │            │    │
#     │            │    └── 阶段命名（多阶段构建）
#     │            └─────── 镜像标签
#     └──────────────────── 镜像名称

# LABEL：添加元数据
LABEL maintainer="your@email.com"
LABEL version="1.0.0"

# ─────────────────────────────────────────────────────────────
# 2. 构建指令
# ─────────────────────────────────────────────────────────────

# RUN：执行命令（创建新层）
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    && rm -rf /var/lib/apt/lists/*
# 技巧：合并命令减少层数，清理缓存减小镜像

# COPY：复制本地文件到镜像
COPY . /build
#  │  │
#  │  └── 镜像内目标路径
#  └───── 本地源路径

# WORKDIR：设置工作目录
WORKDIR /build
# 相当于 cd /build，后续指令在此目录执行

# ─────────────────────────────────────────────────────────────
# 3. 运行配置
# ─────────────────────────────────────────────────────────────

# ENV：设置环境变量
ENV LOG_LEVEL=info
ENV CONFIG_PATH=/app/config

# EXPOSE：声明端口（仅文档作用）
EXPOSE 8080 9090

# VOLUME：挂载点（数据持久化）
VOLUME ["/app/data", "/app/logs"]

# ─────────────────────────────────────────────────────────────
# 4. 启动配置
# ─────────────────────────────────────────────────────────────

# USER：指定运行用户（安全）
USER nuclaw
# 避免以 root 运行，限制权限

# HEALTHCHECK：健康检查
HEALTHCHECK --interval=30s --timeout=3s --retries=3 \
    CMD curl -f http://localhost:8080/health || exit 1

# ENTRYPOINT + CMD：启动命令
ENTRYPOINT ["./nuclaw"]
CMD ["--config", "./config/production.yaml"]
# │                              │
# │                              └── 默认参数（可被覆盖）
# └────────────────────────────── 固定执行的命令
```

### 2.3 多阶段构建

**为什么需要多阶段构建？**

```
单阶段构建的问题：

编译阶段需要的          运行阶段需要的
• 编译器 (g++)          • 运行时库 (.so)
• 头文件                • 可执行文件
• 构建工具 (cmake)
• 源码

单阶段镜像 = 编译产物 + 编译工具 + 源码 = 2GB+

多阶段构建：

┌──────────────┐         ┌──────────────┐
│  Build Stage │         │  Run Stage   │
│  • 编译器     │         │  • 运行时库   │
│  • 头文件     │ ──────▶ │  • 可执行文件 │
│  • 源码       │  复制   │  • 配置       │
└──────────────┘ 产物    └──────────────┘
      │                      │
      └────── 丢弃 ──────────┘

结果：镜像从 2GB 降到 200MB
```

---

## 三、NuClaw Dockerfile 详解

```dockerfile
# ═══════════════════════════════════════════════════════════
# 阶段 1：构建环境
# ═══════════════════════════════════════════════════════════
FROM ubuntu:22.04 AS builder

# 使用阿里云镜像加速（国内环境）
RUN sed -i 's/archive.ubuntu.com/mirrors.aliyun.com/g' /etc/apt/sources.list

# 安装编译依赖
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    libboost-all-dev \
    libssl-dev \
    libsqlite3-dev \
    && rm -rf /var/lib/apt/lists/*
    # ↑ 清理 apt 缓存，减小层大小

# 设置构建目录
WORKDIR /build

# 复制源码
COPY CMakeLists.txt .
COPY src ./src
COPY include ./include
COPY tests ./tests

# 编译
RUN mkdir -p build && cd build \
    && cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_FLAGS="-O3 -march=native" \
    && make -j$(nproc)
    # ↑ -j$(nproc) 使用所有 CPU 核心并行编译

# 运行测试（可选，确保构建正确）
RUN cd build && ctest --output-on-failure

# ═══════════════════════════════════════════════════════════
# 阶段 2：运行环境
# ═══════════════════════════════════════════════════════════
FROM ubuntu:22.04

# 只安装运行时依赖（不需要编译器）
RUN apt-get update && apt-get install -y \
    libboost-system1.74.0 \
    libboost-thread1.74.0 \
    libssl3 \
    libsqlite3-0 \
    curl \
    && rm -rf /var/lib/apt/lists/*

# 创建非 root 用户（安全最佳实践）
RUN useradd -m -u 1000 -s /bin/bash nuclaw
#     │      │   │            │
#     │      │   │            └── 默认 shell
#     │      │   └── 用户 ID（固定便于权限管理）
#     │      └── 创建 home 目录
#     └── 用户名

# 创建应用目录
WORKDIR /app

# 从构建阶段复制产物
COPY --from=builder /build/build/nuclaw ./
# ↑ --from=builder 指定源阶段

# 复制配置
COPY --from=builder /build/config ./config

# 创建数据目录并设置权限
RUN mkdir -p /app/data /app/logs \
    && chown -R nuclaw:nuclaw /app

# 切换到非 root 用户
USER nuclaw

# 暴露端口
EXPOSE 8080 9090

# 健康检查
HEALTHCHECK --interval=30s \
            --timeout=3s \
            --start-period=5s \
            --retries=3 \
    CMD curl -f http://localhost:8080/health || exit 1
#     │                              │
#     │                              └── 失败退出码
#     └── 健康检查端点

# 启动命令
ENTRYPOINT ["./nuclaw"]
CMD ["--config", "./config/production.yaml"]
```

---

## 四、Docker Compose 编排

### 4.1 多服务架构

```yaml
# docker-compose.yml
version: '3.8'

services:
  # ───────────────────────────────────────────────────────
  # NuClaw Agent 服务
  # ───────────────────────────────────────────────────────
  nuclaw:
    build:
      context: .
      dockerfile: Dockerfile
      # 构建参数
      args:
        - BUILD_TYPE=Release
    image: nuclaw:latest
    container_name: nuclaw-agent
    
    # 端口映射
    ports:
      - "8080:8080"    # HTTP 接口
      - "9090:9090"    # Metrics 接口
    
    # 环境变量
    environment:
      - OPENAI_API_KEY=${OPENAI_API_KEY}
      - LOG_LEVEL=info
    
    # 数据持久化
    volumes:
      - ./data:/app/data
      - ./logs:/app/logs
      - ./config:/app/config:ro  # 只读挂载
    
    # 资源限制
    deploy:
      resources:
        limits:
          cpus: '2'
          memory: 2G
        reservations:
          cpus: '0.5'
          memory: 512M
    
    # 重启策略
    restart: unless-stopped
    
    # 健康检查
    healthcheck:
      test: ["CMD", "curl", "-f", "http://localhost:8080/health"]
      interval: 30s
      timeout: 3s
      retries: 3
    
    # 网络
    networks:
      - nuclaw-network
    
    # 依赖（启动顺序）
    depends_on:
      - redis
      - vector-db

  # ───────────────────────────────────────────────────────
  # Redis（会话存储）
  # ───────────────────────────────────────────────────────
  redis:
    image: redis:7-alpine
    container_name: nuclaw-redis
    volumes:
      - redis-data:/data
    networks:
      - nuclaw-network
    restart: unless-stopped

  # ───────────────────────────────────────────────────────
  # Chroma（向量数据库）
  # ───────────────────────────────────────────────────────
  vector-db:
    image: chromadb/chroma:latest
    container_name: nuclaw-chroma
    volumes:
      - chroma-data:/chroma/chroma
    networks:
      - nuclaw-network
    restart: unless-stopped

  # ───────────────────────────────────────────────────────
  # Prometheus（监控）
  # ───────────────────────────────────────────────────────
  prometheus:
    image: prom/prometheus:latest
    container_name: nuclaw-prometheus
    ports:
      - "9091:9090"
    volumes:
      - ./monitoring/prometheus.yml:/etc/prometheus/prometheus.yml:ro
      - prometheus-data:/prometheus
    command:
      - '--config.file=/etc/prometheus/prometheus.yml'
      - '--storage.tsdb.path=/prometheus'
    networks:
      - nuclaw-network
    restart: unless-stopped

  # ───────────────────────────────────────────────────────
  # Grafana（可视化）
  # ───────────────────────────────────────────────────────
  grafana:
    image: grafana/grafana:latest
    container_name: nuclaw-grafana
    ports:
      - "3000:3000"
    volumes:
      - grafana-data:/var/lib/grafana
      - ./monitoring/grafana/dashboards:/etc/grafana/provisioning/dashboards:ro
    environment:
      - GF_SECURITY_ADMIN_PASSWORD=${GRAFANA_PASSWORD}
    networks:
      - nuclaw-network
    restart: unless-stopped

# 数据卷定义
volumes:
  redis-data:
  chroma-data:
  prometheus-data:
  grafana-data:

# 网络定义
networks:
  nuclaw-network:
    driver: bridge
```

---

## 五、CI/CD 流程

### 5.1 持续集成/持续部署的价值

```
传统发布流程：

开发 ──▶ 手动编译 ──▶ 手动测试 ──▶ 打包 ──▶ 上传服务器
                                          │
                              发现问题 ───┘
                                          │
                              回滚（手动，耗时，易错）

CI/CD 流程：

Push ──▶ 自动构建 ──▶ 自动测试 ──▶ 自动部署 ──▶ 自动验证
                                              │
                                  发现问题 ───┘
                                              │
                                  自动回滚（秒级，可靠）

CI/CD 收益：
• 发布频率提高 10x
• 故障恢复时间从小时降到分钟
• 人为错误减少 80%
• 开发者专注于编码
```

### 5.2 GitHub Actions 工作流

```yaml
# .github/workflows/ci-cd.yml
name: CI/CD Pipeline

on:
  push:
    branches: [main, develop]
    tags: ['v*']
  pull_request:
    branches: [main]

env:
  REGISTRY: ghcr.io
  IMAGE_NAME: ${{ github.repository }}

jobs:
  # ═══════════════════════════════════════════════════════
  # 阶段 1：构建与测试
  # ═══════════════════════════════════════════════════════
  build-and-test:
    runs-on: ubuntu-22.04
    
    steps:
      # 检出代码
      - name: Checkout
        uses: actions/checkout@v4
      
      # 缓存依赖（加速构建）
      - name: Cache dependencies
        uses: actions/cache@v3
        with:
          path: |
            ~/.ccache
            build/_deps
          key: ${{ runner.os }}-deps-${{ hashFiles('**/CMakeLists.txt') }}
      
      # 安装编译依赖
      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y \
            build-essential cmake ccache \
            libboost-all-dev libssl-dev libsqlite3-dev
      
      # 编译
      - name: Build
        run: |
          mkdir -p build
          cd build
          cmake .. \
            -DCMAKE_BUILD_TYPE=Release \
            -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
          make -j$(nproc)
      
      # 运行测试
      - name: Test
        run: |
          cd build
          ctest --output-on-failure --parallel $(nproc)
      
      # 代码质量检查（可选）
      - name: Lint
        run: |
          # 运行 clang-tidy, cppcheck 等
          echo "Running static analysis..."

  # ═══════════════════════════════════════════════════════
  # 阶段 2：构建镜像
  # ═══════════════════════════════════════════════════════
  build-image:
    needs: build-and-test
    runs-on: ubuntu-22.04
    permissions:
      contents: read
      packages: write
    
    steps:
      - name: Checkout
        uses: actions/checkout@v4
      
      # 登录容器仓库
      - name: Login to Registry
        uses: docker/login-action@v3
        with:
          registry: ${{ env.REGISTRY }}
          username: ${{ github.actor }}
          password: ${{ secrets.GITHUB_TOKEN }}
      
      # 提取元数据（标签）
      - name: Extract metadata
        id: meta
        uses: docker/metadata-action@v5
        with:
          images: ${{ env.REGISTRY }}/${{ env.IMAGE_NAME }}
          tags: |
            type=ref,event=branch
            type=semver,pattern={{version}}
            type=sha,prefix=,suffix=,format=short
      
      # 构建并推送镜像
      - name: Build and push
        uses: docker/build-push-action@v5
        with:
          context: .
          push: true
          tags: ${{ steps.meta.outputs.tags }}
          labels: ${{ steps.meta.outputs.labels }}
          cache-from: type=gha
          cache-to: type=gha,mode=max

  # ═══════════════════════════════════════════════════════
  # 阶段 3：部署（仅主分支和标签）
  # ═══════════════════════════════════════════════════════
  deploy:
    needs: build-image
    runs-on: ubuntu-22.04
    if: github.ref == 'refs/heads/main' || startsWith(github.ref, 'refs/tags/v')
    environment: production  # 需要人工审批
    
    steps:
      - name: Deploy to server
        run: |
          echo "Deploying image ${{ steps.meta.outputs.tags }}"
          # SSH 到服务器执行部署脚本
          # 或使用 Kubernetes API
          # 或调用云服务商 API
```

---

## 六、生产部署最佳实践

### 6.1 配置管理

```
环境配置分离：

config/
├── default.yaml      # 默认配置（所有环境共用）
├── development.yaml  # 开发环境覆盖
├── staging.yaml      # 测试环境覆盖
└── production.yaml   # 生产环境覆盖

配置加载优先级（从高到低）：
1. 环境变量（OPENAI_API_KEY）
2. 环境特定配置（production.yaml）
3. 默认配置（default.yaml）
```

### 6.2 密钥管理

```
❌ 错误做法：
• 密钥写在代码里
• 密钥提交到 Git
• 密钥明文存储在配置文件

✅ 正确做法：
• 使用 Docker Secrets 或 K8s Secrets
• 生产环境使用 Vault 等密钥管理系统
• 密钥注入环境变量，不落地磁盘

示例（Docker Secrets）：
docker secret create openai_api_key - < api_key.txt
docker service create \
  --secret openai_api_key \
  -e OPENAI_API_KEY_FILE=/run/secrets/openai_api_key \
  nuclaw:latest
```

### 6.3 日志管理

```yaml
# 日志配置
logging:
  driver: "json-file"
  options:
    max-size: "100m"
    max-file: "3"
    labels: "service,environment"
    tags: "{{.ImageName}}/{{.Name}}/{{.ID}}"

# 或使用集中式日志
driver: "fluentd"
options:
  fluentd-address: "localhost:24224"
  tag: "docker.nuclaw"
```

---

## 七、本章小结

**核心收获：**

1. **容器化**：
   - 镜像 vs 容器 vs 分层存储
   - Dockerfile 指令详解
   - 多阶段构建减小镜像体积

2. **服务编排**：
   - Docker Compose 管理多服务
   - 服务依赖、网络、数据卷
   - 资源限制和健康检查

3. **CI/CD**：
   - GitHub Actions 自动化流程
   - 构建缓存加速
   - 自动测试和部署

4. **生产实践**：
   - 环境配置分离
   - 密钥安全管理
   - 日志聚合

---

## 八、引出的问题

### 8.1 性能瓶颈问题

容器化部署后，系统运行稳定，但性能可能遇到瓶颈：

```
问题场景：
• 相同请求反复计算 LLM，成本高
• HTTP 连接频繁建立，延迟大
• 缓存命中率低，数据库压力大

需要：系统性的性能优化
```

**下一章预告（Step 15）：**

我们将实现**性能优化**：
- 缓存策略（LRU、多级缓存）
- 连接池复用
- 性能监控和调优

部署已经完成，接下来要让系统跑得更快、更省资源。
