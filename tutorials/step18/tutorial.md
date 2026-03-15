# Step 18: 智能客服 SaaS 平台 —— 核心功能实现

> 目标：实现 Chat Service、AI Service、Knowledge Service 三大核心服务
> 
003e 难度：⭐⭐⭐⭐⭐ | 代码量：约 1500 行 | 预计学习时间：5-6 小时

---

## 一、项目结构

### 1.1 代码组织

```
smartsupport/
├── CMakeLists.txt              # 根构建配置
├── config/
│   ├── smartsupport.yaml       # 主配置
│   └── tenants/                # 租户配置模板
├── src/
│   ├── common/                 # 公共库
│   │   ├── types.hpp           # 通用类型定义
│   │   ├── utils.hpp           # 工具函数
│   │   └── database.hpp        # 数据库连接池
│   │
│   ├── gateway/                # 接入层
│   │   ├── server.hpp          # HTTP/WebSocket 服务器
│   │   ├── websocket.hpp       # WebSocket 管理
│   │   └── im_adapters/        # IM 平台适配器
│   │       ├── feishu.hpp
│   │       └── wechat.hpp
│   │
│   ├── services/               # 业务服务
│   │   ├── chat/               # 会话服务
│   │   │   ├── service.hpp
│   │   │   ├── session.hpp
│   │   │   └── router.hpp
│   │   │
│   │   ├── ai/                 # AI 服务
│   │   │   ├── service.hpp
│   │   │   ├── agent.hpp
│   │   │   └── rag.hpp
│   │   │
│   │   ├── knowledge/          # 知识服务
│   │   │   ├── service.hpp
│   │   │   ├── indexer.hpp
│   │   │   └── retriever.hpp
│   │   │
│   │   └── tenant/             # 租户服务
│   │       ├── service.hpp
│   │       └── isolator.hpp
│   │
│   └── main.cpp                # 入口
│
├── tests/                      # 测试
└── docker/                     # 部署配置
    ├── Dockerfile
    └── docker-compose.yml
```

### 1.2 根 CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.14)
project(smartsupport VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找依赖
find_package(Boost REQUIRED COMPONENTS system thread)
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBPQXX REQUIRED libpqxx)

# 第三方库（FetchContent）
include(FetchContent)
FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz
)
FetchContent_MakeAvailable(json)

# 公共库
add_library(common STATIC
    src/common/types.cpp
    src/common/utils.cpp
    src/common/database.cpp
)
target_link_libraries(common
    Boost::system
    Threads::Threads
    ${LIBPQXX_LIBRARIES}
    nlohmann_json::nlohmann_json
)

# 服务库
add_library(chat_service STATIC
    src/services/chat/service.cpp
    src/services/chat/session.cpp
)
target_link_libraries(chat_service common)

add_library(ai_service STATIC
    src/services/ai/service.cpp
    src/services/ai/agent.cpp
    src/services/ai/rag.cpp
)
target_link_libraries(ai_service common)

add_library(knowledge_service STATIC
    src/services/knowledge/service.cpp
    src/services/knowledge/indexer.cpp
    src/services/knowledge/retriever.cpp
)
target_link_libraries(knowledge_service common)

# 可执行文件
add_executable(smartsupport src/main.cpp)
target_link_libraries(smartsupport
    chat_service
    ai_service
    knowledge_service
    common
)
```

---

## 二、Chat Service 实现

### 2.1 会话管理

```cpp
// src/services/chat/session.hpp
#pragma once

#include "common/types.hpp"

namespace smartsupport::chat {

// 会话状态
enum class SessionState {
    ACTIVE,          // 活跃（AI 处理中）
    WAITING_AI,      // 等待 AI 响应
    WITH_HUMAN,      // 人工客服接入
    CLOSED           // 已关闭
};

// 会话
class Session : public std::enable_shared_from_this<Session> {
public:
    Session(const std::string& id,
            const std::string& tenant_id,
            const std::string& user_id,
            const std::string& channel);
    
    // 添加用户消息
    void add_user_message(const std::string& content);
    
    // 添加 AI 回复
    void add_ai_message(const std::string& content,
                        const std::vector<std::string>& sources);
    
    // 获取历史（用于 RAG 上下文）
    std::vector<Message> get_recent_history(size_t limit = 10) const;
    
    // 状态管理
    void transition_to(SessionState new_state);
    SessionState get_state() const;
    
    // 元数据
    std::string get_id() const { return id_; }
    std::string get_tenant_id() const { return tenant_id_; }

private:
    std::string id_;
    std::string tenant_id_;
    std::string user_id_;
    std::string channel_;
    SessionState state_ = SessionState::ACTIVE;
    
    std::vector<Message> messages_;
    std::chrono::steady_clock::time_point last_activity_;
};

} // namespace
```

```cpp
// src/services/chat/service.hpp
#pragma once

#include "session.hpp"
#include "services/ai/service.hpp"
#include "services/knowledge/service.hpp"

namespace smartsupport::chat {

class ChatService {
public:
    ChatService(asio::io_context& io,
                std::shared_ptr<ai::AIService> ai_service,
                std::shared_ptr<knowledge::KnowledgeService> kb_service);
    
    // WebSocket 连接管理
    void on_connect(const std::string& conn_id,
                    const std::string& tenant_id,
                    const std::string& user_id);
    
    void on_disconnect(const std::string& conn_id);
    
    void on_message(const std::string& conn_id,
                    const std::string& content);
    
    // 发送消息给客户端
    void send_to_client(const std::string& conn_id,
                        const json& message);
    
    // 广播给会话
    void broadcast_to_session(const std::string& session_id,
                              const json& message);

private:
    // 获取或创建会话
    std::shared_ptr<Session> get_or_create_session(
        const std::string& tenant_id,
        const std::string& user_id
    );
    
    // 路由决策：AI 还是人工
    enum class RouteTarget { AI, HUMAN };
    RouteDecision decide_route(const Session& session,
                               const std::string& message);
    
    // 处理 AI 响应
    void handle_ai_response(const std::string& session_id,
                            const std::string& conn_id,
                            ai::AIResponse response);

    asio::io_context& io_;
    std::shared_ptr<ai::AIService> ai_service_;
    std::shared_ptr<knowledge::KnowledgeService> kb_service_;
    
    // 会话管理
    std::map<std::string, std::shared_ptr<Session>> sessions_;
    std::map<std::string, std::string> conn_to_session_;
    std::map<std::string, WebSocketConnection> connections_;
    std::mutex sessions_mutex_;
};

} // namespace
```

### 2.2 消息路由

```cpp
// src/services/chat/router.cpp

RouteDecision ChatService::decide_route(const Session& session,
                                        const std::string& message) {
    // 规则 1：用户明确要求人工
    if (contains_keywords(message, {"人工", "客服", "转人工", "找人"})) {
        return RouteDecision{
            .target = RouteTarget::HUMAN,
            .reason = "user_requested"
        };
    }
    
    // 规则 2：会话状态是 WITH_HUMAN（已经在人工服务中）
    if (session.get_state() == SessionState::WITH_HUMAN) {
        return RouteDecision{
            .target = RouteTarget::HUMAN,
            .reason = "already_with_human"
        };
    }
    
    // 规则 3：AI 连续 3 次低置信度
    auto recent = session.get_recent_history(5);
    int low_confidence_count = 0;
    for (const auto& msg : recent) {
        if (msg.sender == "ai" && msg.confidence < 0.5f) {
            low_confidence_count++;
        }
    }
    if (low_confidence_count >= 3) {
        return RouteDecision{
            .target = RouteTarget::HUMAN,
            .reason = "ai_struggling"
        };
    }
    
    // 默认：AI 处理
    return RouteDecision{
        .target = RouteTarget::AI,
        .reason = "default"
    };
}
```

---

## 三、AI Service 实现

### 3.1 Agent 核心

```cpp
// src/services/ai/agent.hpp

namespace smartsupport::ai {

// 租户级 Agent 配置
struct AgentConfig {
    std::string system_prompt;
    float temperature = 0.7f;
    int max_context_length = 4000;
    std::vector<std::string> tools_enabled;
    bool enable_rag = true;
    float confidence_threshold = 0.6f;
};

class TenantAgent {
public:
    TenantAgent(const std::string& tenant_id,
                const AgentConfig& config,
                std::shared_ptr<knowledge::KnowledgeService> kb,
                LLMClient& llm);
    
    // 处理用户输入
    struct Response {
        std::string content;
        bool needs_escalation;
        float confidence;
        std::vector<std::string> knowledge_sources;
        std::vector<ToolCall> tool_calls;
    };
    
    void process(const std::string& session_id,
                 const std::vector<Message>& history,
                 const std::string& user_input,
                 std::function<void(Response)> callback);

private:
    // 构建 Prompt
    std::string build_prompt(const std::vector<Message>& history,
                             const std::string& user_input,
                             const std::vector<KnowledgeDoc>& knowledge);
    
    // 执行工具
    void execute_tools(const std::vector<ToolCall>& calls,
                       std::function<void(json)> callback);
    
    // 评估置信度
    float evaluate_confidence(const std::string& response,
                              const std::vector<KnowledgeDoc>& knowledge);

    std::string tenant_id_;
    AgentConfig config_;
    std::shared_ptr<knowledge::KnowledgeService> kb_;
    LLMClient& llm_;
    ToolExecutor tools_;
};

} // namespace
```

```cpp
// src/services/ai/agent.cpp

void TenantAgent::process(const std::string& session_id,
                          const std::vector<Message>& history,
                          const std::string& user_input,
                          std::function<void(Response)> callback) {
    
    // 1. RAG 检索相关知识
    std::vector<KnowledgeDoc> knowledge;
    if (config_.enable_rag) {
        knowledge = kb_>retrieve(tenant_id_, user_input, 5);
    }
    
    // 2. 构建 Prompt
    std::string prompt = build_prompt(history, user_input, knowledge);
    
    // 3. 调用 LLM
    llm_.chat({{"system", config_.system_prompt},
               {"user", prompt}},
        [this, callback, knowledge](const std::string& llm_response) {
            
            // 4. 评估置信度
            float confidence = evaluate_confidence(llm_response, knowledge);
            
            // 5. 检查是否需要转人工
            bool needs_escalation = confidence < config_.confidence_threshold;
            
            // 6. 提取知识来源
            std::vector<std::string> sources;
            for (const auto& doc : knowledge) {
                sources.push_back(doc.title);
            }
            
            callback(Response{
                .content = llm_response,
                .needs_escalation = needs_escalation,
                .confidence = confidence,
                .knowledge_sources = sources
            });
        }
    );
}

std::string TenantAgent::build_prompt(
    const std::vector<Message>& history,
    const std::string& user_input,
    const std::vector<KnowledgeDoc>& knowledge) {
    
    std::ostringstream prompt;
    
    // 知识上下文
    if (!knowledge.empty()) {
        prompt << "以下是相关知识，请基于这些信息回答：\n";
        for (size_t i = 0; i < knowledge.size(); ++i) {
            prompt << "[" << (i + 1) << "] " << knowledge[i].content << "\n";
        }
        prompt << "\n";
    }
    
    // 历史对话（最近的 5 轮）
    size_t start = history.size() > 10 ? history.size() - 10 : 0;
    for (size_t i = start; i < history.size(); ++i) {
        prompt << (history[i].sender == "user" ? "用户" : "助手")
                  << "：" << history[i].content << "\n";
    }
    
    // 当前输入
    prompt << "用户：" << user_input << "\n助手：";
    
    return prompt.str();
}
```

### 3.2 AI Service 封装

```cpp
// src/services/ai/service.hpp

class AIService {
public:
    AIService(asio::io_context& io, const Config& config);
    
    // 获取或创建租户 Agent
    TenantAgent& get_agent(const std::string& tenant_id);
    
    // 处理消息（异步）
    void process(const std::string& tenant_id,
                 const std::string& session_id,
                 const std::vector<Message>& history,
                 const std::string& user_input,
                 std::function<void(AIResponse)> callback);

private:
    // 加载租户配置
    AgentConfig load_tenant_config(const std::string& tenant_id);
    
    asio::io_context& io_;
    Config config_;
    LLMClient llm_client_;
    std::shared_ptr<knowledge::KnowledgeService> kb_service_;
    
    // 租户 Agent 缓存
    std::map<std::string, std::unique_ptr<TenantAgent>> agents_;
    std::mutex agents_mutex_;
};
```

---

## 四、Knowledge Service 实现

### 4.1 文档处理与索引

```cpp
// src/services/knowledge/indexer.hpp

namespace smartsupport::knowledge {

class DocumentIndexer {
public:
    DocumentIndexer(EmbeddingClient& embedder,
                   std::shared_ptr<VectorStore> vector_store,
                   std::shared_ptr<Database> db);
    
    // 添加文档
    void add_document(const std::string& tenant_id,
                      const Document& doc,
                      std::function<void(bool)> callback);
    
    // 删除文档
    void delete_document(const std::string& tenant_id,
                         const std::string& doc_id);

private:
    // 文本分块（Chunking）
    std::vector<std::string> chunk_text(const std::string& text,
                                          size_t chunk_size = 500,
                                          size_t overlap = 50);
    
    // 生成向量嵌入
    void generate_embeddings(const std::vector<std::string>& chunks,
                             std::function<void(std::vector<std::vector<float>>)> callback);
    
    EmbeddingClient& embedder_;
    std::shared_ptr<VectorStore> vector_store_;
    std::shared_ptr<Database> db_;
};

} // namespace
```

```cpp
// src/services/knowledge/indexer.cpp

std::vector<std::string> DocumentIndexer::chunk_text(
    const std::string& text,
    size_t chunk_size,
    size_t overlap) {
    
    std::vector<std::string> chunks;
    
    // 简单的滑动窗口分块
    size_t start = 0;
    while (start < text.size()) {
        size_t end = std::min(start + chunk_size, text.size());
        
        // 尽量在句子边界分割
        if (end < text.size()) {
            size_t sentence_end = text.find_last_of("。！？.!?\n", end);
            if (sentence_end != std::string::npos && sentence_end > start) {
                end = sentence_end + 1;
            }
        }
        
        chunks.push_back(text.substr(start, end - start));
        start = end - overlap;  // 重叠以保持上下文
    }
    
    return chunks;
}

void DocumentIndexer::add_document(const std::string& tenant_id,
                                   const Document& doc,
                                   std::function<void(bool)> callback) {
    // 1. 文本分块
    auto chunks = chunk_text(doc.content);
    
    // 2. 生成嵌入
    generate_embeddings(chunks, 
        [this, tenant_id, doc, chunks, callback](auto embeddings) {
            if (embeddings.empty()) {
                callback(false);
                return;
            }
            
            // 3. 存储到向量数据库
            for (size_t i = 0; i < chunks.size(); ++i) {
                KnowledgeChunk chunk{
                    .id = generate_uuid(),
                    .tenant_id = tenant_id,
                    .doc_id = doc.id,
                    .content = chunks[i],
                    .embedding = embeddings[i],
                    .chunk_index = static_cast<int>(i)
                };
                
                vector_store_>add(chunk);
            }
            
            // 4. 存储文档元数据
            db_>execute(
                "INSERT INTO knowledge_docs (id, tenant_id, title, content) "
                "VALUES ($1, $2, $3, $4)",
                {doc.id, tenant_id, doc.title, doc.content}
            );
            
            callback(true);
        }
    );
}
```

### 4.2 知识检索

```cpp
// src/services/knowledge/retriever.hpp

class KnowledgeRetriever {
public:
    KnowledgeRetriever(EmbeddingClient& embedder,
                       std::shared_ptr<VectorStore> vector_store);
    
    // 语义检索
    std::vector<KnowledgeDoc> retrieve(const std::string& tenant_id,
                                        const std::string& query,
                                        size_t top_k = 5);
    
    // 混合检索（语义 + 关键词）
    std::vector<KnowledgeDoc> hybrid_retrieve(const std::string& tenant_id,
                                               const std::string& query,
                                               size_t top_k = 5);

private:
    // 重排序（使用更精确的模型）
    std::vector<KnowledgeDoc> rerank(const std::vector<KnowledgeDoc>& candidates,
                                      const std::string& query);
    
    EmbeddingClient& embedder_;
    std::shared_ptr<VectorStore> vector_store_;
};
```

### 4.3 Knowledge Service 封装

```cpp
// src/services/knowledge/service.hpp

class KnowledgeService {
public:
    KnowledgeService(const Config& config,
                     EmbeddingClient& embedder);
    
    // 索引文档
    void index_document(const std::string& tenant_id,
                        const Document& doc,
                        std::function<void(bool)> callback);
    
    // 检索知识
    std::vector<KnowledgeDoc> retrieve(const std::string& tenant_id,
                                        const std::string& query,
                                        size_t top_k = 5);
    
    // 删除知识
    void delete_document(const std::string& tenant_id,
                         const std::string& doc_id);

private:
    std::unique_ptr<DocumentIndexer> indexer_;
    std::unique_ptr<KnowledgeRetriever> retriever_;
};
```

---

## 五、服务集成

### 5.1 主程序入口

```cpp
// src/main.cpp

#include "services/chat/service.hpp"
#include "services/ai/service.hpp"
#include "services/knowledge/service.hpp"
#include "gateway/server.hpp"

int main(int argc, char* argv[]) {
    try {
        // 1. 加载配置
        auto config = Config::load("config/smartsupport.yaml");
        
        // 2. 初始化 IO 上下文
        asio::io_context io;
        
        // 3. 初始化服务（按依赖顺序）
        // 知识服务（AI 服务依赖它）
        auto knowledge_service = std::make_shared<knowledge::KnowledgeService>(
            config.knowledge,
            embedding_client
        );
        
        // AI 服务
        auto ai_service = std::make_shared<ai::AIService>(
            io,
            config.ai,
            knowledge_service
        );
        
        // 会话服务
        auto chat_service = std::make_shared<chat::ChatService>(
            io,
            ai_service,
            knowledge_service
        );
        
        // 4. 启动 HTTP/WebSocket 服务器
        GatewayServer server(io, config.server, chat_service);
        server.start();
        
        // 5. 运行事件循环
        std::vector<std::thread> threads;
        for (size_t i = 0; i < config.server.worker_threads; ++i) {
            threads.emplace_back([&io]() { io.run(); });
        }
        
        io.run();  // 主线程也参与
        
        // 6. 等待结束
        for (auto& t : threads) {
            t.join();
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
```

---

## 六、本章小结

**核心收获：**

1. **项目结构**：多模块 CMake 项目组织
2. **Chat Service**：WebSocket 会话管理、消息路由
3. **AI Service**：租户级 Agent、RAG 集成、置信度评估
4. **Knowledge Service**：文档分块、向量索引、语义检索

---

## 七、引出的问题

### 7.1 高级功能缺失

当前实现了核心对话能力，但缺少：

```
• 人工客服接入（人机协作）
• 多租户隔离实现
• 计费与套餐管理
• 管理后台界面
```

---

**下一章预告（Step 19）：**

我们将实现高级功能：
- Human Service：人工客服工作台、会话接管
- Tenant Service：多租户数据隔离、资源配额
- Billing Service：套餐计费、用量统计
- Admin Dashboard：租户管理、运营分析

核心功能已完成，接下来是 SaaS 化必需的高级功能。
