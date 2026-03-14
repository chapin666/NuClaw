// include/nuclaw/agent_state.hpp
#pragma once
#include <string>
#include <vector>
#include <map>
#include <chrono>

namespace nuclaw {

// 情感状态
struct EmotionState {
    float happiness = 0.0f;
    float energy = 5.0f;
    float trust = 5.0f;
    
    std::string describe() const {
        if (happiness > 5) return "开心";
        if (happiness < -5) return "难过";
        if (energy < 3) return "疲惫";
        return "平静";
    }
};

// 记忆条目
struct Memory {
    std::string content;
    std::string category;
    float importance = 5.0f;
    std::chrono::system_clock::time_point timestamp;
};

// 与用户的关系
struct Relationship {
    std::string user_id;
    std::string user_name;
    float familiarity = 0.0f;
    float affinity = 5.0f;
    int interaction_count = 0;
};

// Agent 状态
class AgentState {
public:
    std::string agent_id;
    std::string current_activity = "idle";
    EmotionState emotion;
    
    std::vector<Memory> short_term_memory;
    std::map<std::string, Relationship> relationships;
    
    void add_memory(const std::string& content, 
                    const std::string& category = "event",
                    float importance = 5.0f) {
        Memory mem;
        mem.content = content;
        mem.category = category;
        mem.importance = importance;
        mem.timestamp = std::chrono::system_clock::now();
        
        short_term_memory.push_back(mem);
        
        // 只保留最近 10 条
        if (short_term_memory.size() > 10) {
            short_term_memory.erase(short_term_memory.begin());
        }
    }
    
    Relationship& get_or_create_relationship(const std::string& user_id) {
        if (relationships.count(user_id) == 0) {
            relationships[user_id] = Relationship();
            relationships[user_id].user_id = user_id;
        }
        return relationships[user_id];
    }
    
    void update_relationship(const std::string& user_id, 
                             const std::string& user_name,
                             float affinity_delta) {
        auto& rel = get_or_create_relationship(user_id);
        rel.user_name = user_name;
        rel.affinity += affinity_delta;
        rel.interaction_count++;
        rel.familiarity = std::min(10.0f, rel.familiarity + 0.5f);
    }
};

} // namespace nuclaw