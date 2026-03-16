// ============================================================================
// emotion.hpp - 情感状态管理
// ============================================================================

#pragma once
#include <string>
#include <algorithm>

namespace nuclaw {

// 情感状态
struct EmotionState {
    float happiness = 0.0f;  // -10 (难过) ~ +10 (开心)
    float energy = 5.0f;     // 0 (疲惫) ~ 10 (精力充沛)
    float trust = 5.0f;      // 0 (不信任) ~ 10 (高度信任)
    
    std::string describe() const {
        if (happiness > 5) return "开心";
        if (happiness < -5) return "难过";
        if (energy < 3) return "疲惫";
        return "平静";
    }
    
    void update_from_interaction(const std::string& user_msg) {
        if (user_msg.find("谢谢") != std::string::npos ||
            user_msg.find("棒") != std::string::npos ||
            user_msg.find("好") != std::string::npos) {
            happiness += 1.0f;
            trust += 0.5f;
        } else if (user_msg.find("笨") != std::string::npos ||
                   user_msg.find("差") != std::string::npos ||
                   user_msg.find("错") != std::string::npos) {
            happiness -= 1.0f;
            trust -= 0.5f;
        }
        // 边界限制
        happiness = std::max(-10.0f, std::min(10.0f, happiness));
        trust = std::max(0.0f, std::min(10.0f, trust));
        energy = std::max(0.0f, std::min(10.0f, energy));
    }
};

} // namespace nuclaw
