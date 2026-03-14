# Step 17: 旅行小管家

> 目标：基于 Step 16 的状态系统，构建带记忆能力的智能旅行助手
> 
> 难度：⭐⭐⭐⭐ (综合实战)
> 
> 代码量：约 1200 行

## 项目简介

**旅行小管家**是 Step 16 状态系统的实际应用。它拥有：

- 🧠 **记忆能力**：记得用户的偏好、历史行程
- 😊 **情感互动**：根据用户反馈调整推荐风格
- 👤 **用户画像**：为每个用户建立旅行偏好档案
- ✈️ **实用功能**：路线规划、预算计算、行李清单

## 核心架构

```
┌─────────────────────────────────────────────────────────────────────┐
│                        旅行小管家 TravelPal                         │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  ┌──────────────────────────────────────────────────────────────┐  │
│  │                    PersonalizedAgent                         │  │
│  │  (继承自 Step 16 的 AgentState + MemoryManager)              │  │
│  │                    + 旅行专业领域知识                         │  │
│  └──────────────────────────────────────────────────────────────┘  │
│                              │                                      │
│  ┌───────────────────────────┼───────────────────────────┐          │
│  │                           │                           │          │
│  ▼                           ▼                           ▼          │
│ ┌──────────────┐     ┌──────────────┐     ┌──────────────┐          │
│ │  行程规划器   │     │  预算计算器   │     │  行李清单器   │          │
│ │  (新)        │     │  (新)        │     │  (新)        │          │
│ └──────────────┘     └──────────────┘     └──────────────┘          │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

## 基于 Step 16 的扩展

### 1. 旅行偏好用户画像

```cpp
// travel_profile.hpp
#pragma once
#include <nuclaw/agent_state.hpp>

namespace travelpal {

struct TravelPreferences {
    // 从 Step 16 的 AgentState.personality 扩展
    std::vector<std::string> preferred_destinations;  // 偏好目的地
    std::vector<std::string> travel_styles;           // "背包客", "舒适型"
    std::vector<std::string> interests;               // "美食", "历史"
    int preferred_pace = 3;                           // 行程节奏 1-5
    double avg_budget_per_day = 500;                  // 日均预算
    std::string preferred_season;                     // 偏好季节
};

struct TravelHistory {
    std::vector<std::string> visited_countries;
    std::vector<std::string> favorite_hotels;
    int total_trips = 0;
    double total_spent = 0;
};

class TravelerProfile : public nuclaw::AgentState {
public:
    TravelPreferences travel_prefs;
    TravelHistory travel_history;
    
    // 从记忆中提取旅行偏好
    void extract_travel_preferences();
    
    // 推荐目的地（基于偏好和历史）
    std::vector<std::string> recommend_destinations(
        double budget, 
        int days,
        const std::string& season
    );
};

} // namespace travelpal
```

### 2. 行程规划器

```cpp
// trip_planner.hpp
#pragma once
#include "travel_profile.hpp"

namespace travelpal {

struct DayPlan {
    int day_number;
    std::vector<Activity> activities;
    std::string accommodation;
    Transport transport;
};

struct TripPlan {
    std::string destination;
    DateRange dates;
    double budget;
    std::vector<DayPlan> itinerary;
    std::vector<std::string> packing_list;
};

class TripPlanner {
public:
    TripPlanner(std::shared_ptr<TravelerProfile> profile)
        : profile_(profile) {}
    
    // 规划完整行程
    TripPlan plan_trip(const TripRequest& request) {
        // 1. 使用用户画像推荐目的地
        auto destinations = profile_->recommend_destinations(
            request.budget, request.days, request.season
        );
        
        // 2. 查询实时信息（天气、航班）
        auto weather = query_weather(destinations[0], request.dates);
        
        // 3. 生成每日行程（考虑用户 pace 偏好）
        TripPlan plan;
        for (int day = 1; day <= request.days; ++day) {
            DayPlan day_plan;
            day_plan.day_number = day;
            
            // 根据 pace 偏好安排活动数量
            int activity_count = profile_->travel_prefs.preferred_pace > 3 ? 4 : 2;
            
            day_plan.activities = select_attractions(
                destinations[0], 
                profile_->travel_prefs.interests,
                activity_count
            );
            
            plan.itinerary.push_back(day_plan);
        }
        
        // 4. 生成行李清单（基于天气和活动）
        plan.packing_list = generate_packing_list(
            destinations[0], weather, plan.itinerary
        );
        
        return plan;
    }

private:
    std::shared_ptr<TravelerProfile> profile_;
};

} // namespace travelpal
```

### 3. 个性化对话引擎（扩展 Step 16）

```cpp
// travelpal_chat.hpp
#pragma once
#include <nuclaw/personalized_chat.hpp>
#include "trip_planner.hpp"

namespace travelpal {

class TravelPalChat : public nuclaw::PersonalizedChatEngine {
public:
    TravelPalChat(
        std::shared_ptr<LLMClient> llm,
        std::shared_ptr<MemoryManager> memory_mgr,
        std::shared_ptr<TravelerProfile> profile
    ) : PersonalizedChatEngine(llm, memory_mgr), 
        profile_(profile),
        planner_(profile) {}
    
    async::Task<std::string> chat(
        const std::string& user_id,
        const std::string& user_name,
        const std::string& message) override {
        
        // 1. 检查是否是旅行规划请求
        auto intent = classify_travel_intent(message);
        
        if (intent == TravelIntent::PLAN_TRIP) {
            // 提取需求参数
            auto params = extract_trip_params(message);
            
            // 2. 使用用户画像优化推荐
            TripRequest req{
                .user_id = user_id,
                .budget = params.budget,
                .days = params.days,
                .season = params.season,
                .preferences = profile_->travel_prefs
            };
            
            // 3. 生成行程
            auto plan = planner_.plan_trip(req);
            
            // 4. 格式化回复（带有个性化语气）
            return format_plan_response(plan, profile_->emotion);
        }
        
        // 其他意图：使用父类的个性化对话
        return PersonalizedChatEngine::chat(user_id, user_name, message);
    }

private:
    std::shared_ptr<TravelerProfile> profile_;
    TripPlanner planner_;
};

} // namespace travelpal
```

## 交互示例

```
用户：我想去日本

小旅：日本！✨ 好选择！我记得你之前去过东京，
这次想去关西地区吗？那边有京都、大阪、奈良...

你更喜欢：
1️⃣ 关西深度游（京都-大阪-奈良）
2️⃣ 北海道雪景
3️⃣ 冲绳海岛

（已根据用户历史推荐不同的选项）

---

用户：预算 1 万，5 天

小旅：收到！让我根据你的偏好规划一下...
（我知道你爱吃美食、喜欢悠闲的行程，
所以每天安排 2-3 个景点，留出用餐时间）

🗓️ 行程建议：
Day 1: 抵达大阪，道顿堀美食
Day 2: 大阪城 → 京都（清水寺夜樱）
Day 3: 京都和服体验 → 岚山竹林（你之前提过想拍竹林照片）
Day 4: 奈良看鹿 → 返回大阪
Day 5: 购物 → 返程

💰 预算明细：
- 机票：3500（知道你偏好直飞）
- 住宿：2500（民宿风格，你之前给好评的店）
- 餐饮：2000（预留了 400/天美食预算）
- 交通：800
- 门票购物：1200

🎒 行李清单（基于 3 月天气）：
☑️ 春装外套（早晚温差大）
☑️  walking shoes（你脚伤刚恢复）
☑️ 相机（你说要拍樱花）
```

## 从 Step 16 继承的组件

| Step 16 组件 | Step 17 扩展 |
|:---:|:---|
| `AgentState` | `TravelerProfile`（添加旅行偏好） |
| `MemoryManager` | 提取旅行相关记忆 |
| `Relationship` | 记录用户的旅行伙伴 |
| `EmotionState` | 根据行程满意度调整 |

## 新增组件

- `TravelPreferences` — 旅行偏好设置
- `TripPlanner` — 行程规划引擎
- `BudgetCalculator` — 预算分配计算
- `PackingListGenerator` — 行李清单生成

---

**下一章预告：Step 18 — 虚拟咖啡厅「星语轩」**

在 Step 17 的基础上，不再只有一个 Agent，而是多个拥有独立状态、记忆、情感的 Agent 在同一个空间中互动！
