// ============================================================================
// vector_store.hpp - 简化版向量存储（内存实现）
// ============================================================================

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
        std::lock_guard<std::mutex> lock(mutex_);
        documents_.push_back(doc);
    }
    
    std::vector<SearchResult> search(const std::vector<float>& query_vec, size_t top_k = 3) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<SearchResult> results;
        
        for (const auto& doc : documents_) {
            float score = cosine_similarity(query_vec, doc.embedding);
            results.push_back({doc, score});
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
    float cosine_similarity(const std::vector<float>& a, const std::vector<float>& b) {
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
