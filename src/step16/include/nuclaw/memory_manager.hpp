// memory_manager.hpp - 记忆管理器
#pragma once
#include "agent_state.hpp"
#include <memory>

namespace nuclaw {

// 向量数据库客户端接口（简化版）
class VectorStoreClient {
public:
    virtual ~VectorStoreClient() = default;
    virtual std::vector<float> encode(const std::string& text) = 0;
    virtual void upsert(const std::string& id, 
                       const std::vector<float>& vector,
                       const std::map<std::string, std::string>& metadata) = 0;
    virtual std::vector<std::pair<std::string, float>> search(
        const std::vector<float>& query_vector,
        size_t top_k) = 0;
};

class MemoryManager {
public:
    MemoryManager(std::shared_ptr<VectorStoreClient> vector_store)
        : vector_store_(vector_store) {}
    
    // 添加短期记忆
    void add_short_term(AgentState& state, const std::string& content, 
                       const std::string& category = "event") {
        Memory mem{
            .id = generate_uuid(),
            .content = content,
            .category = category,
            .timestamp = std::chrono::system_clock::now()
        };
        
        state.short_term_memory.push_back(mem);
        
        // 限制容量，重要记忆转入长期记忆
        if (state.short_term_memory.size() > 10) {
            consolidate_memories(state);
        }
    }
    
    // 存储到长期记忆
    void store_long_term(
        const std::string& agent_id,
        const std::string& user_id,
        const Memory& memory) {
        
        // 生成 embedding
        auto embedding = vector_store_->encode(memory.content);
        
        // 存储到向量数据库
        vector_store_->upsert(memory.id, embedding, {
            {"agent_id", agent_id},
            {"user_id", user_id},
            {"category", memory.category},
            {"content", memory.content}
        });
    }
    
    // 检索相关记忆
    std::vector<Memory> retrieve_memories(
        const std::string& agent_id,
        const std::string& user_id,
        const std::string& query,
        size_t limit = 5) {
        
        // 生成查询向量
        auto query_embedding = vector_store_->encode(query);
        
        // 向量搜索
        auto results = vector_store_->search(query_embedding, limit);
        
        // 转换为 Memory 对象
        std::vector<Memory> memories;
        for (const auto& [id, score] : results) {
            // 简化实现，实际应从数据库获取完整信息
            memories.push_back(Memory{
                .id = id,
                .content = "Retrieved memory " + id,
                .category = "recalled"
            });
        }
        
        return memories;
    }
    
    // 记忆巩固
    void consolidate_memories(AgentState& state) {
        for (const auto& mem : state.short_term_memory) {
            // 判断重要性
            if (mem.importance > 7 || mem.category == "preference") {
                store_long_term(state.agent_id, "", mem);
            }
        }
        
        // 清空短期记忆
        state.short_term_memory.clear();
    }

private:
    std::string generate_uuid() {
        // 简化实现
        static std::atomic<int> counter{0};
        return "mem-" + std::to_string(counter++);
    }
    
    std::shared_ptr<VectorStoreClient> vector_store_;
};

} // namespace nuclaw
