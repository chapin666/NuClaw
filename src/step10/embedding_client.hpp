// ============================================================================
// embedding_client.hpp - Embedding 客户端（简化版）
// ============================================================================
// Step 10: 提供文本向量化的接口
// 实际项目中应调用 OpenAI API 或本地模型
// ============================================================================

#pragma once

#include <vector>
#include <string>
#include <cmath>
#include <sstream>
#include <algorithm>

class EmbeddingClient {
public:
    // 检查是否配置了真实 Embedding API
    bool is_configured() const {
        // 简化版：始终使用 Mock
        // 实际应检查 OPENAI_API_KEY 等环境变量
        return false;
    }
    
    // 获取文本的 embedding 向量
    // 简化实现：使用字符哈希生成伪向量
    // 实际应调用 OpenAI API: text-embedding-3-small/ada-002
    std::vector<float> get_embedding(const std::string& text) {
        // 生成固定维度的向量（简化版）
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
        for (float v : vec) {
            norm += v * v;
        }
        norm = std::sqrt(norm);
        
        if (norm > 0.0f) {
            for (float& v : vec) {
                v /= norm;
            }
        }
    }
};
