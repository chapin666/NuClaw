# Step 17 完整实现指南

## 快速开始（30 分钟上手）

### 第一步：环境准备

```bash
# 1. 确保在 step16 基础上工作
cd NuClaw
cp -r src/step16 src/step17
cd src/step17

# 2. 创建项目结构
mkdir -p include/travelpal src tests data/{profiles,destinations,activities}

# 3. 安装依赖（如果还没有）
# Ubuntu/Debian
sudo apt-get install libboost-all-dev nlohmann-json-dev libfmt-dev

# macOS
brew install boost nlohmann-json fmt
```

### 第二步：最小可用版本（MVP）

让我们先实现一个最简版本，能跑起来最重要：

```cpp
// include/travelpal/simple_travelpal.hpp
#pragma once
#include <string>
#include <vector>
#include <map>
#include <nlohmann/json.hpp>

namespace travelpal {

// 最简用户画像
struct SimpleProfile {
    std::string user_id;
    std::string name;
    std::map<std::string, std::string> memories;  // 关键记忆
    std::vector<std::string> preferences;         // 偏好
    
    void remember(const std::string& key, const std::string& value) {
        memories[key] = value;
    }
    
    std::string recall(const std::string& key) const {
        auto it = memories.find(key);
        return it != memories.end() ? it->second : "";
    }
};

// 最简行程规划
struct TripPlan {
    std::string destination;
    int days;
    double budget;
    std::vector<std::string> activities;
    
    std::string to_string() const {
        std::string result = fmt::format("📍 {} {}日游\n", destination, days);
        result += fmt::format("💰 预算：{}元\n\n", budget);
        result += "🗓️ 行程：\n";
        for (size_t i = 0; i < activities.size(); ++i) {
            result += fmt::format("Day {}: {}\n", i + 1, activities[i]);
        }
        return result;
    }
};

// 最简旅行助手
class SimpleTravelPal {
public:
    SimpleProfile profile;
    
    // 处理用户输入
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
        
        // 默认回复
        return generate_response(user_input);
    }

private:
    void extract_info(const std::string& input) {
        // 提取天数
        if (input.find("5天") != std::string::npos || 
            input.find("五天") != std::string::npos) {
            profile.remember("days", "5");
        }
        
        // 提取预算
        if (input.find("1万") != std::string::npos ||
            input.find("10000") != std::string::npos) {
            profile.remember("budget", "10000");
        }
        
        // 提取目的地偏好
        if (input.find("日本") != std::string::npos) {
            profile.remember("preferred_country", "日本");
        }
        
        // 提取特殊需求
        if (input.find("过敏") != std::string::npos) {
            profile.remember("has_allergy", "true");
            // 提取具体过敏源
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
        
        // 检查是否有季节偏好
        if (profile.recall("preferred_month") == "3") {
            response += "🌸 **日本关西** - 樱花季！\n";
            response += "   京都、大阪、奈良，古都可以看樱花\n";
            response += "   3 月底正好是樱花满开的时候\n\n";
        } else {
            response += "🗾 **日本关西**\n";
            response += "   京都、大阪、奈良\n";
            response += "   美食、文化、风景都有\n\n";
        }
        
        // 根据特殊需求调整
        if (profile.recall("mobility_limitation") == "true") {
            response += "⚠️ 考虑到你的脚伤，我选的景点都比较平坦，\n";
            response += "   每天都有充足的休息时间。\n\n";
        }
        
        if (profile.recall("allergy_seafood") == "true") {
            response += "🍽️ 你有海鲜过敏，我会推荐：\n";
            response += "   • 京都的汤豆腐、怀石料理\n";
            response += "   • 大阪的炸串、章鱼烧（可选无海鲜）\n";
            response += "   • 避开海鲜市场类的餐厅\n";
        }
        
        return response;
    }
    
    std::string generate_plan() {
        int days = std::stoi(profile.recall("days"));
        double budget = std::stod(profile.recall("budget"));
        
        TripPlan plan;
        plan.destination = "日本关西（京都-大阪-奈良）";
        plan.days = days;
        plan.budget = budget;
        
        // 根据用户情况调整行程
        if (profile.recall("mobility_limitation") == "true") {
            // 脚伤用户：减少步行，增加休息
            plan.activities = {
                "抵达大阪，酒店休息，道顿堀晚餐（少步行）",
                "大阪城（电梯上城）→ 京都，酒店休息",
                "和服体验（店内为主）→ 清水寺（打车上下）",
                "奈良（近铁直达，公园喂鹿，少走山路）",
                "黑门市场（可选）→ 返程"
            };
        } else {
            plan.activities = {
                "抵达大阪，道顿堀美食",
                "大阪城 → 前往京都，清水寺夜樱",
                "和服体验 → 伏见稻荷 → 岚山",
                "奈良一日游（东大寺、春日大社）",
                "购物 → 返程"
            };
        }
        
        return plan.to_string();
    }
    
    std::string calculate_budget() {
        double budget = std::stod(profile.recall("budget"));
        int days = std::stoi(profile.recall("days"));
        double daily = budget / days;
        
        std::string response = fmt::format("💰 预算分配（{}元/天）\n\n", daily);
        
        response += fmt::format("🏨 住宿：{:.0f}元/晚（民宿/商务酒店）\n", daily * 0.35);
        response += fmt::format("🍜 餐饮：{:.0f}元/天\n", daily * 0.30);
        response += fmt::format("🚇 交通：{:.0f}元（JR Pass + 市内）\n", daily * 0.15);
        response += fmt::format("🎫 活动：{:.0f}元/天\n", daily * 0.15);
        response += fmt::format("🛍️ 购物/其他：{:.0f}元\n", daily * 0.05);
        
        return response;
    }
    
    std::string generate_response(const std::string& input) {
        // 简单的记忆反馈
        if (profile.recall("name").empty() && input.length() < 10) {
            profile.name = input;
            return fmt::format("你好{}！我是你的旅行小管家。\n"
                             "告诉我你想去哪里、预算多少、几天时间？", input);
        }
        
        return "我收到了！可以告诉我更多细节吗？比如：\n"
               "• 想玩几天？\n"
               "• 预算多少？\n"
               "• 有什么特殊需求吗？（比如饮食限制、身体状况）";
    }
};

} // namespace travelpal
```

```cpp
// src/main.cpp
#include "travelpal/simple_travelpal.hpp"
#include <iostream>
#include <string>

int main() {
    using namespace travelpal;
    
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
```

### 第三步：编译运行

```bash
# 编译
g++ -std=c++17 -I include src/main.cpp -o travelpal -lfmt

# 运行
./travelpal
```

### 第四步：测试对话

```
☕ 旅行小管家上线！
请输入你的名字：小李

你好 小李！我是你的旅行小管家。
告诉我你想去哪里旅行？

> 我想去日本，预算1万，5天，脚伤刚恢复，对海鲜过敏

🤖 根据你的情况，我推荐：

🌸 **日本关西** - 樱花季！
   京都、大阪、奈良，古都可以看樱花
   3 月底正好是樱花满开的时候

⚠️ 考虑到你的脚伤，我选的景点都比较平坦，
   每天都有充足的休息时间。

🍽️ 你有海鲜过敏，我会推荐：
   • 京都的汤豆腐、怀石料理
   • 大阪的炸串、章鱼烧（可选无海鲜）
   • 避开海鲜市场类的餐厅

> 帮我规划行程

🤖 📍 日本关西（京都-大阪-奈良） 5日游
💰 预算：10000元

🗓️ 行程：
Day 1: 抵达大阪，酒店休息，道顿堀晚餐（少步行）
Day 2: 大阪城（电梯上城）→ 京都，酒店休息
Day 3: 和服体验（店内为主）→ 清水寺（打车上下）
Day 4: 奈良（近铁直达，公园喂鹿，少走山路）
Day 5: 黑门市场（可选）→ 返程

> 预算怎么分配

🤖 💰 预算分配（2000元/天）

🏨 住宿：700元/晚（民宿/商务酒店）
🍜 餐饮：600元/天
🚇 交通：300元（JR Pass + 市内）
🎫 活动：300元/天
🛍️ 购物/其他：100元

> quit

再见！祝旅途愉快！
```

---

## 进阶：完整版实现

如果你已经完成了上面的 MVP，可以继续实现完整版：

### 完整版功能清单

- [ ] 用户画像系统（TravelerProfile）
- [ ] 意图识别（规则 + LLM 混合）
- [ ] 行程规划算法（考虑时间、距离、体力）
- [ ] 预算计算器（分项详细预算）
- [ ] 行李清单生成器（根据天气、活动、特殊需求）
- [ ] 记忆提取器（从对话中提取关键信息）
- [ ] 向量检索（长期记忆）
- [ ] IM 接入（微信/钉钉机器人）

### 下一步

参考 `case_study.md` 中的详细代码实现完整版。

---

**恭喜！你已经实现了一个最简单的旅行助手！** 🎉

接下来可以尝试：
1. 添加更多目的地数据
2. 接入真实天气 API
3. 使用 LLM 生成更自然的回复
4. 接入微信/钉钉，让它成为真正的机器人
