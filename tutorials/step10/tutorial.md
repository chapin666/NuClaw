# Step 10: RAG 检索 - 向量数据库与 Embedding

> 目标：实现知识检索增强生成，解决 LLM 知识局限
> 
> 难度：⭐⭐⭐⭐⭐ (困难)
> 
> 代码量：约 800 行
> 
> 预计学习时间：5-6 小时

---

## 🎯 Agent 开发知识点

**本节核心问题：** 如何让 Agent 具备领域知识，减少幻觉？

**Agent 架构中的位置：**
```
用户提问 → 检索相关知识 → 增强 Prompt → LLM 生成
                ↑
         向量数据库（领域知识库）
```

**关键能力：**
- 语义检索（不是关键词匹配）
- 上下文增强
- 引用溯源

---

## 📚 理论基础 + 代码实现

### 1. Embedding 原理与实现

**理论：**

Embedding 将文本映射到语义向量空间：
```
"猫"    → [0.2, -0.5, 0.8, ...]  (768维)
"狗"    → [0.3, -0.4, 0.7, ...]  ← 与"猫"距离近
"汽车"  → [-0.1, 0.2, -0.3, ...] ← 与"猫"距离远
```

**代码实现：**

```cpp
// embedding_client.hpp
class EmbeddingClient {
public:
    // 获取文本的 embedding 向量
    std::vector<float> get_embedding(const std::string& text) {
        // 实际项目：调用 OpenAI API
        // return call_openai_api(text);
        
        // 简化版：哈希生成（仅用于演示）
        const size_t dimension = 128;
        std::vector<float> vec(dimension, 0.0f);
        
        for (size_t i = 0; i < text.length(); ++i) {
            size_t idx = i % dimension;
            vec[idx] += static_cast<float>(text[i]) / 255.0f;
        }
        
        normalize(vec);
        return vec;
    }
    
    // 批量 embedding
    std::vector<std::vector<float>> get_embeddings(
        const std::vector<std::string>& texts) {
        std::vector<std::vector<float>> results;
        for (const auto& text : texts) {
            results.push_back(get_embedding(text));
        }
        return results;
    }

private:
    void normalize(std::vector<float>& vec) {
        float norm = 0.0f;
        for (float v : vec) norm += v * v;
        norm = std::sqrt(norm);
        if (norm > 0.0f) {
            for (float& v : vec) v /= norm;
        }
    }
};
```

### 2. 向量存储与相似度计算

**理论：**

余弦相似度衡量语义相似性：
```
cos(θ) = (A · B) / (|A| × |B|)

范围：-1 到 1
- 1：完全相同方向（语义相同）
- 0：正交（无关）
```

**代码实现：**

```cpp
// vector_store.hpp
struct Document {
    std::string id;
    std::string text;
    std::vector<float> embedding;
};

struct SearchResult {
    Document doc;
    float score;  // 相似度分数
};

class VectorStore {
public:
    void add_document(const Document& doc) {
        std::lock_guard<std::mutex> lock(mutex_);
        documents_.push_back(doc);
    }
    
    // 相似度搜索
    std::vector<SearchResult> search(
        const std::vector<float>& query_vec, 
        size_t top_k = 3) {
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<SearchResult> results;
        for (const auto& doc : documents_) {
            float score = cosine_similarity(query_vec, doc.embedding);
            results.push_back({doc, score});
        }
        
        // 按相似度排序
        std::sort(results.begin(), results.end(),
            [](const SearchResult& a, const SearchResult& b) {
                return a.score > b.score;
            });
        
        if (results.size() > top_k) {
            results.resize(top_k);
        }
        return results;
    }

private:
    float cosine_similarity(const std::vector<float>& a, 
                            const std::vector<float>& b) {
        if (a.size() != b.size() || a.empty()) return 0.0f;
        
        float dot = 0.0f, norm_a = 0.0f, norm_b = 0.0f;
        for (size_t i = 0; i < a.size(); ++i) {
            dot += a[i] * b[i];
            norm_a += a[i] * a[i];
            norm_b += b[i] * b[i];
        }
        
        if (norm_a == 0.0f || norm_b == 0.0f) return 0.0f;
        return dot / (std::sqrt(norm_a) * std::sqrt(norm_b));
    }
    
    mutable std::mutex mutex_;
    std::vector<Document> documents_;
};
```

### 3. 文档处理与分块

**理论：**

长文档需要分块处理：
- 超过 embedding 模型最大长度
- 小块语义更聚焦

**代码实现：**

```cpp
// document_processor.hpp
class DocumentProcessor {
public:
    struct ChunkConfig {
        size_t chunk_size = 500;        // 每块字符数
        size_t chunk_overlap = 50;      // 重叠字符数
    };
    
    std::vector<std::string> split_text(const std::string& text,
                                          const ChunkConfig& config = {}) {
        std::vector<std::string> chunks;
        std::string current_chunk;
        
        // 按段落分割
        std::vector<std::string> paragraphs = split_paragraphs(text);
        
        for (const auto& para : paragraphs) {
            if (current_chunk.length() + para.length() > config.chunk_size) {
                if (!current_chunk.empty()) {
                    chunks.push_back(current_chunk);
                }
                
                // 保留重叠部分
                if (current_chunk.length() > config.chunk_overlap) {
                    current_chunk = current_chunk.substr(
                        current_chunk.length() - config.chunk_overlap);
                } else {
                    current_chunk.clear();
                }
            }
            
            if (!current_chunk.empty()) {
                current_chunk += "\n\n";
            }
            current_chunk += para;
        }
        
        if (!current_chunk.empty()) {
            chunks.push_back(current_chunk);
        }
        
        return chunks;
    }

private:
    std::vector<std::string> split_paragraphs(const std::string& text) {
        std::vector<std::string> parts;
        // 实现略...
        return parts;
    }
};
```

### 4. RAG 工具集成到 Agent

```cpp
// rag_tool.hpp - RAG 检索工具
class RAGTool : public Tool {
public:
    RAGTool(VectorStore& store, EmbeddingClient& embedding)
        : store_(store), embedding_(embedding) {}
    
    std::string get_name() const override { 
        return "knowledge_search"; 
    }
    
    std::string get_description() const override {
        return "从知识库中检索相关信息";
    }
    
    ToolResult execute(const std::string& query) const override {
        try {
            // 1. 向量化查询
            auto query_vec = embedding_.get_embedding(query);
            
            // 2. 检索相关文档
            auto results = store_.search(query_vec, 3);
            
            // 3. 构建返回结果
            json::object response;
            json::array docs;
            
            for (const auto& result : results) {
                json::object doc;
                doc["id"] = result.doc.id;
                doc["text"] = result.doc.text;
                doc["score"] = result.score;
                docs.push_back(doc);
            }
            
            response["query"] = query;
            response["results"] = docs;
            
            return ToolResult::ok(json::serialize(response));
            
        } catch (const std::exception& e) {
            return ToolResult::fail(std::string("检索失败: ") + e.what());
        }
    }

private:
    VectorStore& store_;
    EmbeddingClient& embedding_;
};

// chat_engine.hpp - 集成 RAG
class ChatEngine {
public:
    std::string process(const std::string& user_input, ChatContext& ctx) {
        // Step 1: 检索相关知识
        auto relevant_docs = knowledge_base_.search(user_input, 3);
        
        // Step 2: 构建增强的 prompt
        std::string context = build_context(relevant_docs);
        std::string augmented_input = context + "\n\n用户问题: " + user_input;
        
        // Step 3: LLM 生成（基于检索到的知识）
        return llm_.complete(augmented_input);
    }

private:
    std::string build_context(const std::vector<SearchResult>& docs) {
        std::stringstream ss;
        ss << "相关背景知识：\n";
        for (size_t i = 0; i < docs.size(); ++i) {
            ss << "[" << (i + 1) << "] " << docs[i].doc.text << "\n";
        }
        ss << "\n请基于以上知识回答问题。";
        return ss.str();
    }
    
    KnowledgeBase knowledge_base_;
    LLMClient llm_;
};
```

---

## 🔧 实战练习

### 练习：构建企业知识库 Agent

**场景：** 公司内部文档问答助手

**要求：**
1. 加载公司文档（PDF/Markdown）
2. 分块并构建向量索引
3. 回答员工关于公司政策的提问

**代码框架：**
```cpp
class EnterpriseAgent {
public:
    void load_documents(const std::string& folder_path) {
        // 遍历文件夹
        // 提取文本
        // 分块、embedding、存入向量库
    }
    
    std::string answer(const std::string& question) {
        // 检索相关知识
        // 构建 prompt
        // 调用 LLM 生成回答
        // 标注引用来源
    }
};
```

---

## 📋 Agent 开发检查清单

- [ ] Embedding 向量维度是否一致？
- [ ] 文档分块大小是否合理？
- [ ] 相似度阈值是否合适？
- [ ] 检索结果是否标注来源？
- [ ] 知识库更新机制？

---

**下一步：** Step 11 多 Agent 协作系统
