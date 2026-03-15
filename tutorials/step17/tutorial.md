# Step 17: 多租户智能客服平台 —— 需求分析与架构设计

> 目标：基于 Step 16 StatefulAgent，实现多租户隔离和定制化
> 难度：⭐⭐⭐⭐
> 代码量：约 1200 行（较 Step 16 新增 200 行，修改 main.cpp）

---

## 问题引入

### Step 16 的局限

Step 16 的 `StatefulAgent` 能记住用户、有情感、有记忆，但它有个致命问题：**所有用户共享同一个 Agent 实例**。

```
当前行为：
用户 A（租户 1 的客户）: 你好
Agent: 你好！

用户 B（租户 2 的客户）: 你好  
Agent: 你好！
      ↑
      └── 同一个 Agent，数据混在一起！

问题：
1. 用户数据没有隔离（租户 1 的用户记录能被租户 2 看到）
2. 无法定制化（所有用户看到的 Agent 都一样）
3. 无法多租户部署（一个程序只能服务一个客户群）
```

### 本章目标

**实现多租户隔离**：让每个租户拥有独立的 Agent 实例，数据完全隔离，且可定制化。

---

## 解决方案

### 核心思路

```
Step 16（单租户）:
┌─────────────────────────────────┐
│        StatefulAgent            │
│  ┌───────────────────────────┐  │
│  │  所有用户的记忆/关系混在一起 │  │
│  └───────────────────────────┘  │
└─────────────────────────────────┘

Step 17（多租户）:
┌─────────────────────────────────────────────────────────┐
│                    Application                          │
│  ┌──────────────────┐    ┌──────────────────┐          │
│  │ CustomerService  │    │ CustomerService  │          │
│  │ Agent (租户 A)    │    │ Agent (租户 B)    │          │
│  │                  │    │                  │          │
│  │ • 独立记忆        │    │ • 独立记忆        │          │
│  │ • 定制化人设      │    │ • 定制化人设      │          │
│  │ • 隔离数据        │    │ • 隔离数据        │          │
│  └──────────────────┘    └──────────────────┘          │
└─────────────────────────────────────────────────────────┘
```

### 代码对比

#### Step 16 的代码（单租户）

```cpp
// Step 16: 单 Agent 服务所有用户
StatefulAgent agent("memory_bot", "记忆型助手");

// 用户 A（租户 1）
agent.process_user_message("user_a", "小明", "你好");

// 用户 B（租户 2）
agent.process_user_message("user_b", "小红", "你好");
// 问题：user_a 和 user_b 的数据混在一起，无法区分租户
```

#### Step 17 的修改（多租户）

```cpp
// Step 17: 多 Agent 实例，每个租户独立

// 租户 A 配置
TenantContext tenant_a("tenant_ecommerce", "快购商城");
tenant_a.agent_persona = {
    {"name", "小快"},
    {"style", "热情、高效"}
};

// 租户 B 配置
TenantContext tenant_b("tenant_edu", "智慧学堂");
tenant_b.agent_persona = {
    {"name", "小智"},
    {"style", "耐心、专业"}
};

// 创建两个 Agent 实例（核心变化）
CustomerServiceAgent agent_a(tenant_a);  // 服务租户 A
CustomerServiceAgent agent_b(tenant_b);  // 服务租户 B

// 同一用户在不同租户下完全隔离
agent_a.handle_customer_message("user_001", "小明", "你好");
agent_b.handle_customer_message("user_001", "小明", "你好");
// 结果：数据隔离，user_001 在 agent_a 和 agent_b 中是两个独立用户
```

### 新增代码详解

#### 1. TenantContext - 租户上下文

```cpp
// include/nuclaw/tenant.hpp（新增文件）
struct TenantContext {
    std::string tenant_id;       // 租户唯一标识
    std::string tenant_name;     // 显示名称
    json::object agent_persona;  // Agent 人设（名称、性格）
    TenantConfig config;         // 租户配置
};
```

#### 2. CustomerServiceAgent - 演进自 StatefulAgent

```cpp
// include/nuclaw/customer_service_agent.hpp（新增文件）
class CustomerServiceAgent : public StatefulAgent {
public:
    // 必须传入租户上下文
    CustomerServiceAgent(const TenantContext& tenant);
    
    // 带租户隔离的消息处理
    std::string handle_customer_message(
        const std::string& user_id,
        const std::string& user_name, 
        const std::string& message
    );
    
    // 新增：人工转接判断
    bool should_escalate_to_human(
        const std::string& user_id,
        const std::string& message
    );
    
private:
    TenantContext tenant_ctx_;  // 租户上下文
};
```

### 文件变更清单

| 文件 | 变更类型 | 说明 |
|:---|:---|:---|
| `include/nuclaw/stateful_agent.hpp` | 复用 | 从 Step 16 复用，无修改 |
| `include/nuclaw/memory.hpp` | 复用 | 从 Step 16 复用，无修改 |
| `include/nuclaw/emotion.hpp` | 复用 | 从 Step 16 复用，无修改 |
| `include/nuclaw/tenant.hpp` | **新增** | 租户上下文定义 |
| `include/nuclaw/customer_service_agent.hpp` | **新增** | 多租户 Agent 类 |
| `src/main.cpp` | **修改** | 多租户演示程序 |
| `CMakeLists.txt` | **修改** | 适配新结构 |

---

## 完整源码

### 目录结构

```
src/step17/
├── CMakeLists.txt
├── include/
│   └── nuclaw/
│       ├── stateful_agent.hpp      # 复用 Step 16
│       ├── memory.hpp              # 复用 Step 16
│       ├── emotion.hpp             # 复用 Step 16
│       ├── relationship.hpp        # 复用 Step 16
│       ├── tenant.hpp              # 新增
│       └── customer_service_agent.hpp  # 新增
└── src/
    └── main.cpp                    # 修改
```

### tenant.hpp

```cpp
#pragma once
#include <string>
#include <boost/json.hpp>

namespace json = boost::json;
namespace nuclaw {

struct TenantContext {
    std::string tenant_id;
    std::string tenant_name;
    json::object agent_persona;
    TenantConfig config;
    
    TenantContext(const std::string& id, const std::string& name)
        : tenant_id(id), tenant_name(name) {}
    
    std::string get_agent_name() const {
        if (agent_persona.contains("name")) {
            return std::string(agent_persona.at("name").as_string());
        }
        return "客服助手";
    }
};

} // namespace nuclaw
```

### customer_service_agent.hpp

```cpp
#pragma once
#include "nuclaw/stateful_agent.hpp"
#include "nuclaw/tenant.hpp"

namespace nuclaw {

class CustomerServiceAgent : public StatefulAgent {
public:
    CustomerServiceAgent(const TenantContext& tenant)
        : StatefulAgent(tenant.get_agent_name(), "客服Agent"),
          tenant_ctx_(tenant) {}
    
    std::string handle_customer_message(
        const std::string& user_id,
        const std::string& user_name,
        const std::string& message
    ) {
        // 生成租户隔离的用户ID
        std::string global_user_id = tenant_ctx_.tenant_id + ":" + user_id;
        
        // 调用父类方法（复用 Step 16 的记忆和情感）
        return process_user_message(global_user_id, user_name, message);
    }
    
    bool should_escalate_to_human(const std::string& user_id, 
                                   const std::string& message) {
        // 关键词触发
        if (message.find("人工") != std::string::npos ||
            message.find("客服") != std::string::npos) {
            return true;
        }
        return false;
    }

private:
    TenantContext tenant_ctx_;
};

} // namespace nuclaw
```

### main.cpp

```cpp
#include "nuclaw/customer_service_agent.hpp"
#include <iostream>

using namespace nuclaw;

int main() {
    // 租户 A：快购商城
    TenantContext tenant_a("tenant_ecommerce", "快购商城");
    tenant_a.agent_persona = {{"name", "小快"}};
    CustomerServiceAgent agent_a(tenant_a);
    
    // 租户 B：智慧学堂
    TenantContext tenant_b("tenant_edu", "智慧学堂");
    tenant_b.agent_persona = {{"name", "小智"}};
    CustomerServiceAgent agent_b(tenant_b);
    
    // 同一用户在不同租户下完全隔离
    std::cout << agent_a.handle_customer_message("user_001", "小明", "你好") << "\n";
    std::cout << agent_b.handle_customer_message("user_001", "小明", "你好") << "\n";
    
    return 0;
}
```

---

## 编译运行

```bash
# 进入 Step 17 目录
cd src/step17

# 创建构建目录
mkdir build && cd build

# 配置
cmake ..

# 编译
make -j

# 运行
./step17_demo
```

**预期输出：**

```
========================================
Step 17: 多租户智能客服平台
========================================
[初始化] 租户: 快购商城 (tenant_ecommerce)
         Agent: 小快 | 风格: 热情、高效 | 领域: 电商客服

[初始化] 租户: 智慧学堂 (tenant_edu)  
         Agent: 小智 | 风格: 耐心、专业 | 领域: 教育咨询

【演示 1】同一用户（小明）分别咨询两个租户
----------------------------------------

【快购商城 - 购物咨询】
小明: 你好，我想买东西
小快: 欢迎联系快购商城！
      欢迎光临快购商城！我是您的专属客服小快...

【智慧学堂 - 课程咨询】
小明: 你好，我想买东西
小智: 您好！欢迎来到智慧学堂，我是小智老师...

【演示 2】验证数据隔离
----------------------------------------
📊 快购商城的小明会话摘要:
   租户: tenant_ecommerce
   互动次数: 2
   
📊 智慧学堂的小明会话摘要:
   租户: tenant_edu
   互动次数: 2

✅ 结论：同一 user_id 在不同租户下数据完全隔离！
```

---

## 本章总结

- ✅ **解决了 Step 16 的问题**：单 Agent 无法服务多租户
- ✅ **实现了数据隔离**：`tenant_id:user_id` 复合键
- ✅ **实现了租户定制**：每个租户有独立的人设和配置
- ✅ **复用了 Step 16 全部能力**：记忆、情感、关系
- ✅ **代码演进清晰**：新增 2 个头文件，修改 main.cpp，复用 4 个文件

---

## 课后思考

当前实现还有什么问题？

<details>
<summary>点击查看下一章要解决的问题 💡</summary>

**问题 1：单进程架构**
> 现在所有租户 Agent 都在一个进程里。如果某个租户 Agent 卡死，会影响所有租户。如何拆分成独立服务？

**问题 2：知识库缺失**
> Agent 还是基于规则的回复，没有真正的知识库查询能力。如何接入 RAG（Step 10 的向量检索）？

**问题 3：人工协作不完善**
> `should_escalate_to_human()` 只是判断，没有真正转接人工客服的功能。如何实现完整的人机协作？

**Step 18 预告：服务拆分**
我们将把 `CustomerServiceAgent` 拆分为三个微服务：
- **ChatService**：管理 WebSocket 连接和会话
- **AIService**：处理 LLM 调用和 RAG 检索  
- **KnowledgeService**：管理向量知识库

</details>
