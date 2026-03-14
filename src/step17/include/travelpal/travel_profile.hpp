// include/travelpal/travel_profile.hpp
#pragma once
#include <string>
#include <vector>
#include <map>

namespace travelpal {

// 旅行风格
enum class TravelStyle {
    BACKPACKER,      // 背包客
    COMFORT,         // 舒适型
    LUXURY,          // 奢华型
    FAMILY,          // 亲子游
    ROMANTIC,        // 浪漫游
    ADVENTURE        // 探险型
};

// 用户画像
struct TravelerProfile {
    std::string user_id;
    std::string name;
    
    // 偏好
    TravelStyle style = TravelStyle::COMFORT;
    std::map<std::string, int> activity_preferences;  // 活动偏好分数 (0-10)
    double daily_budget = 500;
    
    // 特殊需求
    std::vector<std::string> dietary_restrictions;
    std::vector<std::string> allergies;
    std::vector<std::string> mobility_limitations;
    
    // 记忆
    std::map<std::string, std::string> key_memories;
    std::vector<std::string> past_destinations;
    
    void remember(const std::string& key, const std::string& value) {
        key_memories[key] = value;
    }
    
    std::string recall(const std::string& key) const {
        auto it = key_memories.find(key);
        return it != key_memories.end() ? it->second : "";
    }
};

// 行程
struct TripPlan {
    std::string destination;
    int days;
    double total_budget;
    std::vector<std::string> activities;
    
    std::string to_string() const {
        std::string result = "📍 " + destination + " " + std::to_string(days) + "日游\n";
        result += "💰 预算：" + std::to_string(int(total_budget)) + "元\n\n";
        result += "🗓️ 行程：\n";
        for (size_t i = 0; i < activities.size(); ++i) {
            result += "Day " + std::to_string(i + 1) + ": " + activities[i] + "\n";
        }
        return result;
    }
};

} // namespace travelpal