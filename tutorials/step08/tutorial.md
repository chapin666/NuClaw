# Step 8: 长期记忆与 RAG

> 目标：实现长期记忆存储，使用向量数据库和 RAG 增强回复
> 
> 难度：⭐⭐⭐⭐ | 代码量：约 650 行 | 预计学习时间：4-5 小时

---

## 一、问题引入

### 1.1 Step 7 的问题

短期记忆解决了上下文长度问题，但还有局限：

```
用户周一问："我的密码是什么？"
Agent："您的密码是 xxx"

用户周三再问："我的密码是什么？"
Agent："抱歉，我不知道"  ← 摘要把密码细节丢失了！

用户周五说："记住我喜欢 Python"
用户下个月说："我应该学什么编程语言？"
Agent："这取决于您的兴趣"  ← 完全忘记了！
```

**核心问题：**
- 摘要会丢失细节
- 滑动窗口会丢失早期信息
- 无法跨会话记住重要信息

### 1.2 我们需要什么？

**长期记忆（Long-term Memory）：**
- 永久存储重要信息
- 精确检索相关内容
- 跨会话保持记忆

**RAG（Retrieval-Augmented Generation）：**
- 检索相关信息
- 增强 LLM 回复
- 支持外部知识库

---

## 二、核心概念

### 2.1 向量嵌入（Embedding）

**什么是 Embedding？**

将文本转换为高维向量，语义相似的文本向量距离近：

```
"我喜欢 Python"     → [0.1, 0.2, -0.5, ..., 0.3]  (1536 维)
"Python 很受欢迎"   → [0.15, 0.18, -0.48, ..., 0.32]  (相似)
"今天天气很好"      → [-0.3, 0.8, 0.2, ..., -0.1]  (不相似)
```

**Embedding 模型：**
| 模型 | 维度 | 特点 |
|:---|:---|:---|
| OpenAI text-embedding-ada-002 | 1536 | 效果好，需 API |
| OpenAI text-embedding-3-small | 1536 | 更便宜 |
| local sentence-transformers | 384-768 | 本地运行 |

### 2.2 向量数据库

**什么是向量数据库？**

专门存储和检索向量数据的数据库：

```
┌─────────────────────────────────────────┐
│           Vector Database               │
├─────────────────────────────────────────┤
│                                         │
│   ID    Text                    Vector  │
│   ────────────────────────────────────  │
│   1     "用户喜欢 Python"       [0.1...]│
│   2     "用户密码是 123456"     [0.3...]│
│   3     "用户住在北京"          [0.2...]│
│   ...                                   │
│                                         │
│   查询："我该学什么语言？"              │
│   ↓                                     │
│   向量：[0.12, 0.19, -0.51, ...]        │
│   ↓                                     │
│   相似度搜索                            │
│   ↓                                     │
│   结果：ID 1 (相似度 0.95)              │
│                                         │
└─────────────────────────────────────────┘
```

**向量数据库选择：**
| 数据库 | 特点 | 适用场景 |
|:---|:---|:---|
| Chroma | 轻量、易用 | 开发/小规模 |
| Pinecone | 托管服务 | 生产环境 |
| Milvus | 高性能 | 大规模 |
| pgvector | PostgreSQL 扩展 | 已有 PG 环境 |

### 2.3 RAG 流程

**RAG（检索增强生成）：**

```
用户提问
    │
    ▼
┌─────────────────┐
│  1. 向量化查询   │  ← "我该学什么语言？" → Vector
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  2. 向量检索    │  ← 在数据库中找最相似的
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  3. 获取上下文  │  ← "用户喜欢 Python"
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  4. 构建提示    │  ← 结合检索结果构建 Prompt
│                 │
│  系统：你是助手  │
│  上下文：用户喜欢 Python
│  用户：我该学什么语言？
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  5. LLM 生成    │  ← "既然您喜欢 Python，可以深入学习..."
└─────────────────┘
```

---

## 三、代码结构详解

### 3.1 Embedding 客户端

```cpp
class EmbeddingClient {
public:
    EmbeddingClient(HttpClient& http, const std::string& api_key)
        : http_(http), api_key_(api_key) {}
    
    // 获取文本的向量表示
    std::vector<float> embed(const std::string& text) {
        json request = {
            {"model", "text-embedding-ada-002"},
            {"input", text}
        };
        
        std::map<std::string, std::string> headers = {
            {"Authorization", "Bearer " + api_key_},
            {"Content-Type", "application/json"}
        };
        
        std::string response = http_.post(
            "api.openai.com",
            "/v1/embeddings",
            request.dump(),
            headers
        );
        
        return parse_embedding(response);
    }
    
    // 批量获取
    std::vector<std::vector<float>> embed_batch(
        const std::vector<std::string>& texts) {
        json request = {
            {"model", "text-embedding-ada-002"},
            {"input", texts}
        };
        
        // ... 发送请求
        return parse_embeddings(response);
    }

private:
    std::vector<float> parse_embedding(const std::string& response) {
        json j = json::parse(response);
        std::vector<float> embedding = 
            j["data"][0]["embedding"].get<std::vector<float>>();
        return embedding;
    }

    HttpClient& http_;
    std::string api_key_;
};
```

### 3.2 内存向量存储（简化版）

```cpp
struct MemoryItem {
    std::string id;
    std::string content;
    std::vector<float> embedding;
    std::string session_id;
    std::chrono::system_clock::time_point timestamp;
    std::map<std::string, std::string> metadata;
};

class VectorStore {
public:
    // 添加记忆
    void add(const MemoryItem& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        items_.push_back(item);
    }
    
    // 相似度搜索
    std::vector<MemoryItem> search(
        const std::vector<float>& query_embedding,
        size_t top_k = 5,
        const std::string& session_filter = "") {
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<std::pair<float, MemoryItem>> scored;
        
        for (const auto& item : items_) {
            // 会话过滤
            if (!session_filter.empty() && 
                item.session_id != session_filter) {
                continue;
            }
            
            // 计算余弦相似度
            float similarity = cosine_similarity(
                query_embedding, 
                item.embedding
            );
            
            scored.push_back({similarity, item});
        }
        
        // 按相似度排序
        std::sort(scored.begin(), scored.end(),
            [](const auto& a, const auto& b) {
                return a.first > b.first;
            }
        );
        
        // 返回 top_k
        std::vector<MemoryItem> results;
        for (size_t i = 0; i < std::min(top_k, scored.size()); ++i) {
            results.push_back(scored[i].second);
        }
        
        return results;
    }
    
    // 删除某个会话的记忆
    void delete_by_session(const std::string& session_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        items_.erase(
            std::remove_if(items_.begin(), items_.end(),
                [&](const MemoryItem& item) {
                    return item.session_id == session_id;
                }
            ),
            items_.end()
        );
    }

private:
    // 余弦相似度：cos(θ) = (A·B) / (|A| × |B|)
    float cosine_similarity(
        const std::vector<float>& a,
        const std::vector<float>& b) {
        
        if (a.size() != b.size()) return 0.0f;
        
        float dot = 0.0f;
        float norm_a = 0.0f;
        float norm_b = 0.0f;
        
        for (size_t i = 0; i < a.size(); ++i) {
            dot += a[i] * b[i];
            norm_a += a[i] * a[i];
            norm_b += b[i] * b[i];
        }
        
        return dot / (std::sqrt(norm_a) * std::sqrt(norm_b));
    }

    std::vector<MemoryItem> items_;
    std::mutex mutex_;
};
```

### 3.3 长期记忆管理器

```cpp
class LongTermMemory {
public:
    LongTermMemory(EmbeddingClient& embedder, VectorStore& store)
        : embedder_(embedder), store_(store) {}
    
    // 保存重要信息到长期记忆
    void remember(const std::string& content,
                  const std::string& session_id,
                  const std::map<std::string, std::string>& metadata = {}) {
        
        // 1. 获取向量表示
        auto embedding = embedder_.embed(content);
        
        // 2. 生成唯一 ID
        std::string id = generate_id();
        
        // 3. 存储到向量数据库
        MemoryItem item;
        item.id = id;
        item.content = content;
        item.embedding = embedding;
        item.session_id = session_id;
        item.timestamp = std::chrono::system_clock::now();
        item.metadata = metadata;
        
        store_.add(item);
    }
    
    // 检索相关记忆
    std::vector<std::string> recall(
        const std::string& query,
        const std::string& session_id = "",
        size_t top_k = 3) {
        
        // 1. 向量化查询
        auto query_embedding = embedder_.embed(query);
        
        // 2. 相似度搜索
        auto results = store_.search(query_embedding, top_k, session_id);
        
        // 3. 返回内容
        std::vector<std::string> contents;
        for (const auto& item : results) {
            contents.push_back(item.content);
        }
        return contents;
    }
    
    // 从对话中提取并保存重要信息
    void extract_and_save(const std::vector<json>& history,
                          const std::string& session_id) {
        // 这里简化处理，实际应该用 LLM 提取
        for (const auto& msg : history) {
            if (msg["role"] == "assistant") {
                std::string content = msg.value("content", "");
                
                // 简单规则：包含"记住"的信息保存
                if (content.find("记住") != std::string::npos ||
                    content.find("用户") != std::string::npos) {
                    remember(content, session_id);
                }
            }
        }
    }

private:
    std::string generate_id() {
        // 简化 ID 生成
        static std::atomic<int> counter{0};
        return "mem_" + std::to_string(++counter);
    }

    EmbeddingClient& embedder_;
    VectorStore& store_;
};
```

### 3.4 集成 RAG 到 Agent

```cpp
class AgentSession : public enable_shared_from_this<AgentSession> {
public:
    AgentSession(tcp::socket socket,
                 SessionManager& session_mgr,
                 LLMClient& llm,
                 LongTermMemory& ltm)
        : ws_(move(socket)),
          session_mgr_(session_mgr),
          llm_(llm),
          ltm_(ltm) {}

private:
    void process() {
        // 1. 获取用户最后一条消息
        std::string last_message = 
            session_data_->history.back()["content"];
        
        // 2. 检索长期记忆
        auto memories = ltm_.recall(last_message, session_id_, 3);
        
        // 3. 构建增强的提示
        std::vector<json> augmented_history = build_augmented_prompt(
            session_data_->history,
            memories
        );
        
        // 4. 调用 LLM
        vector<Tool> tools = {weather_tool};
        LLMResponse response = llm_.chat(augmented_history, tools);
        
        // 5. 处理响应
        if (response.has_tool_call) {
            // ... 工具调用处理
        } else {
            send_reply(response.content);
        }
        
        // 6. 保存到长期记忆（可选）
        ltm_.extract_and_save(session_data_->history, session_id_);
    }
    
    std::vector<json> build_augmented_prompt(
        const std::vector<json>& history,
        const std::vector<std::string>& memories) {
        
        std::vector<json> augmented;
        
        // 系统提示
        augmented.push_back({
            {"role", "system"},
            {"content", "你是智能助手。以下是与用户相关的历史信息："}
        });
        
        // 添加检索到的记忆
        std::string memory_text;
        for (const auto& mem : memories) {
            memory_text += "- " + mem + "\n";
        }
        
        if (!memory_text.empty()) {
            augmented.push_back({
                {"role", "system"},
                {"content", "[相关记忆]\n" + memory_text}
            });
        }
        
        // 添加当前对话历史
        for (const auto& msg : history) {
            augmented.push_back(msg);
        }
        
        return augmented;
    }

    websocket::stream<tcp::socket> ws_;
    SessionManager& session_mgr_;
    LLMClient& llm_;
    LongTermMemory& ltm_;
    // ...
};
```

---

## 四、完整 RAG 流程示例

```
用户: 我喜欢 Python
    │
    ▼
Agent: 好的，我记住了
    │
    ├──▶ 保存到长期记忆
    │      "用户喜欢 Python"
    │
...
一周后...
...
用户: 我应该学什么编程语言？
    │
    ▼
检索相关记忆
    │
    ├──▶ 向量化查询："我应该学什么编程语言？"
    │
    ├──▶ 相似度搜索
    │      结果："用户喜欢 Python" (相似度 0.85)
    │
    ├──▶ 构建增强提示：
    │      [系统] 你是助手
    │      [记忆] 用户喜欢 Python
    │      [用户] 我应该学什么编程语言？
    │
    ▼
LLM 生成回复：
"既然您喜欢 Python，可以深入学习 Python 的高级特性，
比如异步编程、装饰器等。也可以学习相关领域如数据科学。"
```

---

## 五、本章总结

- ✅ 向量嵌入将文本转换为语义向量
- ✅ 向量数据库存储和检索向量
- ✅ RAG 流程增强 LLM 回复
- ✅ 长期记忆跨会话保持信息
- ✅ 代码从 550 行扩展到 650 行

---

## 六、课后思考

我们的 Agent 现在有了完整的记忆系统：
- **短期记忆**：对话上下文、滑动窗口、摘要
- **长期记忆**：向量存储、语义检索

但一个 Agent 的能力有限。复杂任务需要多个 Agent 协作：

```
用户: 帮我规划一个去日本的旅行

单个 Agent：
- 只能给出通用建议
- 无法处理复杂的多步骤任务

多个 Agent 协作：
- Agent 1（行程规划）：规划路线
- Agent 2（酒店预订）：查找酒店
- Agent 3（交通查询）：查询航班/火车
- Agent 4（预算管理）：计算费用
- Agent 5（整合）：汇总所有信息
```

<details>
<summary>点击查看下一章 💡</summary>

**Step 9: 多 Agent 协作**

我们将学习：
- 多 Agent 架构设计
- 任务分解和分配
- Agent 间通信
- 结果整合

</details>
