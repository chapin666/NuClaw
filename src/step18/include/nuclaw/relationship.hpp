// ============================================================================
// relationship.hpp - 用户关系管理
// ============================================================================

#pragma once
#include <string>
#include <map>
#include <algorithm>

namespace nuclaw {

// 单个用户关系
struct Relationship {
    std::string user_id;
    std::string user_name;
    float familiarity = 0.0f;   // 熟悉度 0 ~ 10
    float affinity = 5.0f;      // 好感度 0 ~ 10
    int interaction_count = 0;  // 交互次数
    std::string last_interaction;
    
    std::string get_familiarity_level() const {
        if (familiarity > 7) return "老朋友";
        if (familiarity > 3) return "熟人";
        return "陌生人";
    }
};

// 关系管理器
class RelationshipManager {
public:
    Relationship& get_or_create(const std::string& user_id) {
        if (relationships_.count(user_id) == 0) {
            relationships_[user_id] = Relationship();
            relationships_[user_id].user_id = user_id;
        }
        return relationships_[user_id];
    }
    
    void update(const std::string& user_id, 
                const std::string& user_name,
                float affinity_delta = 0.5f) {
        auto& rel = get_or_create(user_id);
        rel.user_name = user_name;
        rel.affinity += affinity_delta;
        rel.interaction_count++;
        rel.familiarity = std::min(10.0f, rel.familiarity + 0.5f);
    }
    
    const Relationship* get(const std::string& user_id) const {
        auto it = relationships_.find(user_id);
        if (it != relationships_.end()) {
            return &it->second;
        }
        return nullptr;
    }
    
    const std::map<std::string, Relationship>& get_all() const {
        return relationships_;
    }
    
    size_t count() const { return relationships_.size(); }

private:
    std::map<std::string, Relationship> relationships_;
};

} // namespace nuclaw
