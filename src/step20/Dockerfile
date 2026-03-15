# ============================================================================
# Dockerfile - Step 20: 生产部署
# ============================================================================
# 多阶段构建，减小镜像体积

# 阶段 1：构建
FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

# 安装构建依赖
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    libboost-all-dev \
    && rm -rf /var/lib/apt/lists/*

# 复制源码
WORKDIR /build
COPY . .

# 构建
RUN mkdir -p build && cd build \
    && cmake .. -DCMAKE_BUILD_TYPE=Release \
    && make -j$(nproc)

# 阶段 2：运行
FROM ubuntu:22.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive

# 安装运行时依赖（仅 Boost 运行时库）
RUN apt-get update && apt-get install -y \
    libboost-system1.74.0 \
    libboost-thread1.74.0 \
    libboost-json1.74.0 \
    && rm -rf /var/lib/apt/lists/*

# 创建非 root 用户
RUN useradd -m -s /bin/bash nuclaw

# 复制构建产物
WORKDIR /app
COPY --from=builder /build/build/step20_demo /app/

# 设置权限
RUN chown -R nuclaw:nuclaw /app

USER nuclaw

# 健康检查
HEALTHCHECK --interval=30s --timeout=3s --start-period=5s --retries=3 \
    CMD /app/step20_demo --health-check || exit 1

EXPOSE 8080

CMD ["./step20_demo"]
