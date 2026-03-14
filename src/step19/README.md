# Step 19: SaaS 化 AI 助手平台

🏢 多租户 SaaS 平台演示

## 快速开始

```bash
# 编译
g++ -std=c++17 -I include src/main.cpp -o saas_platform

# 运行
./saas_platform
```

## 功能

- ✅ 租户管理
- ✅ API Key 认证
- ✅ 配额控制
- ✅ 知识库隔离

## 文件结构

```
include/saas/
└── simple_tenant.hpp    # 租户管理
src/
└── main.cpp             # 演示程序
```
