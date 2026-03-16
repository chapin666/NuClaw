// ============================================================================
// step17_progressive.hpp - Step 17: 旅行小管家（基于 Step 16 演进）
// ============================================================================
//
// 演进路径：
// Step 16: StatefulAgent ──→ Step 17: TravelAgent（旅行专用）
//                    继承 + 旅行知识
// ============================================================================

#pragma once
#include "step16_progressive.hpp"
#include <vector>
#include <map>

namespace step17 {

using namespace step16;

// ============================================
// Step 17: 旅行专用 Agent（继承 StatefulAgent）
// ============================================

struct TravelPreference {
    std::string user_id;
    std::string preferred_style;  // 背包客/舒适/奢华
    double daily_budget = 500;
    std::vector<std::string> preferred_destinations;
    std::vector<std::string> dietary_restrictions;
    std::vector<std::string> special_needs;
};

struct TripPlan {
    std::string destination;
    int days;
    double total_budget;
    std::vector<std::string> activities;
    std::vector<std::string> food_recommendations;
};

class TravelAgent : public StatefulAgent {
public:
    TravelAgent(const std::string& name) 
        : StatefulAgent(name, "旅行助手") {
        init_knowledge_base();
    }
    
    // 处理旅行相关查询
    std::string handle_travel_query(const std::string& user_id,
                                     const std::string& user_name,
                                     const std::string& query) {
        // 先使用父类的记忆功能
        std::string base_response = process_user_message(user_id, user_name, query);
        
        // 提取旅行偏好（存入记忆）
        extract_travel_preferences(user_id, query);
        
        // 旅行专用意图识别
        if (query.find("推荐") != std::string::npos ||
            query.find("去哪") != std::string::npos) {
            return recommend_destination(user_id);
        }
        else if (query.find("行程") != std::string::npos ||
                 query.find("规划") != std::string::npos) {
            return generate_trip_plan(user_id);
        }
        else if (query.find("预算") != std::string::npos) {
            return calculate_budget(user_id);
        }
        else if (query.find("吃") != std::string::npos ||
                 query.find("美食") != std::string::npos) {
            return recommend_food(user_id);
        }
        
        return base_response;
    }
    
    // 显示旅行相关记忆
    void show_travel_memory() {
        show_state();
        
        std::cout << "\n🧳 旅行偏好记忆:\n";
        for (const auto& [user_id, pref] : user_preferences_) {
            std::cout << "  用户 " << pref.preferred_style << ":\n";
            std::cout << "    预算: " << pref.daily_budget << "元/天\n";
            std::cout << "    偏好目的地: ";
            for (const auto& dest : pref.preferred_destinations) {
                std::cout << dest << " ";
            }
            std::cout << "\n";
        }
    }

private:
    std::map<std::string, TravelPreference> user_preferences_;
    std::map<std::string, std::vector<std::string>> destination_db_;
    
    void init_knowledge_base() {
        // 目的地数据库
        destination_db_["日本"] = {
            "京都 - 古寺、樱花、传统文化",
            "大阪 - 美食、购物、道顿堀",
            "奈良 - 鹿公园、东大寺",
            "东京 - 现代都市、秋叶原"
        };
        destination_db_["泰国"] = {
            "清迈 - 古城、寺庙、慢生活",
            "曼谷 - 大皇宫、夜市、SPA",
            "普吉岛 - 海滩、潜水、度假"
        };
        destination_db_["云南"] = {
            "丽江 - 古城、玉龙雪山",
            "大理 - 洱海、苍山、风花雪月",
            "香格里拉 - 藏区风情、普达措"
        };
    }
    
    void extract_travel_preferences(const std::string& user_id, 
                                     const std::string& query) {
        auto& pref = user_preferences_[user_id];
        pref.user_id = user_id;
        
        // 提取预算
        if (query.find("1万") != std::string::npos) {
            pref.daily_budget = 2000;  // 5天1万 = 2000/天
            state_.add_memory("用户预算: 1万元", "preference", user_id);
        } else if (query.find("5000") != std::string::npos) {
            pref.daily_budget = 1000;
            state_.add_memory("用户预算: 5000元", "preference", user_id);
        }
        
        // 提取目的地偏好
        if (query.find("日本") != std::string::npos) {
            pref.preferred_destinations.push_back("日本");
            state_.add_memory("偏好目的地: 日本", "preference", user_id);
        } else if (query.find("泰国") != std::string::npos) {
            pref.preferred_destinations.push_back("泰国");
            state_.add_memory("偏好目的地: 泰国", "preference", user_id);
        } else if (query.find("云南") != std::string::npos) {
            pref.preferred_destinations.push_back("云南");
            state_.add_memory("偏好目的地: 云南", "preference", user_id);
        }
        
        // 提取特殊需求
        if (query.find("海鲜过敏") != std::string::npos) {
            pref.dietary_restrictions.push_back("海鲜过敏");
            state_.add_memory("特殊需求: 海鲜过敏", "important", user_id);
        }
        if (query.find("脚伤") != std::string::npos ||
            query.find("不能走") != std::string::npos) {
            pref.special_needs.push_back("行动不便");
            state_.add_memory("特殊需求: 行动不便", "important", user_id);
        }
        
        // 提取风格
        if (query.find("穷游") != std::string::npos ||
            query.find("背包") != std::string::npos) {
            pref.preferred_style = "背包客";
        } else if (query.find("舒适") != std::string::npos) {
            pref.preferred_style = "舒适型";
        } else if (query.find("奢华") != std::string::npos ||
                   query.find("豪华") != std::string::npos) {
            pref.preferred_style = "奢华型";
        }
    }
    
    std::string recommend_destination(const std::string& user_id) {
        auto& pref = user_preferences_[user_id];
        std::string response = "根据您的情况，我推荐:\n\n";
        
        if (pref.preferred_destinations.empty()) {
            response += "🗾 日本关西（京都-大阪-奈良）\n";
            response += "   适合初次出国，美食文化丰富\n";
        } else {
            std::string dest = pref.preferred_destinations.back();
            response += "🗾 " + dest + "\n";
            
            auto it = destination_db_.find(dest);
            if (it != destination_db_.end()) {
                for (const auto& place : it->second) {
                    response += "   • " + place + "\n";
                }
            }
        }
        
        // 根据特殊需求调整
        if (!pref.special_needs.empty()) {
            response += "\n⚠️ 考虑到您的" + pref.special_needs[0] + "，";
            response += "我会安排轻松行程\n";
        }
        
        return response;
    }
    
    std::string generate_trip_plan(const std::string& user_id) {
        auto& pref = user_preferences_[user_id];
        std::string dest = pref.preferred_destinations.empty() ? 
                          "日本关西" : pref.preferred_destinations.back();
        
        std::string response = "📍 " + dest + " 5日游规划:\n\n";
        
        bool easy_mode = false;
        for (const auto& need : pref.special_needs) {
            if (need == "行动不便") easy_mode = true;
        }
        
        if (easy_mode) {
            response += "Day 1: 抵达，酒店休息，附近晚餐\n";
            response += "Day 2: 市区景点（打车为主），轻松游览\n";
            response += "Day 3: 文化体验（室内活动为主）\n";
            response += "Day 4: 近郊游览，充足休息时间\n";
            response += "Day 5: 购物，返程\n";
        } else {
            response += "Day 1: 抵达，探索美食\n";
            response += "Day 2: 主要景点游览\n";
            response += "Day 3: 深度文化体验\n";
            response += "Day 4: 周边一日游\n";
            response += "Day 5: 购物，返程\n";
        }
        
        return response;
    }
    
    std::string calculate_budget(const std::string& user_id) {
        auto& pref = user_preferences_[user_id];
        double daily = pref.daily_budget > 0 ? pref.daily_budget : 1000;
        
        std::string response = "💰 预算分配建议（" + 
            std::to_string(int(daily)) + "元/天）:\n\n";
        response += "🏨 住宿: " + std::to_string(int(daily * 0.35)) + "元\n";
        response += "🍜 餐饮: " + std::to_string(int(daily * 0.30)) + "元\n";
        response += "🚇 交通: " + std::to_string(int(daily * 0.15)) + "元\n";
        response += "🎫 门票: " + std::to_string(int(daily * 0.15)) + "元\n";
        response += "🛍️ 其他: " + std::to_string(int(daily * 0.05)) + "元\n";
        
        return response;
    }
    
    std::string recommend_food(const std::string& user_id) {
        auto& pref = user_preferences_[user_id];
        std::string response = "🍜 美食推荐:\n\n";
        
        if (!pref.dietary_restrictions.empty()) {
            response += "⚠️ 注意：您有" + pref.dietary_restrictions[0] + "\n";
            response += "推荐以下无海鲜餐厅:\n";
        }
        
        if (pref.preferred_destinations.empty() ||
            pref.preferred_destinations.back() == "日本") {
            response += "• 京都：汤豆腐、怀石料理\n";
            response += "• 大阪：炸串、章鱼烧（可选无海鲜）\n";
            response += "• 奈良：柿叶寿司（选无海鲜款）\n";
        } else if (pref.preferred_destinations.back() == "泰国") {
            response += "• 清迈：Khao Soi（咖喱面）\n";
            response += "• 芒果糯米饭\n";
            response += "• 泰式炒粉（选无海鲜）\n";
        }
        
        return response;
    }
};

} // namespace step17