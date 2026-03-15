# Step 10: RAG 检索 —— 给 Agent 装上"外接大脑"

> 目标：实现 RAG（检索增强生成），使用向量数据库存储和检索知识
> 
003e 难度：⭐⭐⭐⭐ | 代码量：约 800 行 | 预计学习时间：4-5 小时

---

## 一、为什么需要 RAG？

### 1.1 现有系统的局限

Step 9 的工具系统很强大，但 Agent 缺乏**知识储备**：

```
用户: "公司的请假流程是什么？"
Agent: 抱歉，我没有相关信息。

用户: "项目文档里怎么说的？"
Agent: 我不知道有哪些项目文档。

用户: "根据我的笔记，我最喜欢的颜色是什么？"
Agent: 抱歉，我看不到您的笔记。
```

**问题核心：** Agent 只能调用实时 API，无法访问历史文档、私有知识。

### 1.2 LLM 的知识局限

即使接入 GPT-4，也有以下问题：

| 问题 | 说明 | 示例 |
|:---|:---|:---|
| **知识截止时间** | 训练数据有截止日期 | 不知道 2024 年后的产品 |
| **没有私有数据** | 无法访问企业内部文档 | 公司制度、产品手册 |
| **幻觉问题** | 可能编造不存在的信息 | 一本正经地胡说八道 |
| **无法引用来源** | 不知道信息来自哪里 | 无法提供文档出处 |

### 1.3 RAG 的解决方案

```
用户提问
    │
    ▼
┌─────────────────┐
│  1. 向量化查询   │  ← "请假流程" → [0.1, 0.2, -0.3, ...]
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  2. 向量检索    │  ← 在向量数据库中找最相似的文档片段
│  (Top-K 搜索)   │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  3. 构建上下文  │  ← "根据公司制度第三章：请假需要..."
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  4. LLM 生成    │  ← 结合检索到的知识生成回复
│  (有依据的回答)  │
└─────────────────┘
```

**RAG 优势：**
- ✅ 访问私有知识
- ✅ 减少幻觉（有依据）
- ✅ 知识可更新（添加新文档）
- ✅ 可追溯来源

---

## 二、核心概念

### 2.1 向量嵌入（Embedding）

**什么是 Embedding？**

将文本转换为高维向量，语义相似的文本在向量空间中距离近：

```
"今天天气很好"     → [0.1, 0.2, -0.5, ..., 0.3]  (1536 维)
"今天阳光明媚"     → [0.15, 0.18, -0.48, ..., 0.32]  (相似)
"Python 编程"      → [-0.3, 0.8, 0.2, ..., -0.1]  (不相似)
```

**Embedding 模型：**

| 模型 | 维度 | 特点 |
|:---|:---|:---|
| OpenAI text-embedding-3-small | 1536 | 效果好，需 API Key |
| OpenAI text-embedding-ada-002 | 1536 | 上一代模型 |
| sentence-transformers (本地) | 384/768 | 无需联网，隐私好 |

### 2.2 向量数据库

**什么是向量数据库？**

专门存储和检索向量的数据库，支持**相似度搜索**：

```
┌─────────────────────────────────────────┐
│           Vector Database               │
├─────────────────────────────────────────┤
│                                         │
│   ID    Text                    Vector  │
│   ────────────────────────────────────  │
│   1     "公司请假流程..."       [0.1...]│
│   2     "报销制度说明..."       [0.3...]│
│   3     "用户喜欢蓝色"          [0.2...]│
│   ...                                   │
│                                         │
│   查询："怎么请假？"                    │
│   ↓                                     │
│   向量：[0.12, 0.19, -0.51, ...]        │
│   ↓                                     │
│   相似度搜索                            │
│   ↓                                     │
│   结果：ID 1 (相似度 0.92)              │
│        "公司请假流程..."                │
│                                         │
└─────────────────────────────────────────┘
```

**向量数据库选择：**

| 数据库 | 特点 | 适用场景 |
|:---|:---|:---|
| **Chroma** | 轻量、易用、嵌入式 | 开发、小规模 |
| **pgvector** | PostgreSQL 扩展 | 已有 PG 环境 |
| **Pinecone** | 托管服务 | 生产环境 |
| **Milvus** | 高性能 | 大规模 |

### 2.3 相似度计算

**余弦相似度**：

```
cos(θ) = (A · B) / (|A| × |B|)

A · B = A[0]*B[0] + A[1]*B[1] + ... + A[n]*B[n]
|A| = sqrt(A[0]² + A[1]² + ... + A[n]²)
```

```cpp
float cosine_similarity(const std::vector<float>& a, 
                        const std::vector<float>& b) {
    float dot = 0.0f, norm_a = 0.0f, norm_b = 0.0f;
    for (size_t i = 0; i < a.size(); i++) {
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }
    return dot / (std::sqrt(norm_a) * std::sqrt(norm_b));
}
```

**相似度范围：** -1（完全相反）到 1（完全相同），通常 > 0.7 认为相关。

---

## 三、核心实现

### 3.1 Embedding 客户端

```cpp
class EmbeddingClient {
public:
    EmbeddingClient(asio::io_context& io, const std::string& api_key)
        : io_(io), api_key_(api_key) {}
    
    // 获取文本的向量表示
    void embed(const std::string& text,
               std::function<void(std::vector<float>)> callback);
    
    // 批量获取（更高效）
    void embed_batch(const std::vector<std::string>& texts,
                     std::function<void(std::vector<std::vector<float>>)> callback);

private:
    asio::io_context& io_;
    std::string api_key_;
};

void EmbeddingClient::embed(const std::string& text,
                            std::function<void(std::vector<float>)> callback) {
    // 构造请求
    json request = {
        {"model", "text-embedding-3-small"},
        {"input", text}
    };
    
    // 发送 HTTP 请求到 OpenAI
    http_client_.post("https://api.openai.com/v1/embeddings",
                      request.dump(),
                      [callback](HttpResponse resp) {
        if (!resp.success) {
            callback({});
            return;
        }
        
        try {
            json j = json::parse(resp.body);
            std::vector<float> embedding = 
                j["data"][0]["embedding"].get<std::vector<float>>();
            callback(embedding);
        } catch (...) {
            callback({});
        }
    });
}
```

### 3.2 内存向量存储（简化版）

```cpp
struct Document {
    std::string id;
    std::string content;
    std::vector<float> embedding;
    std::map<std::string, std::string> metadata;
};

class VectorStore {
public:
    // 添加文档
    void add_document(const Document& doc);
    
    // 相似度搜索
    std::vector<Document> search(const std::vector<float>& query_embedding,
                                   size_t top_k = 5);
    
    // 删除文档
    void delete_document(const std::string& id);

private:
    float cosine_similarity(const std::vector<float>& a,
                           const std::vector<float>& b);
    
    std::vector<Document> documents_;
    std::mutex mutex_;
};

void VectorStore::add_document(const Document& doc) {
    std::lock_guard<std::mutex> lock(mutex_);
    documents_.push_back(doc);
}

std::vector<Document> VectorStore::search(const std::vector<float>& query,
                                          size_t top_k) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 计算所有文档的相似度
    std::vector<std::pair<float, size_t>> scores;  // (相似度, 索引)
    
    for (size_t i = 0; i < documents_.size(); i++) {
        float sim = cosine_similarity(query, documents_[i].embedding);
        scores.push_back({sim, i});
    }
    
    // 按相似度排序（降序）
    std::sort(scores.begin(), scores.end(),
              [](const auto& a, const auto& b) {
                  return a.first > b.first;
              });
    
    // 返回 Top-K
    std::vector<Document> results;
    for (size_t i = 0; i < std::min(top_k, scores.size()); i++) {
        results.push_back(documents_[scores[i].second]);
    }
    
    return results;
}
```

### 3.3 RAG 管理器

```cpp
class RAGManager {
public:
    RAGManager(EmbeddingClient& embedder, VectorStore& store)
        : embedder_(embedder), store_(store) {}
    
    // 添加知识文档
    void add_knowledge(const std::string& id,
                       const std::string& content,
                       const std::map<std::string, std::string>& metadata = {});
    
    // 检索相关知识
    void retrieve(const std::string& query,
                  size_t top_k,
                  std::function<void(std::vector<Document>)> callback);
    
    // 构建增强的 Prompt
    std::string build_augmented_prompt(
        const std::string& user_query,
        const std::vector<Document>& retrieved_docs
    );

private:
    EmbeddingClient& embedder_;
    VectorStore& store_;
};

void RAGManager::add_knowledge(const std::string& id,
                               const std::string& content,
                               const std::map<std::string, std::string>& metadata) {
    // 1. 获取向量表示
    embedder_.embed(content, [this, id, content, metadata](std::vector<float> embedding) {
        // 2. 存储到向量数据库
        Document doc{
            .id = id,
            .content = content,
            .embedding = embedding,
            .metadata = metadata
        };
        store_.add_document(doc);
    });
}

void RAGManager::retrieve(const std::string& query,
                          size_t top_k,
                          std::function<void(std::vector<Document>)> callback) {
    // 1. 向量化查询
    embedder_.embed(query, [this, top_k, callback](std::vector<float> query_emb) {
        // 2. 相似度搜索
        auto results = store_.search(query_emb, top_k);
        callback(results);
    });
}

std::string RAGManager::build_augmented_prompt(
    const std::string& user_query,
    const std::vector<Document>& retrieved_docs) {
    
    std::string prompt = "基于以下信息回答问题：\n\n";
    
    // 添加检索到的文档
    for (size_t i = 0; i < retrieved_docs.size(); i++) {
        prompt += "[文档 " + std::to_string(i + 1) + "]\n";
        prompt += retrieved_docs[i].content + "\n\n";
    }
    
    prompt += "用户问题：" + user_query + "\n\n";
    prompt += "请基于上述文档回答，如果文档中没有相关信息，请明确说明。";
    
    return prompt;
}
```

---

## 四、集成到 Agent

```cpp
class RAGAgent {
public:
    RAGAgent(LLMClient& llm, 
             EmbeddingClient& embedder,
             VectorStore& store)
        : llm_(llm), rag_manager_(embedder, store) {
        
        // 加载知识库（示例）
        load_knowledge_base();
    }
    
    void process(const std::string& user_input,
                 std::function<void(const std::string&)> callback);

private:
    void load_knowledge_base() {
        // 添加公司制度文档
        rag_manager_.add_knowledge(
            "leave_policy",
            "公司请假流程：1. 提前 3 天在 OA 系统提交申请。"
            "2. 直属领导审批。3. 人事部门备案。"
            "4. 紧急情况可事后补假。",
            {{"category", "制度"}, {"department", "人事"}}
        );
        
        // 添加产品文档
        rag_manager_.add_knowledge(
            "product_intro",
            "NuClaw 是一个 AI Agent 开发框架，"
            "支持工具调用、RAG、多 Agent 协作。",
            {{"category", "产品"}}
        );
    }
    
    LLMClient& llm_;
    RAGManager rag_manager_;
};

void RAGAgent::process(const std::string& user_input,
                       std::function<void(const std::string&)> callback) {
    
    // 1. 检索相关知识
    rag_manager_.retrieve(user_input, 3, 
        [this, user_input, callback](std::vector<Document> docs) {
            
            // 2. 构建增强 Prompt
            std::string augmented_prompt = 
                rag_manager_.build_augmented_prompt(user_input, docs);
            
            // 3. 调用 LLM
            std::vector<Message> messages = {
                {"user", augmented_prompt}
            };
            
            llm_.chat(messages, [callback](auto ec, std::string reply) {
                if (ec) {
                    callback("抱歉，服务暂时不可用");
                } else {
                    callback(reply);
                }
            });
        }
    );
}
```

---

## 五、本章小结

**核心收获：**

1. **RAG 流程**：
   - 向量化查询
   - 向量检索
   - 构建增强 Prompt
   - LLM 生成有依据的回复

2. **向量技术**：
   - Embedding 模型
   - 向量数据库
   - 余弦相似度

3. **知识管理**：
   - 文档向量化存储
   - 语义检索
   - 来源可追溯

---

## 六、引出的问题

### 6.1 多 Agent 协作

复杂任务需要多个 Agent 协作：

```
用户: "帮我规划一个去日本的旅行"

需要：
- 行程规划 Agent
- 酒店查询 Agent  
- 交通查询 Agent
- 预算计算 Agent
```

**需要：** 多 Agent 架构、任务分发、结果整合。

---

**下一章预告（Step 11）：**

我们将实现**多 Agent 协作**：
- Agent 通信协议
- 任务分解和分配
- Coordinator 模式
- 结果整合

Agent 已经有工具和知识，接下来要让多个 Agent 协同工作。
