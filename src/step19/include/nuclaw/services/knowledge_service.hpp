// ============================================================================
// knowledge_service.hpp - Step 18: 知识库/RAG 服务
// ============================================================================
// 演进说明：
//   复用 Step 10 RAG 向量检索，封装为独立服务
//   
//   Step 10: 实现了 VectorStore 和基础 RAG
//   Step 18: 封装为 KnowledgeService，支持多租户知识库隔离
//
//   演进方式：
//     - 复用 include/nuclaw/vector_store.hpp (Step 10)
//     - 复用 include/nuclaw/document_processor.hpp (Step 10)
//     - 新增租户知识库隔离
// ============================================================================

#pragma once
#include "nuclaw/vector_store.hpp"
#include "nuclaw/document_processor.hpp"
#include <boost/asio.hpp>
#include <memory>
#include <map>

namespace nuclaw {

// 知识库配置
struct KnowledgeBaseConfig {
    std::string embedding_model = "text-embedding-ada-002";
    int top_k = 5;                    // 检索 Top-K 结果
    float similarity_threshold = 0.7f; // 相似度阈值
    size_t max_chunk_size = 500;      // 文档分块大小
};

// 检索结果
struct RetrievalResult {
    std::string content;
    std::string source;
    float score;
};

// KnowledgeService: 向量知识库管理（Step 18 新增）
class KnowledgeService {
public:
    KnowledgeService(const KnowledgeBaseConfig& config)
        : config_(config) {}
    
    // 初始化租户知识库
    void initialize_tenant_kb(const std::string& tenant_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (tenant_stores_.find(tenant_id) == tenant_stores_.end()) {
            tenant_stores_[tenant_id] = std::make_unique<VectorStore>();
            std::cout << "[KnowledgeService] 初始化知识库: " << tenant_id << "\n";
        }
    }
    
    // 添加文档到租户知识库
    void add_document(const std::string& tenant_id,
                      const std::string& doc_id,
                      const std::string& content) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = tenant_stores_.find(tenant_id);
        if (it == tenant_stores_.end()) {
            std::cerr << "[KnowledgeService] 错误: 租户知识库不存在 " << tenant_id << "\n";
            return;
        }
        
        // 文档分块（简化版，实际使用 DocumentProcessor）
        std::vector<std::string> chunks = chunk_document(content);
        
        // 生成向量并存储（简化版，实际调用 Embedding API）
        for (size_t i = 0; i < chunks.size(); ++i) {
            std::vector<float> embedding = generate_embedding(chunks[i]);
            Document doc;
            doc.id = doc_id + "_chunk_" + std::to_string(i);
            doc.text = chunks[i];
            doc.embedding = embedding;
            it->second->add_document(doc);
        }
        
        std::cout << "[KnowledgeService] 添加文档: " << doc_id 
                  << " (" << chunks.size() << " 块) 到租户 " << tenant_id << "\n";
    }
    
    // 检索相关知识（RAG 核心）
    std::vector<RetrievalResult> retrieve(const std::string& tenant_id,
                                           const std::string& query,
                                           int top_k = -1) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = tenant_stores_.find(tenant_id);
        if (it == tenant_stores_.end()) {
            return {};  // 租户知识库不存在
        }
        
        if (top_k < 0) top_k = config_.top_k;
        
        // 生成查询向量（简化版）
        std::vector<float> query_embedding = generate_embedding(query);
        
        // 向量检索
        auto results = it->second->search(query_embedding, top_k);
        
        // 格式化结果
        std::vector<RetrievalResult> formatted;
        for (const auto& r : results) {
            if (r.score >= config_.similarity_threshold) {
                formatted.push_back({r.doc.text, r.doc.id, r.score});
            }
        }
        
        return formatted;
    }
    
    // 获取知识库统计
    struct KBStats {
        std::string tenant_id;
        size_t document_count;
        size_t chunk_count;
    };
    
    std::vector<KBStats> get_stats() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<KBStats> stats;
        for (const auto& [tenant_id, store] : tenant_stores_) {
            stats.push_back({tenant_id, store->size(), store->size()});
        }
        return stats;
    }

private:
    KnowledgeBaseConfig config_;
    std::map<std::string, std::unique_ptr<VectorStore>> tenant_stores_;
    mutable std::mutex mutex_;
    
    // 文档分块（简化实现）
    std::vector<std::string> chunk_document(const std::string& content) {
        std::vector<std::string> chunks;
        size_t pos = 0;
        while (pos < content.length()) {
            size_t end = std::min(pos + config_.max_chunk_size, content.length());
            chunks.push_back(content.substr(pos, end - pos));
            pos = end;
        }
        if (chunks.empty()) chunks.push_back(content);
        return chunks;
    }
    
    // 生成向量（简化版，实际调用 Embedding API）
    std::vector<float> generate_embedding(const std::string& text) {
        // 简化：使用文本哈希模拟向量
        std::vector<float> vec(128, 0.0f);
        for (size_t i = 0; i < text.length() && i < 128; ++i) {
            vec[i] = static_cast<float>(text[i]) / 255.0f;
        }
        return vec;
    }
};

} // namespace nuclaw
