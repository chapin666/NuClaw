// src/main.cpp - Step 17: 旅行小管家
#include "travelpal/travel_profile.hpp"
#include <iostream>
#include <string>
#include <cstdlib>

using namespace travelpal;

// 最简旅行助手
class SimpleTravelPal {
public:
    TravelerProfile profile;
    
    std::string chat(const std::string& user_input) {
        // 提取信息
        extract_info(user_input);
        
        // 判断意图
        if (user_input.find("去哪") != std::string::npos ||
            user_input.find("推荐") != std::string::npos) {
            return recommend_destination();
        }
        
        if (user_input.find("行程") != std::string::npos ||
            user_input.find("规划") != std::string::npos) {
            return generate_plan();
        }
        
        if (user_input.find("预算") != std::string::npos) {
            return calculate_budget();
        }
        
        return generate_response(user_input);
    }

private:
    void extract_info(const std::string& input) {
        // 提取天数
        if (input.find("5天") != std::string::npos || 
            input.find("五天") != std::string::npos) {
            profile.remember("days", "5");
        } else if (input.find("3天") != std::string::npos ||
                   input.find("三天") != std::string::npos) {
            profile.remember("days", "3");
        }
        
        // 提取预算
        if (input.find("1万") != std::string::npos ||
            input.find("10000") != std::string::npos) {
            profile.remember("budget", "10000");
        } else if (input.find("5000") != std::string::npos) {
            profile.remember("budget", "5000");
        }
        
        // 提取目的地偏好
        if (input.find("日本") != std::string::npos) {
            profile.remember("preferred_country", "日本");
        } else if (input.find("泰国") != std::string::npos) {
            profile.remember("preferred_country", "泰国");
        }
        
        // 提取特殊需求
        if (input.find("过敏") != std::string::npos) {
            profile.remember("has_allergy", "true");
            if (input.find("海鲜") != std::string::npos) {
                profile.remember("allergy_seafood", "true");
            }
        }
        
        if (input.find("脚伤") != std::string::npos ||
            input.find("不能走") != std::string::npos) {
            profile.remember("mobility_limitation", "true");
        }
    }
    
    std::string recommend_destination() {
        std::string response = "根据你的情况，我推荐：\n\n";
        
        std::string country = profile.recall("preferred_country");
        if (country == "日本") {
            response += "🗾 **日本关西**\n";
            response += "   京都、大阪、奈良\n";
            response += "   美食、文化、风景都有\n\n";
        } else if (country == "泰国") {
            response += "🏝️ **泰国清迈**\n";
            response += "   古城、寺庙、美食\n";
            response += "   性价比高，适合放松\n\n";
        } else {
            response += "🗾 **日本关西**\n   京都、大阪、奈良\n\n";
        }
        
        // 根据特殊需求调整
        if (profile.recall("mobility_limitation") == "true") {
            response += "⚠️ 考虑到你的脚伤，我选的景点都比较平坦，\n";
            response += "   每天都有充足的休息时间。\n\n";
        }
        
        if (profile.recall("allergy_seafood") == "true") {
            response += "🍽️ 你有海鲜过敏，我会推荐无海鲜的餐厅。\n";
        }
        
        return response;
    }
    
    std::string generate_plan() {
        std::string days_str = profile.recall("days");
        std::string budget_str = profile.recall("budget");
        
        if (days_str.empty()) days_str = "5";
        if (budget_str.empty()) budget_str = "10000";
        
        int days = std::stoi(days_str);
        double budget = std::stod(budget_str);
        
        TripPlan plan;
        plan.destination = profile.recall("preferred_country") == "泰国" 
            ? "泰国清迈" : "日本关西（京都-大阪-奈良）";
        plan.days = days;
        plan.total_budget = budget;
        
        // 根据用户情况调整行程
        if (profile.recall("mobility_limitation") == "true") {
            plan.activities = {
                "抵达，酒店休息，附近晚餐（少步行）",
                "市区景点（打车为主），酒店休息",
                "文化体验（店内为主），轻松活动",
                "近郊游览（少走路），返程准备",
                "购物，返程"
            };
        } else {
            plan.activities = {
                "抵达，美食探索",
                "市区主要景点",
                "文化体验",
                "周边游览",
                "购物，返程"
            };
        }
        
        // 调整天数
        while ((int)plan.activities.size() > days) {
            plan.activities.pop_back();
        }
        while ((int)plan.activities.size() < days) {
            plan.activities.push_back("自由活动");
        }
        
        return plan.to_string();
    }
    
    std::string calculate_budget() {
        std::string budget_str = profile.recall("budget");
        std::string days_str = profile.recall("days");
        
        if (budget_str.empty()) budget_str = "10000";
        if (days_str.empty()) days_str = "5";
        
        double budget = std::stod(budget_str);
        int days = std::stoi(days_str);
        double daily = budget / days;
        
        std::string response = "💰 预算分配（约" + std::to_string(int(daily)) + "元/天）\n\n";
        response += "🏨 住宿：" + std::to_string(int(daily * 0.35)) + "元/晚\n";
        response += "🍜 餐饮：" + std::to_string(int(daily * 0.30)) + "元/天\n";
        response += "🚇 交通：" + std::to_string(int(daily * 0.15)) + "元\n";
        response += "🎫 活动：" + std::to_string(int(daily * 0.15)) + "元/天\n";
        response += "🛍️ 其他：" + std::to_string(int(daily * 0.05)) + "元\n";
        
        return response;
    }
    
    std::string generate_response(const std::string& input) {
        if (profile.name.empty() && input.length() < 10) {
            profile.name = input;
            return "你好" + input + "！我是你的旅行小管家。\n"
                   "告诉我你想去哪里、预算多少、几天时间？";
        }
        
        return "我收到了！可以告诉我更多细节吗？比如：\n"
               "• 想玩几天？\n"
               "• 预算多少？\n"
               "• 有什么特殊需求吗？";
    }
};

int main() {
    SimpleTravelPal travelpal;
    std::string input;
    
    std::cout << "☕ 旅行小管家上线！\n";
    std::cout << "请输入你的名字：";
    std::getline(std::cin, input);
    travelpal.profile.name = input;
    
    std::cout << "\n你好 " << input << "！我是你的旅行小管家。\n";
    std::cout << "告诉我你想去哪里旅行？\n\n";
    
    while (true) {
        std::cout << "> ";
        std::getline(std::cin, input);
        
        if (input == "quit" || input == "exit") {
            break;
        }
        
        std::string response = travelpal.chat(input);
        std::cout << "\n🤖 " << response << "\n\n";
    }
    
    std::cout << "再见！祝旅途愉快！\n";
    return 0;
}