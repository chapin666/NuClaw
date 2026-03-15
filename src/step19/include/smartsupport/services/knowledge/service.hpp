#pragma once

#include "smartsupport/common/types.hpp"
#include <vector>
#include <string>

namespace smartsupport::services::knowledge {

// 知识文档
struct Document {
    std::string id;
    std::string title;
    std::string content;
    std::string doc_type;  // faq/product/policy
    json metadata;
};

// 知识检索服务
class KnowledgeService {
public:
    struct Config {
        std::string vector_db_url;
        std::string embedding_model;
    };
    
    KnowledgeService(const Config& config);
    
    // 索引文档
    void index_document(const std::string& tenant_id,
                        const Document& doc,
                        std::function<void(bool)> callback);
    
    // 检索知识
    std::vector<json> retrieve(const std::string& tenant_id,
                                const std::string& query,
                                size_t top_k = 5);
    
    // 删除文档
    void delete_document(const std::string& tenant_id,
                         const std::string& doc_id);

private:
    Config config_;
};

} // namespace smartsupport::services::knowledge
