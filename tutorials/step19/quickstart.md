# Step 19: SaaS 化 AI 助手平台 - 完整实战案例

> **目标**：构建生产级多租户 AI 平台
> 
003e **案例**：智能客服 SaaS 平台
> 
003e **预计时间**：8-10 小时

---

## 📋 实战案例概述

### 业务场景

你正在创建一个智能客服 SaaS 平台，服务企业客户：

**平台运营方**：你
- 管理租户、套餐、计费
- 监控系统、处理运维

**租户 A**：电商平台
- 管理员：张总
- 客服：小李、小王
- 知识库：产品 FAQ、退货政策
- 接入：官网、微信公众号

**租户 B**：教育机构
- 管理员：李老师
- 教师：张老师、王老师
- 知识库：课程资料、招生信息
- 接入：官网、钉钉群

### 我们要实现的功能

1. **多租户隔离** - A 公司的数据 B 公司看不到
2. **认证授权** - OAuth2、API Key、权限控制
3. **租户管理** - 创建、配置、配额管理
4. **计费系统** - 按量计费、发票、报表
5. **运营后台** - 平台级监控、收入统计

---

## 🚀 快速开始（30 分钟 MVP）

### 第一步：项目结构

```bash
cd NuClaw
cp -r src/step18 src/step19
cd src/step19

mkdir -p include/saas/services src/services
```

### 第二步：最简多租户系统

```cpp
// include/saas/simple_tenant.hpp
#pragma once
#include <string>
#include <map>
#include <vector>
#include <mutex>
#include <nlohmann/json.hpp>

namespace saas {

// 租户
struct Tenant {
    std::string tenant_id;
    std::string name;
    std::string api_key;  // 用于认证
    
    // 配额
    int max_conversations = 1000;
    int used_conversations = 0;
    
    // 知识库（简单存储）
    std::vector<std::string> knowledge_base;
    
    bool check_quota() const {
        return used_conversations < max_conversations;
    }
};

// 租户管理器
class TenantManager {
private:
    std::map<std::string, Tenant> tenants_;  // api_key -> tenant
    std::mutex mutex_;
    
public:
    // 创建租户
    Tenant create_tenant(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        Tenant tenant;
        tenant.tenant_id = "tenant_" + generate_random_string(8);
        tenant.name = name;
        tenant.api_key = "sk_" + generate_random_string(32);
        
        tenants_[tenant.api_key] = tenant;
        return tenant;
    }
    
    // 验证 API Key
    bool validate_api_key(const std::string& api_key) {
        std::lock_guard<std::mutex> lock(mutex_);
        return tenants_.count(api_key) > 0;
    }
    
    // 获取租户
    Tenant* get_tenant(const std::string& api_key) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = tenants_.find(api_key);
        if (it != tenants_.end()) {
            return &it->second;
        }
        return nullptr;
    }
    
    // 记录用量
    void record_usage(const std::string& api_key) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = tenants_.find(api_key);
        if (it != tenants_.end()) {
            it->second.used_conversations++;
        }
    }
    
    // 获取所有租户（平台管理用）
    std::vector<Tenant> list_all() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<Tenant> result;
        for (const auto& [key, tenant] : tenants_) {
            result.push_back(tenant);
        }
        return result;
    }

private:
    std::string generate_random_string(int length) {
        const std::string chars = "abcdefghijklmnopqrstuvwxyz0123456789";
        std::string result;
        for (int i = 0; i < length; ++i) {
            result += chars[rand() % chars.length()];
        }
        return result;
    }
};

// 聊天服务
class ChatService {
private:
    TenantManager& tenant_manager_;
    
public:
    ChatService(TenantManager& tm) : tenant_manager_(tm) {}
    
    struct ChatRequest {
        std::string api_key;
        std::string message;
        std::string session_id;
    };
    
    struct ChatResponse {
        bool success;
        std::string message;
        std::string error;
        int remaining_quota;
    };
    
    ChatResponse chat(const ChatRequest& req) {
        // 1. 验证 API Key
        if (!tenant_manager_.validate_api_key(req.api_key)) {
            return {false, "", "Invalid API Key", 0};
        }
        
        // 2. 获取租户
        Tenant* tenant = tenant_manager_.get_tenant(req.api_key);
        if (!tenant) {
            return {false, "", "Tenant not found", 0};
        }
        
        // 3. 检查配额
        if (!tenant->check_quota()) {
            return {false, "", "Quota exceeded. Please upgrade your plan.", 0};
        }
        
        // 4. 处理消息（这里用简单的回复模拟）
        std::string reply = generate_reply(req.message, *tenant);
        
        // 5. 记录用量
        tenant_manager_.record_usage(req.api_key);
        
        // 6. 返回结果
        return {
            true,
            reply,
            "",
            tenant->max_conversations - tenant->used_conversations
        };
    }

private:
    std::string generate_reply(const std::string& msg, const Tenant& tenant) {
        // 简单模拟：在知识库中查找关键词
        for (const auto& knowledge : tenant.knowledge_base) {
            if (msg.find(knowledge.substr(0, 4)) != std::string::npos) {
                return "根据我们的资料：" + knowledge;
            }
        }
        
        // 默认回复
        return "您好！我是 " + tenant.name + " 的智能客服。\n"
               "有什么可以帮您的吗？";
    }
};

} // namespace saas
```

### 第三步：HTTP API 服务

```cpp
// src/main.cpp
#include "saas/simple_tenant.hpp"
#include <cpp-httplib/httplib.h>
#include <iostream>
#include <nlohmann/json.hpp>

using namespace saas;

int main() {
    httplib::Server server;
    TenantManager tenant_manager;
    ChatService chat_service(tenant_manager);
    
    // ===== 平台管理接口 =====
    
    // 创建租户
    server.Post("/admin/tenants", [&](const httplib::Request& req, httplib::Response& res) {
        auto body = nlohmann::json::parse(req.body);
        std::string name = body.value("name", "");
        
        if (name.empty()) {
            res.status = 400;
            res.set_content(R"({"error": "name is required"})", "application/json");
            return;
        }
        
        Tenant tenant = tenant_manager.create_tenant(name);
        
        nlohmann::json response = {
            {"tenant_id", tenant.tenant_id},
            {"name", tenant.name},
            {"api_key", tenant.api_key},
            {"max_conversations", tenant.max_conversations}
        };
        
        res.set_content(response.dump(), "application/json");
    });
    
    // 列出所有租户
    server.Get("/admin/tenants", [&](const httplib::Request& req, httplib::Response& res) {
        auto tenants = tenant_manager.list_all();
        
        nlohmann::json response = nlohmann::json::array();
        for (const auto& tenant : tenants) {
            response.push_back({
                {"tenant_id", tenant.tenant_id},
                {"name", tenant.name},
                {"used_conversations", tenant.used_conversations},
                {"max_conversations", tenant.max_conversations}
            });
        }
        
        res.set_content(response.dump(), "application/json");
    });
    
    // ===== 租户接口 =====
    
    // 添加知识库
    server.Post("/knowledge", [&](const httplib::Request& req, httplib::Response& res) {
        std::string api_key = req.get_header_value("X-API-Key");
        
        Tenant* tenant = tenant_manager.get_tenant(api_key);
        if (!tenant) {
            res.status = 401;
            res.set_content(R"({"error": "Unauthorized"})", "application/json");
            return;
        }
        
        auto body = nlohmann::json::parse(req.body);
        std::string knowledge = body.value("content", "");
        
        tenant->knowledge_base.push_back(knowledge);
        
        res.set_content(R"({"status": "added"})", "application/json");
    });
    
    // 对话接口
    server.Post("/chat", [&](const httplib::Request& req, httplib::Response& res) {
        std::string api_key = req.get_header_value("X-API-Key");
        
        auto body = nlohmann::json::parse(req.body);
        
        ChatService::ChatRequest chat_req;
        chat_req.api_key = api_key;
        chat_req.message = body.value("message", "");
        chat_req.session_id = body.value("session_id", "");
        
        auto chat_res = chat_service.chat(chat_req);
        
        nlohmann::json response;
        if (chat_res.success) {
            response = {
                {"message", chat_res.message},
                {"remaining_quota", chat_res.remaining_quota}
            };
        } else {
            res.status = 400;
            response = {
                {"error", chat_res.error}
            };
        }
        
        res.set_content(response.dump(), "application/json");
    });
    
    // 健康检查
    server.Get("/health", [](const httplib::Request& req, httplib::Response& res) {
        res.set_content(R"({"status": "healthy"})", "application/json");
    });
    
    std::cout << "🏢 SaaS AI 平台启动在 http://localhost:8080\n";
    std::cout << "\nAPI 端点：\n";
    std::cout << "  POST /admin/tenants - 创建租户\n";
    std::cout << "  GET  /admin/tenants - 列出租户\n";
    std::cout << "  POST /knowledge - 添加知识（需 API Key）\n";
    std::cout << "  POST /chat - 对话（需 API Key）\n";
    std::cout << "\n按 Ctrl+C 停止\n";
    
    server.listen("0.0.0.0", 8080);
    
    return 0;
}
```

### 第四步：编译运行

```bash
# 编译（需要 httplib 和 nlohmann/json）
g++ -std=c++17 -I include src/main.cpp -o saas_platform -lpthread

# 运行
./saas_platform

🏢 SaaS AI 平台启动在 http://localhost:8080

API 端点：
  POST /admin/tenants - 创建租户
  GET  /admin/tenants - 列出租户
  POST /knowledge - 添加知识（需 API Key）
  POST /chat - 对话（需 API Key）

按 Ctrl+C 停止
```

### 第五步：测试 API

```bash
# 1. 创建租户 A（电商平台）
curl -X POST http://localhost:8080/admin/tenants \
  -H "Content-Type: application/json" \
  -d '{"name": "示例电商"}'

# 响应：
# {
#   "tenant_id": "tenant_abc123",
#   "name": "示例电商",
#   "api_key": "sk_x8j2k9m3n4p5q6r7s8t9u0v1w2x3y4z5",
#   "max_conversations": 1000
# }

# 保存 API Key
TENANT_A_KEY="sk_x8j2k9m3n4p5q6r7s8t9u0v1w2x3y4z5"

# 2. 创建租户 B（教育机构）
curl -X POST http://localhost:8080/admin/tenants \
  -H "Content-Type: application/json" \
  -d '{"name": "示例教育"}'

TENANT_B_KEY="sk_a1b2c3d4e5f6g7h8i9j0k1l2m3n4o5p6"

# 3. 为租户 A 添加知识库
curl -X POST http://localhost:8080/knowledge \
  -H "X-API-Key: $TENANT_A_KEY" \
  -H "Content-Type: application/json" \
  -d '{"content": "我们支持7天无理由退货"}'

curl -X POST http://localhost:8080/knowledge \
  -H "X-API-Key: $TENANT_A_KEY" \
  -H "Content-Type: application/json" \
  -d '{"content": "满99元包邮"}'

# 4. 租户 A 的用户对话
curl -X POST http://localhost:8080/chat \
  -H "X-API-Key: $TENANT_A_KEY" \
  -H "Content-Type: application/json" \
  -d '{"message": "怎么退货"}'

# 响应：
# {
#   "message": "根据我们的资料：我们支持7天无理由退货",
#   "remaining_quota": 999
# }

# 5. 租户 B 的用户对话（没有知识库）
curl -X POST http://localhost:8080/chat \
  -H "X-API-Key: $TENANT_B_KEY" \
  -H "Content-Type: application/json" \
  -d '{"message": "怎么退货"}'

# 响应（默认回复，因为租户 B 没有退货相关的知识）：
# {
#   "message": "您好！我是 示例教育 的智能客服。\n有什么可以帮您的吗？",
#   "remaining_quota": 999
# }

# 6. 查看所有租户（平台管理）
curl http://localhost:8080/admin/tenants

# 响应：
# [
#   {"tenant_id": "tenant_abc123", "name": "示例电商", 
#    "used_conversations": 2, "max_conversations": 1000},
#   {"tenant_id": "tenant_def456", "name": "示例教育",
#    "used_conversations": 1, "max_conversations": 1000}
# ]
```

---

## 进阶：完整功能清单

- [ ] 用户认证（OAuth2、JWT）
- [ ] 角色权限（管理员、操作员、访客）
- [ ] 数据隔离（Database/Schema/Table 级别）
- [ ] 向量数据库隔离（Milvus 多 Collection）
- [ ] 资源配额（按量计费、套餐管理）
- [ ] 计费系统（月度账单、发票）
- [ ] 运营报表（收入、活跃度、Top 租户）
- [ ] API 限流（租户级、用户级）
- [ ] 日志审计（所有操作可追溯）
- [ ] 多区域部署（就近访问）

---

## Docker 部署

```dockerfile
# Dockerfile
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    g++ \
    libboost-all-dev \
    nlohmann-json3-dev

WORKDIR /app
COPY . .
RUN g++ -std=c++17 -I include src/main.cpp -o saas_platform -lpthread

EXPOSE 8080

CMD ["./saas_platform"]
```

```yaml
# docker-compose.yml
version: '3.8'

services:
  platform:
    build: .
    ports:
      - "8080:8080"
    environment:
      - LOG_LEVEL=info
    
  # 后续可以添加：
  # postgres:
  #   image: postgres:15
  #   
  # redis:
  #   image: redis:7-alpine
  #   
  # milvus:
  #   image: milvusdb/milvus:latest
```

---

**恭喜！你已经搭建了一个最简单的 SaaS 平台！** 🏢

现在你可以：
1. 创建更多租户
2. 为每个租户配置不同的知识库
3. 实现真正的 AI 对话（接入 LLM）
4. 添加计费系统，开始赚钱！💰
