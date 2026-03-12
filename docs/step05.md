# Step 5: 多租户架构

> 目标：实现 Schema-per-Tenant 数据隔离。
> 
> 代码量：约 300 行

## 架构

```
Database: nuclaw
├── Schema: public
│   └── tenants (元数据)
├── Schema: tenant_xxx
│   ├── users
│   ├── sessions
│   └── messages
└── Schema: tenant_yyy
    ├── users
    ├── sessions
    └── messages
```

## 核心组件

```cpp
class TenantManager {
    Tenant create(const std::string& name);
    auto with_tenant(const std::string& id, F&& f);
};
```

## 依赖

```bash
sudo apt-get install libpqxx-dev
```

## 编译

```bash
g++ -std=c++17 main.cpp -o server \
    -lboost_system -lpqxx -lpthread
```

## 运行

```bash
export DATABASE_URL=postgresql://user:pass@localhost/nuclaw
./server
```

## 完成

🎉 教程结束！你已掌握 AI Agent 网关的核心架构。

---

**扩展方向:**
- 工具系统 (Tool Calling)
- 流式响应 (SSE)
- 认证授权 (JWT)
- 监控告警 (Prometheus)
