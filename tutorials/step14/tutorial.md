# Step 14: CI/CD 持续集成/部署

> 目标：实现自动化测试、构建和部署流程
> 
003e 难度：⭐⭐⭐ | 预计学习时间：2-3 小时

---

## 一、问题引入

### 1.1 手动部署的问题

```
手动部署流程：
1. 本地修改代码
2. 手动编译测试
3. 手动构建 Docker 镜像
4. 手动推送到仓库
5. 手动登录服务器部署
6. 出问题回滚困难

问题：
- 容易出错
- 耗时费力
- 无法回滚
- 多人协作冲突
```

### 1.2 CI/CD 流程

```
代码提交
    │
    ▼
┌─────────────────┐
│   CI Pipeline   │  ← 持续集成
│                 │
│  • 编译        │
│  • 单元测试    │
│  • 代码检查    │
│  • 构建镜像    │
└────────┬────────┘
         │
         ▼
    测试通过？
         │
    ┌────┴────┐
    │         │
   否        是
    │         │
    ▼         ▼
  失败    ┌─────────────────┐
  告警    │   CD Pipeline   │  ← 持续部署
          │                 │
          │  • 推送镜像    │
          │  • 部署到测试  │
          │  • 集成测试    │
          │  • 部署到生产  │
          └─────────────────┘
```

---

## 二、GitHub Actions 配置

### 2.1 CI 工作流

**.github/workflows/ci.yml：**

```yaml
name: CI

on:
  push:
    branches: [main, develop]
  pull_request:
    branches: [main]

jobs:
  build:
    runs-on: ubuntu-22.04
    
    steps:
      - name: Checkout code
        uses: actions/checkout@v4
      
      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y \
            build-essential \
            cmake \
            libboost-all-dev \
            libssl-dev \
            libsqlite3-dev
      
      - name: Configure CMake
        run: |
          mkdir build
          cd build
          cmake .. -DCMAKE_BUILD_TYPE=Release
      
      - name: Build
        run: cmake --build build --parallel $(nproc)
      
      - name: Run tests
        run: |
          cd build
          ctest --output-on-failure
  
  lint:
    runs-on: ubuntu-22.04
    
    steps:
      - uses: actions/checkout@v4
      
      - name: Run clang-format
        uses: jidicula/clang-format-action@v4.11.0
        with:
          clang-format-version: '14'
          check-path: 'src'
      
      - name: Run clang-tidy
        run: |
          sudo apt-get install -y clang-tidy
          find src -name '*.cpp' -exec clang-tidy {} \;
```

### 2.2 CD 工作流

**.github/workflows/cd.yml：**

```yaml
name: CD

on:
  push:
    tags:
      - 'v*'

env:
  REGISTRY: ghcr.io
  IMAGE_NAME: ${{ github.repository }}

jobs:
  build-and-push:
    runs-on: ubuntu-22.04
    permissions:
      contents: read
      packages: write
    
    steps:
      - name: Checkout
        uses: actions/checkout@v4
      
      - name: Set up Docker Buildx
        uses: docker/setup-buildx-action@v3
      
      - name: Log in to Container Registry
        uses: docker/login-action@v3
        with:
          registry: ${{ env.REGISTRY }}
          username: ${{ github.actor }}
          password: ${{ secrets.GITHUB_TOKEN }}
      
      - name: Extract metadata
        id: meta
        uses: docker/metadata-action@v5
        with:
          images: ${{ env.REGISTRY }}/${{ env.IMAGE_NAME }}
          tags: |
            type=ref,event=tag
            type=semver,pattern={{version}}
            type=semver,pattern={{major}}.{{minor}}
      
      - name: Build and push Docker image
        uses: docker/build-push-action@v5
        with:
          context: .
          push: true
          tags: ${{ steps.meta.outputs.tags }}
          labels: ${{ steps.meta.outputs.labels }}
          cache-from: type=gha
          cache-to: type=gha,mode=max
  
  deploy-staging:
    needs: build-and-push
    runs-on: ubuntu-22.04
    environment: staging
    
    steps:
      - name: Deploy to staging
        run: |
          echo "Deploying to staging server..."
          # ssh 到服务器执行部署脚本
          # 或使用 Kubernetes API
  
  deploy-production:
    needs: deploy-staging
    runs-on: ubuntu-22.04
    environment: production
    
    steps:
      - name: Deploy to production
        run: |
          echo "Deploying to production server..."
```

---

## 三、自动化测试

### 3.1 测试目录结构

```
tests/
├── CMakeLists.txt
├── unit/
│   ├── test_session.cpp
│   ├── test_router.cpp
│   └── test_memory.cpp
├── integration/
│   ├── test_websocket.cpp
│   └── test_llm.cpp
└── fixtures/
    └── mock_llm_client.h
```

### 3.2 单元测试示例

```cpp
#include <gtest/gtest.h>
#include "session_manager.h"

class SessionManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        session_mgr_ = std::make_unique<SessionManager>();
    }
    
    std::unique_ptr<SessionManager> session_mgr_;
};

TEST_F(SessionManagerTest, CreateSession) {
    std::string session_id = session_mgr_->create_session("user123");
    
    EXPECT_FALSE(session_id.empty());
    
    SessionData* data = session_mgr_->get_session(session_id);
    EXPECT_NE(data, nullptr);
    EXPECT_EQ(data->user_id, "user123");
}

TEST_F(SessionManagerTest, GetNonExistentSession) {
    SessionData* data = session_mgr_->get_session("nonexistent");
    EXPECT_EQ(data, nullptr);
}

TEST_F(SessionManagerTest, CleanupExpiredSessions) {
    // 创建会话
    std::string id = session_mgr_->create_session();
    
    // 模拟过期（手动修改时间）
    // ...
    
    // 清理
    session_mgr_->cleanup_expired(std::chrono::minutes(0));
    
    // 验证已删除
    EXPECT_EQ(session_mgr_->get_session(id), nullptr);
}
```

---

## 四、部署策略

### 4.1 蓝绿部署

```
生产环境 v1          生产环境 v2
┌─────────┐          ┌─────────┐
│ 蓝色    │    →     │ 绿色    │
│ v1.0.0  │          │ v1.1.0  │
└────┬────┘          └────┬────┘
     │                    │
     └────────┬───────────┘
              │
         负载均衡器
              │
         切换流量
```

### 4.2 金丝雀发布

```
90% 流量 → v1.0.0（稳定版本）
10% 流量 → v1.1.0（新版本）

监控指标（错误率、延迟）
         │
    ┌────┴────┐
    │         │
  正常       异常
    │         │
    ▼         ▼
 全量切换   回滚
```

---

## 五、本章总结

- ✅ GitHub Actions CI/CD 工作流
- ✅ 自动化测试
- ✅ 自动构建 Docker 镜像
- ✅ 部署策略

---

## 六、课后思考

应用功能完整，但性能还有优化空间：
- 连接池复用
- 缓存策略
- 异步处理

<details>
<summary>点击查看下一章 💡</summary>

**Step 15: 性能优化**

我们将学习：
- 连接池
- 缓存策略
- 异步 LLM 调用
- 性能分析工具

</details>
