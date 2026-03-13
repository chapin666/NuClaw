# Step 10: RAG 检索 - 向量数据库与 Embedding

> 目标：理解 RAG 原理，实现知识检索增强生成
> 
> 难度：⭐⭐⭐⭐⭐ (困难)
> 
> 代码量：约 800 行
> 
> 预计学习时间：5-6 小时

---

## 📚 前置知识

### 什么是 RAG？

**RAG（Retrieval-Augmented Generation，检索增强生成）**

**核心思想：**
在生成回答之前，先从知识库中检索相关信息，然后将检索结果作为上下文输入给 LLM。

**为什么需要 RAG？**

**LLM 的局限性：**
1. **知识截止**：训练数据有截止日期，不知道最新信息
2. **幻觉问题**：会编造不存在的事实
3. **无法访问私有数据**：不能访问企业内部文档

**RAG 的解决方案：**
```
用户问题 → 检索相关知识 → LLM（结合知识生成回答）
                ↑
         向量数据库
```

### RAG 工作流程

```
┌─────────────────────────────────────────────────────────────┐
│                      RAG 工作流程                            │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  1. 知识入库（离线）                                         │
│     ┌──────────┐    ┌──────────┐    ┌──────────────┐       │
│     │ 原始文档 │ →  │ 文本分块 │ →  │ Embedding    │       │
│     │ (PDF/MD) │    │ (Chunk)  │    │ (向量化)      │       │
│     └──────────┘    └──────────┘    └──────┬───────┘       │
│                                            ↓                │
│                                    ┌──────────────┐        │
│                                    │ 向量数据库    │        │
│                                    │ (Vector DB)  │        │
│                                    └──────────────┘        │
│                                                             │
│  2. 查询回答（在线）                                         │
│     ┌──────────┐    ┌──────────┐    ┌──────────────┐       │
│     │ 用户问题 │ →  │ Embedding│ →  │ 相似度检索   │       │
│     │          │    │ (向量化)  │    │ (Top-K)      │       │
│     └──────────┘    └──────────┘    └──────┬───────┘       │
│                                            ↓                │
│     ┌──────────┐    ┌──────────┐    ┌──────────────┐       │
│     │ 生成回答 │ ←  │   LLM    │ ←  │ 检索结果     │       │
│     │          │    │          │    │ (上下文)     │       │
│     └──────────┘    └──────────┘    └──────────────┘       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 核心概念

#### 1. Embedding（嵌入）

**定义：** 将文本转换为高维向量的技术。

**类比：**
```
文字 → Embedding → 向量
"猫"  → 模型     → [0.2, -0.5, 0.8, ...] (768维)
"狗"  → 模型     → [0.3, -0.4, 0.7, ...] (768维)
```

**特点：**
- 语义相似的文本，向量距离近
- "猫" 和 "狗" 的向量距离，比 "猫" 和 "汽车" 近

#### 2. 向量相似度

**余弦相似度：**
```
cos(θ) = (A · B) / (||A|| × ||B||)

取值范围：-1 到 1
- 1：完全相同方向（语义相似）
- 0：正交（无关）
- -1：相反方向（语义相反）
```

#### 3. 向量数据库

**专门存储和检索向量的数据库：**
- **Milvus**：开源，企业级
- **Pinecone**：托管服务
- **Chroma**：轻量级，适合本地
- **pgvector**：PostgreSQL 扩展

**核心操作：**
- `add(vector, metadata)` - 添加向量
- `search(query_vector, top_k)` - 相似度检索

---

## 第一步：Embedding 服务

### 使用 OpenAI Embedding API

```cpp
// embedding_client.hpp
#pragma once
#include <string>
#include <vector>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/json.hpp>

namespace json = boost::json;

class EmbeddingClient {
public:
    EmbeddingClient(const std::string& api_key) 
        : api_key_(api_key) {}
    
    // 获取文本的 embedding 向量
    std::vector<float> get_embedding(const std::string& text) {
        // 构建请求
        json::object request;
        request["input"] = text;
        request["model"] = "text-embedding-3-small";  // 或 ada-002
        
        // 发送 HTTP POST
        std::string response = http_post(
            "https://api.openai.com/v1/embeddings",
            json::serialize(request)
        );
        
        // 解析响应
        json::value result = json::parse(response);
        auto embedding_array = result.at("data").at(0).at("embedding").as_array();
        
        // 转换为 vector<float>
        std::vector<float> vec;
        for (const auto& v : embedding_array) {
            vec.push_back(static_cast<float>(v.as_double()));
        }
        
        return vec;
    }
    
    // 批量获取 embedding（更高效）
    std::vector<std::vector<float>> get_embeddings(
        const std::vector<std::string>& texts) {
        // 批量请求实现...
    }

private:
    std::string http_post(const std::string& url, const std::string& body) {
        // 使用 Boost.Beast 发送 POST 请求
        // ... 实现略
    }
    
    std::string api_key_;
};
```

### 本地 Embedding（轻量级方案）

```cpp
// local_embedding.hpp
// 使用轻量级模型（如 sentence-transformers 的 C++ 实现）
// 或者简单的 TF-IDF/BM25 作为简化方案

class LocalEmbedding {
public:
    // 简化版：使用 TF-IDF
    std::vector<float> embed(const std::string& text) {
        // 分词
        auto words = tokenize(text);
        
        // 计算词频
        std::map<std::string, float> tf;
        for (const auto& word : words) {
            tf[word]++;
        }
        
        // 归一化
        std::vector<float> vec(vocab_size_, 0.0f);
        for (const auto& [word, freq] : tf) {
            size_t idx = get_word_index(word);
            if (idx < vocab_size_) {
                vec[idx] = freq / words.size();
            }
        }
        
        return vec;
    }

private:
    static constexpr size_t vocab_size_ = 10000;
    
    std::vector<std::string> tokenize(const std::string& text) {
        // 简单分词：按空格和标点分割
        std::vector<std::string> tokens;
        // ... 实现略
        return tokens;
    }
    
    size_t get_word_index(const std::string& word) {
        // 哈希到固定大小
        return std::hash<std::string>{}(word) % vocab_size_;
    }
};
```

---

## 第二步：内存向量数据库

### 简化版向量存储

```cpp
// vector_store.hpp
#pragma once
#include <vector>
#include <string>
#include <math>
#include <algorithm>
#include <mutex>

struct Document {
    std::string id;
    std::string text;
    std::string metadata;
    std::vector<float> embedding;
};

struct SearchResult {
    Document doc;
    float score;  // 相似度分数
};

class VectorStore {
public:
    // 添加文档
    void add_document(const Document& doc) {
        std::lock_guard<std::mutex> lock(mutex_);
        documents_.push_back(doc);
    }
    
    // 批量添加
    void add_documents(const std::vector<Document>& docs) {
        std::lock_guard<std::mutex> lock(mutex_);
        documents_.insert(documents_.end(), docs.begin(), docs.end());
    }
    
    // 相似度搜索
    std::vector<SearchResult> search(
        const std::vector<float>& query_embedding,
        size_t top_k = 3) {
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<SearchResult> results;
        
        for (const auto& doc : documents_) {
            float score = cosine_similarity(query_embedding, doc.embedding);
            results.push_back({doc, score});
        }
        
        // 按相似度排序
        std::sort(results.begin(), results.end(),
            [](const SearchResult& a, const SearchResult& b) {
                return a.score > b.score;
            });
        
        // 返回 Top-K
        if (results.size() > top_k) {
            results.resize(top_k);
        }
        
        return results;
    }
    
    // 删除文档
    void delete_document(const std::string& id) {
        std::lock_guard<std::mutex> lock(mutex_);
        documents_.erase(
            std::remove_if(documents_.begin(), documents_.end(),
                [&id](const Document& doc) { return doc.id == id; }),
            documents_.end()
        );
    }
    
    // 获取文档数量
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return documents_.size();
    }

private:
    // 余弦相似度计算
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

---

## 第三步：文档处理

### 文本分块（Chunking）

```cpp
// document_processor.hpp
#pragma once
#include <vector>
#include <string>
#include <sstream>

class DocumentProcessor {
public:
    // 分块参数
    struct ChunkConfig {
        size_t chunk_size = 500;        // 每块字符数
        size_t chunk_overlap = 50;      // 重叠字符数
        std::string separator = "\n\n"; // 优先分隔符
    };
    
    // 将长文本分块
    std::vector<std::string> split_text(const std::string& text,
                                          const ChunkConfig& config = {}) {
        std::vector<std::string> chunks;
        
        // 优先按段落分割
        std::vector<std::string> paragraphs = split_by_separator(text, config.separator);
        
        std::string current_chunk;
        for (const auto& para : paragraphs) {
            if (current_chunk.length() + para.length() > config.chunk_size) {
                // 保存当前块
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
                current_chunk += config.separator;
            }
            current_chunk += para;
        }
        
        // 添加最后一块
        if (!current_chunk.empty()) {
            chunks.push_back(current_chunk);
        }
        
        return chunks;
    }
    
    // 从文件加载文档
    std::string load_file(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            throw std::runtime_error("无法打开文件: " + path);
        }
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

private:
    std::vector<std::string> split_by_separator(const std::string& text,
                                                  const std::string& sep) {
        std::vector<std::string> parts;
        size_t start = 0;
        size_t end = text.find(sep);
        
        while (end != std::string::npos) {
            parts.push_back(text.substr(start, end - start));
            start = end + sep.length();
            end = text.find(sep, start);
        }
        
        parts.push_back(text.substr(start));
        return parts;
    }
};
```

---

## 第四步：RAG 工具实现

### RAG 检索工具

```cpp
// rag_tool.hpp
#pragma once
#include "tool.hpp"
#include "vector_store.hpp"
#include "embedding_client.hpp"
#include <sstream>

class RAGTool : public Tool {
public:
    RAGTool(VectorStore& store, EmbeddingClient& embedding)
        : store_(store), embedding_(embedding) {}
    
    std::string get_name() const override { return "knowledge_search"; }
    
    std::string get_description() const override {
        return R"({
            "name": "knowledge_search",
            "description": "从知识库中检索相关信息",
            "parameters": {
                "type": "object",
                "properties": {
                    "query": {
                        "type": "string",
                        "description": "搜索查询"
                    }
                },
                "required": ["query"]
            }
        })";
    }
    
    ToolResult execute(const std::string& arguments) const override {
        // 解析查询
        std::string query = arguments;  // 简化处理
        
        try {
            // 1. 获取查询的 embedding
            std::vector<float> query_vec = embedding_.get_embedding(query);
            
            // 2. 检索相似文档
            auto results = store_.search(query_vec, 3);
            
            // 3. 构建返回结果
            json::object response;
            json::array docs;
            
            for (const auto& result : results) {
                json::object doc;
                doc["id"] = result.doc.id;
                doc["text"] = result.doc.text;
                doc["score"] = result.score;
                doc["metadata"] = result.doc.metadata;
                docs.push_back(doc);
            }
            
            response["query"] = query;
            response["results"] = docs;
            response["total"] = docs.size();
            
            return ToolResult::ok(json::serialize(response));
            
        } catch (const std::exception& e) {
            return ToolResult::fail(std::string("检索失败: ") + e.what());
        }
    }

private:
    VectorStore& store_;
    EmbeddingClient& embedding_;
};
```

---

## 第五步：知识库构建

### 初始化知识库

```cpp
// knowledge_base.hpp
#pragma once
#include "vector_store.hpp"
#include "embedding_client.hpp"
#include "document_processor.hpp"

class KnowledgeBase {
public:
    KnowledgeBase(VectorStore& store, EmbeddingClient& embedding)
        : store_(store), embedding_(embedding) {}
    
    // 添加文档到知识库
    void add_document(const std::string& id, 
                      const std::string& text,
                      const std::string& metadata = "") {
        // 1. 分块
        auto chunks = processor_.split_text(text);
        
        // 2. 获取 embedding
        auto embeddings = embedding_.get_embeddings(chunks);
        
        // 3. 添加到向量库
        for (size_t i = 0; i < chunks.size(); ++i) {
            Document doc;
            doc.id = id + "_" + std::to_string(i);
            doc.text = chunks[i];
            doc.metadata = metadata;
            doc.embedding = embeddings[i];
            
            store_.add_document(doc);
        }
        
        std::cout << "[+] 已添加文档: " << id 
                  << " (" << chunks.size() << " 块)" << std::endl;
    }
    
    // 从文件添加
    void add_file(const std::string& path, const std::string& metadata = "") {
        std::string text = processor_.load_file(path);
        add_document(path, text, metadata);
    }
    
    // 搜索
    std::vector<SearchResult> search(const std::string& query, size_t top_k = 3) {
        auto query_vec = embedding_.get_embedding(query);
        return store_.search(query_vec, top_k);
    }

private:
    VectorStore& store_;
    EmbeddingClient& embedding_;
    DocumentProcessor processor_;
};
```

---

## 第六步：集成到 Agent

### RAG 增强的 ChatEngine

```cpp
// chat_engine.hpp
#pragma once
#include "tool_registry.hpp"
#include "tool_executor.hpp"
#include "llm_client.hpp"
#include "knowledge_base.hpp"

class ChatEngine {
public:
    ChatEngine(ToolExecutor& executor, KnowledgeBase& kb)
        : executor_(executor), kb_(kb) {}
    
    std::string process(const std::string& user_input, ChatContext& ctx) {
        // Step 1: 先检索相关知识
        auto relevant_docs = kb_.search(user_input, 3);
        
        // Step 2: 构建增强的 prompt
        std::string context = build_context(relevant_docs);
        std::string augmented_input = context + "\n\n用户问题: " + user_input;
        
        // Step 3: 判断是否需要工具
        if (llm_.needs_tool(augmented_input)) {
            ToolCall call = llm_.parse_tool_call(augmented_input);
            ToolResult result = executor_.execute_sync(call);
            return llm_.generate_response(augmented_input, result, call);
        }
        
        // Step 4: 直接回答（基于检索到的知识）
        return llm_.direct_reply(augmented_input);
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
    
    ToolExecutor& executor_;
    KnowledgeBase& kb_;
    LLMClient llm_;
};
```

---

## 本节总结

### 核心概念

1. **RAG**：检索增强生成，解决 LLM 知识截止和幻觉问题
2. **Embedding**：将文本转换为向量，捕捉语义信息
3. **向量相似度**：余弦相似度衡量语义相似性
4. **文本分块**：长文档切分成小块，便于检索

### 代码演进

```
Step 9: 工具注册表 (750行)
   ↓ + RAG 系统
Step 10: 800行
   + embedding_client.hpp: Embedding API 调用
   + vector_store.hpp: 向量数据库存储
   + document_processor.hpp: 文档分块处理
   + rag_tool.hpp: RAG 检索工具
   + knowledge_base.hpp: 知识库管理
```

### RAG 优化技巧

1. **重排序（Reranking）**：初筛后用小模型精排
2. **查询扩展**：用 LLM 生成同义查询
3. **混合检索**：向量检索 + 关键词检索
4. **元数据过滤**：先按标签过滤，再向量检索

---

## 📝 课后练习

### 练习 1：实现重排序
添加一个重排序模型，对初筛结果进行二次排序：
```cpp
class Reranker {
    std::vector<SearchResult> rerank(
        const std::string& query,
        const std::vector<SearchResult>& candidates);
};
```

### 练习 2：混合检索
结合 BM25 关键词检索和向量检索：
```cpp
auto vec_results = vector_search(query);
auto bm25_results = bm25_search(query);
auto merged = merge_results(vec_results, bm25_results);
```

### 练习 3：增量更新
实现知识库的增量更新（不重建整个库）：
```cpp
void update_document(const std::string& id, const std::string& new_text);
```

### 思考题
1. 为什么需要文本分块？不分块有什么问题？
2. Embedding 模型的选择对 RAG 效果有什么影响？
3. 如何评估 RAG 系统的效果？

---

## 📖 扩展阅读

### 向量数据库对比

| 数据库 | 特点 | 适用场景 |
|:---|:---|:---|
| **Milvus** | 开源、分布式 | 大规模生产环境 |
| **Pinecone** | 托管、易用 | 快速启动 |
| **Chroma** | 轻量、本地 | 开发测试 |
| **pgvector** | PostgreSQL 扩展 | 已有 PG 基础设施 |

### RAG 进阶技术

- **GraphRAG**：结合知识图谱的 RAG
- **Self-RAG**：让模型自己判断是否需要检索
- **Corrective RAG**：自动纠正检索结果

---

**恭喜！** 你的 Agent 现在具备了知识检索能力。下一章我们将实现多 Agent 协作系统。
