# Step 10: RAG 检索 - 向量数据库与 Embedding

> 目标：实现知识检索增强生成，解决 LLM 知识局限
> 
> 难度：⭐⭐⭐⭐⭐ (困难)
> 
> 代码量：约 800 行
> 
> 预计学习时间：5-6 小时

---

## 📚 前置知识

### LLM 的局限性

**问题 1：知识截止**
```
用户：2024 年最新的 AI 发展趋势是什么？
LLM：抱歉，我的知识截止到 2023 年初...

原因：训练数据有截止日期，无法获取最新信息
```

**问题 2：幻觉（Hallucination）**
```
用户：介绍一下张明文教授的理论
LLM：张明文教授提出了...（编造的内容）

原因：LLM 会生成听起来合理但实际不存在的内容
```

**问题 3：私有数据访问**
```
用户：我们公司的内部流程是怎样的？
LLM：我不知道你公司的内部信息

原因：训练数据不包含私有/内部文档
```

### 什么是 RAG？

**RAG = Retrieval-Augmented Generation（检索增强生成）**

**核心思想：** 在生成回答之前，先从知识库中检索相关信息，然后把这些信息作为上下文提供给 LLM。

**工作流程：**
```
用户提问 → 检索相关知识 → 构建增强提示 → LLM 生成回答
                ↑
         向量数据库（知识库）
```

**类比：**
```
闭卷考试 vs 开卷考试

闭卷考试（纯 LLM）：
- 只能依靠记忆
- 可能记错或记不清

开卷考试（RAG）：
- 可以查资料
- 基于资料回答，更准确
```

### Embedding 基础

**什么是 Embedding？**

Embedding 是将离散的对象（如词语、句子、图片）映射到连续向量空间的技术。

**关键特性：**
- **语义相似 = 向量相近**
- "猫" 和 "狗" 的向量距离 < "猫" 和 "汽车"

**可视化理解：**
```
2D 向量空间（简化）：

      猫 🐱
     /    \
    /      \
  狗 🐶    老虎 🐯
  |         |
  |         |
 汽车 🚗   飞机 ✈️

猫和狗的距离近（都是宠物）
猫和汽车的距离远（语义无关）
```

**实际应用：**
- 搜索：查询向量和文档向量求相似度
- 推荐：找相似用户或商品
- 分类：向量聚类

### 向量相似度度量

**余弦相似度（最常用）：**
```
cos(θ) = (A · B) / (||A|| × ||B||)

取值范围：-1 到 1
- 1：方向完全相同（语义完全相同）
- 0：正交（无关）
- -1：方向相反（语义相反）
```

**欧氏距离：**
```
d(A, B) = √(Σ(Ai - Bi)²)

直接衡量向量间的距离
```

**选择建议：**
- 余弦相似度：关注方向（语义相似）
- 欧氏距离：关注绝对位置

---

## 第一步：RAG 系统架构

### 整体架构

```
┌─────────────────────────────────────────────────────────────┐
│                      RAG 系统架构                            │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌─────────────────────────────────────────────────────┐   │
│  │                  数据准备（离线）                     │   │
│  │                                                      │   │
│  │   原始文档 → 文本提取 → 分块 → Embedding → 存储     │   │
│  │   (PDF/Word)   (文本)    (Chunk)  (向量)   (向量DB)  │   │
│  └─────────────────────────────────────────────────────┘   │
│                              ↓                              │
│  ┌─────────────────────────────────────────────────────┐   │
│  │                  查询回答（在线）                     │   │
│  │                                                      │   │
│  │   用户问题 → Embedding → 相似度检索 → 构建 Prompt   │   │
│  │      ↓                                    ↓          │   │
│  │   问题向量 ←───────────────────────── 相关文档       │   │
│  │                              ↓                        │   │
│  │                         LLM 生成回答                 │   │
│  └─────────────────────────────────────────────────────┘   │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 核心组件

| 组件 | 职责 | 关键技术 |
|:---|:---|:---|
| **文档处理器** | 提取和分割文本 | PDF 解析、文本分块 |
| **Embedding 服务** | 文本向量化 | OpenAI API、BERT |
| **向量数据库** | 存储和检索向量 | Milvus、Chroma |
| **检索器** | 相似度搜索 | ANN 算法 |
| **Prompt 构建器** | 组装上下文 | Prompt 工程 |

---

## 第二步：Embedding 服务实现

### 使用 OpenAI Embedding API

```cpp
// embedding_client.hpp
#pragma once
#include <string>
#include <vector>
#include <math>
#include <sstream>
#include <algorithm>

class EmbeddingClient {
public:
    // 获取文本的 embedding 向量
    std::vector<float> get_embedding(const std::string& text) {
        // 简化版：使用字符哈希生成伪向量
        // 实际项目：调用 OpenAI API
        // return call_openai_api(text);
        
        const size_t dimension = 128;
        std::vector<float> vec(dimension, 0.0f);
        
        // 基于字符哈希生成向量（仅用于演示）
        for (size_t i = 0; i < text.length(); ++i) {
            size_t idx = i % dimension;
            vec[idx] += static_cast<float>(text[i]) / 255.0f;
        }
        
        // 归一化
        normalize(vec);
        
        return vec;
    }
    
    // 批量获取 embedding
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

---

## 第三步：向量存储实现

### 💡 理论知识：向量数据库的原理

**为什么需要专门的向量数据库？**

传统数据库（MySQL、PostgreSQL）擅长精确匹配和范围查询，但向量搜索需要计算相似度，这是完全不同的计算模式：

```
传统查询：WHERE id = 123          → B+树索引，O(log n)
向量查询：ORDER BY similarity DESC → 全表扫描，O(n)
```

**优化思路：**
1. **近似最近邻（ANN）**：牺牲少量精度换取速度（如 HNSW、IVF）
2. **量化压缩**：降低向量维度，减少内存占用
3. **分片并行**：多线程并行计算相似度

**本教程采用简化方案（线性扫描）：**
- 适合文档数量 < 10万 的场景
- 实现简单，易于理解
- 生产环境建议用 Milvus、Pinecone、Qdrant

### 内存向量数据库

```cpp
// vector_store.hpp
#pragma once
#include <string>
#include <vector>
#include <math>
#include <algorithm>
#include <mutex>

struct Document {
    std::string id;
    std::string text;
    std::vector<float> embedding;
};

struct SearchResult {
    Document doc;
    float score;
};

class VectorStore {
public:
    void add_document(const Document& doc) {
        // 🔒 线程安全：保护共享数据
        // 原理：RAII 锁，构造函数加锁，析构函数解锁
        // 优势：异常安全，不会死锁
        std::lock_guard<std::mutex> lock(mutex_);
        documents_.push_back(doc);
    }
    
    // 相似度搜索
    std::vector<SearchResult> search(
        const std::vector<float>& query_vec, 
        size_t top_k = 3) {
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<SearchResult> results;
        // 📊 时间复杂度：O(n)，n 为文档数量
        // 每个文档计算一次余弦相似度
        for (const auto& doc : documents_) {
            float score = cosine_similarity(query_vec, doc.embedding);
            results.push_back({doc, score});
        }
        
        // ⚡ 排序优化：std::sort 使用 introsort
        // 时间复杂度：O(n log n)，空间复杂度：O(log n)
        std::sort(results.begin(), results.end(),
            [](const SearchResult& a, const SearchResult& b) {
                return a.score > b.score;
            });
        
        // ✂️ 只保留 top_k 结果
        if (results.size() > top_k) {
            results.resize(top_k);
        }
        return results;
    }
        std::sort(results.begin(), results.end(),
            [](const SearchResult& a, const SearchResult& b) {
                return a.score > b.score;
            });
        
        if (results.size() > top_k) {
            results.resize(top_k);
        }
        return results;
    }
    
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return documents_.size();
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

---

## 第四步：文档处理与分块

### 💡 理论知识：为什么需要文本分块？

**LLM 的上下文窗口限制：**
- GPT-3.5: 16K tokens (~12K 汉字)
- GPT-4: 128K tokens (~96K 汉字)
- Claude 3: 200K tokens

**问题：长文档无法一次性放入上下文**
```
完整文档：10万字的技术手册
上下文限制：只能放 1 万字
解决方案：切成 10 块，每块 1 万字
```

**分块策略对比：**

| 策略 | 优点 | 缺点 | 适用场景 |
|:---|:---|:---|:---|
| **固定长度** | 实现简单 | 可能切断句子 | 日志、代码 |
| **段落分割** | 语义完整 | 段落长度不均 | 文章、报告 |
| **语义分块** | 主题相关 | 需要额外模型 | 复杂文档 |
| **递归分块** | 多层次 | 实现复杂 | 结构化文档 |

**重叠（Overlap）的作用：**
```
块1: [AAAAAAAAAA|BBBBBB]
块2:           [BBBBBB|CCCCCCCCC]
                ↑ 重叠区域

避免边界信息丢失：
- 查询 "BBBB" 时，无论它靠近块1还是块2的边界，都能被检索到
- 重叠越多，召回率越高，但存储成本也越高
```

### 文本分块策略

```cpp
// document_processor.hpp
#pragma once
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>

class DocumentProcessor {
public:
    struct ChunkConfig {
        size_t chunk_size = 500;        // 📏 每块字符数
                                        // 权衡：太大超出限制，太小丢失上下文
        
        size_t chunk_overlap = 50;      // 🔁 重叠字符数
                                        // 经验：chunk_size 的 10-20%
        
        std::string separator = "\n\n"; // ✂️ 优先分隔符
                                        // 段落边界 > 句子边界 > 字符边界
    };
    
    // 将长文本分块
    std::vector<std::string> split_text(const std::string& text,
                                          const ChunkConfig& config = {}) {
        std::vector<std::string> chunks;
        
        // 📖 语义边界优先：按段落分割
        // 原理：段落通常是语义完整的单元
        std::vector<std::string> paragraphs = split_by_separator(text, config.separator);
        
        std::string current_chunk;
        for (const auto& para : paragraphs) {
            // ⚠️ 检查是否超出块大小限制
            if (current_chunk.length() + para.length() > config.chunk_size) {
                if (!current_chunk.empty()) {
                    chunks.push_back(current_chunk);
                }
                
                // 🔁 保留重叠部分，避免边界信息丢失
                // 策略：取上一块末尾的 overlap 个字符
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
        
        if (!current_chunk.empty()) {
            chunks.push_back(current_chunk);
        }
        
        return chunks;
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

## 第五步：RAG 工具集成

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
        return "从知识库中检索相关信息";
    }
    
    ToolResult execute(const std::string& query) const override {
        try {
            // 1. 向量化查询
            std::vector<float> query_vec = embedding_.get_embedding(query);
            
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

## 第六步：知识库构建

### 知识库管理

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
            doc.embedding = embeddings[i];
            
            store_.add_document(doc);
        }
        
        std::cout << "[+] 已添加文档: " << id 
                  << " (" << chunks.size() << " 块)" << std::endl;
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

## 本节总结

### 核心概念

1. **RAG**：检索增强生成，解决 LLM 知识局限
2. **Embedding**：将文本转换为语义向量
3. **向量相似度**：余弦相似度衡量语义相近程度
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
4. **上下文压缩**：防止超出 LLM 上下文限制

---

## 📝 课后练习

### 练习 1：实现重排序
添加一个重排序模型，对初筛结果进行二次排序。

### 练习 2：混合检索
结合 BM25 关键词检索和向量检索。

### 练习 3：增量更新
实现知识库的增量更新（不重建整个库）。

### 思考题
1. 为什么 Embedding 比关键词搜索更适合语义检索？
2. RAG 会引入哪些新的问题？如何解决？
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
