// ============================================================================
// main.cpp - Step 10: RAG 检索演示
// ============================================================================

#include "vector_store.hpp"
#include "embedding_client.hpp"
#include "document_processor.hpp"
#include <iostream>
#include <iomanip>

int main() {
    std::cout << "========================================\n";
    std::cout << "   NuClaw Step 10: RAG 检索\n";
    std::cout << "========================================\n\n";
    
    // 1. 初始化组件
    VectorStore store;
    EmbeddingClient embedding;
    DocumentProcessor processor;
    
    // 2. 准备知识库文档
    std::vector<std::pair<std::string, std::string>> knowledge = {
        {"doc1", "北京是中国的首都，人口约 2100 万。著名景点有故宫、长城、天安门等。"},
        {"doc2", "上海是中国最大的城市，人口约 2400 万。著名景点有外滩、东方明珠、豫园等。"},
        {"doc3", "Python 是一种流行的编程语言，由 Guido van Rossum 于 1991 年创建。"},
        {"doc4", "C++ 是一种高性能编程语言，支持面向对象编程和底层内存操作。"},
        {"doc5", "机器学习是人工智能的一个分支，通过数据训练模型来完成任务。"}
    };
    
    std::cout << "[1] 构建知识库...\n";
    
    // 3. 文档分块并添加到向量库
    for (const auto& [id, text] : knowledge) {
        auto chunks = processor.split_text(text);
        
        for (size_t i = 0; i < chunks.size(); ++i) {
            auto vec = embedding.get_embedding(chunks[i]);
            
            Document doc;
            doc.id = id + "_chunk" + std::to_string(i);
            doc.text = chunks[i];
            doc.embedding = vec;
            
            store.add_document(doc);
        }
    }
    
    std::cout << "    已添加 " << store.size() << " 个文档块\n\n";
    
    // 4. 模拟查询
    std::vector<std::string> queries = {
        "北京有什么景点？",
        "Python 是什么？",
        "人工智能相关"
    };
    
    std::cout << "[2] RAG 检索演示\n";
    std::cout << "----------------------------------------\n";
    
    for (const auto& query : queries) {
        std::cout << "\n🔍 查询: " << query << "\n";
        
        // 向量化查询
        auto query_vec = embedding.get_embedding(query);
        
        // 检索相关文档
        auto results = store.search(query_vec, 2);
        
        std::cout << "📄 检索结果:\n";
        for (size_t i = 0; i < results.size(); ++i) {
            std::cout << "   [" << (i + 1) << "] 相似度: " << std::fixed << std::setprecision(4)
                      << results[i].score << "\n";
            std::cout << "       内容: " << results[i].doc.text.substr(0, 50)
                      << (results[i].doc.text.length() > 50 ? "..." : "") << "\n";
        }
    }
    
    std::cout << "\n========================================\n";
    std::cout << "RAG 演示完成！\n";
    std::cout << "========================================\n";
    
    return 0;
}
