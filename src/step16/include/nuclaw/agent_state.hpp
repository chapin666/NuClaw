// agent_state.hpp - Agent 状态与记忆系统
#pragma once
#include <chrono>
#include <string>
#include <vector>
#include <map>
#include <nlohmann/json.hpp>

namespace nuclaw {

// 情感状态 (-10 ~ +10)
struct EmotionState {
    float happiness = 0.0f;     // 开心程度
    float energy = 5.0f;        // 精力水平 (0-10)
    float trust = 5.0f;         // 对当前用户的信任度
    float interest = 5.0f;      // 对当前话题的兴趣度
    
    void decay(float rate = 0.95f) {
        happiness *= rate;
        energy = std::min(10.0f, energy + 0.1f);  // 精力自然恢复
    }
    
    std::string describe() const {
        if (happiness > 5) return "开心";
        if (happiness < -5) return "难过";
        if (energy < 3) return "疲惫";
        return "平静";
    }
};

// 记忆条目
struct Memory {
    std::string id;
    std::string content;
    std::string category;       // "fact", "event", "preference", "emotion"
    float importance = 5.0f;    // 重要性 0-10
    std::chrono::system_clock::time_point timestamp;
    std::map<std::string, std::string> metadata;
};

// 与特定用户的关系
struct Relationship {
    std::string user_id;
    std::string user_name;
    float familiarity = 0.0f;   // 熟悉度 0-10
    float affinity = 5.0f;      // 好感度 0-10
    int interaction_count = 0;  // 交互次数
    std::chrono::system_clock::time_point last_interaction;
    std::vector<std::string> shared_experiences;
};

// Agent 完整状态
class AgentState {
public:
    std::string agent_id;
    std::string current_activity = "idle";
    EmotionState emotion;
    
    // 三层记忆系统
    std::vector<Memory> short_term_memory;    // 最近 10 条
    std::vector<Memory> working_memory;       // 当前任务相关
    std::map<std::string, Relationship> relationships;  // 与各用户的关系
    
    // 个性特征
    struct Personality {
        std::string name;
        std::string role;
        std::vector<std::string> traits;       // ["开朗", "幽默", "细心"]
        std::vector<std::string> likes;
        std::vector<std::string> dislikes;
        nlohmann::json custom_data;
    } personality;
    
    void update_emotion(const std::string& event_type, float intensity);
    void add_memory(const Memory& memory);
    std::vector<Memory> retrieve_relevant_memories(
        const std::string& query, 
        size_t limit = 5
    ) const;
    Relationship& get_or_create_relationship(const std::string& user_id);
};

} // namespace nuclaw
