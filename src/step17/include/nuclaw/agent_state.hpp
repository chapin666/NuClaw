// ============================================================================
// agent_state.hpp - Agent 完整状态
// ============================================================================

#pragma once
#include "emotion.hpp"
#include "memory.hpp"
#include "relationship.hpp"

namespace nuclaw {

// Agent 完整状态
class AgentState {
public:
    std::string agent_id;
    std::string current_activity = "idle";
    
    EmotionState emotion;
    ShortTermMemory short_term_memory{10};  // 保留最近10条
    RelationshipManager relationships;
    
    void add_memory(const std::string& content, 
                    const std::string& category,
                    const std::string& user_id = "") {
        short_term_memory.add(content, category, user_id);
    }
    
    Relationship& get_relationship(const std::string& user_id) {
        return relationships.get_or_create(user_id);
    }
    
    void update_relationship(const std::string& user_id, 
                             const std::string& user_name,
                             float affinity_delta = 0.5f) {
        relationships.update(user_id, user_name, affinity_delta);
    }
    
    void update_emotion(const std::string& user_message) {
        emotion.update_from_interaction(user_message);
    }
};

} // namespace nuclaw
