# Step 17: 旅行小管家

> **目标**：基于 Step 16 的状态系统，构建带记忆能力的智能旅行助手
> 
> **难度**：⭐⭐⭐⭐ (综合实战)
> 
> **代码量**：约 1200 行
> 
> **预计时间**：4-6 小时

!!! tip "快速开始"
    如果你想先跑起来再说，查看 [**30 分钟快速上手指南**](quickstart.md)，
    实现一个最简版的旅行助手！

---

## 🎯 项目概述

### 为什么选择"旅行助手"作为实战项目？

旅行规划是 AI Agent 的典型应用场景：
- **个性化强**：每个人的偏好不同（美食 vs 风景，慢游 vs 打卡）
- **信息密集**：需要整合天气、航班、酒店、景点等多源数据
- **流程复杂**：规划 → 预订 → 准备 → 出行 → 复盘，长周期跟踪
- **记忆重要**：记得用户"上次日本行住的那家酒店"比重新推荐更高效

### 我们要实现什么

**旅行小管家**是一个能记住你、懂你的智能旅行助手：

| 能力 | 说明 |
|:---|:---|
| 🧠 **记忆能力** | 记住你的偏好、历史行程、旅行习惯 |
| 💬 **自然对话** | 像朋友一样聊天，而非填表式交互 |
| 📍 **行程规划** | 根据预算、时间、偏好生成完整行程 |
| 💰 **预算管理** | 自动分配各项预算，实时跟踪支出 |
| 🎒 **智能清单** | 根据目的地、天气、活动生成行李清单 |
| 😊 **情感陪伴** | 旅行前兴奋、旅途中陪伴、回来后分享 |

---

## 📐 架构设计

### 系统架构图

```
┌─────────────────────────────────────────────────────────────────────────┐
│                           旅行小管家 TravelPal                          │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  ┌──────────────────────────────────────────────────────────────────┐  │
│  │                        用户层                                     │  │
│  │  • 微信/钉钉/飞书  • Web 前端  • 语音对话                         │  │
│  └──────────────────────────────────────────────────────────────────┘  │
│                              │                                          │
│  ┌───────────────────────────┴───────────────────────────┐              │
│  ▼                                                       ▼              │
│ ┌──────────────────────────────────────────────────────────────────┐   │
│ │                    TravelPalChatEngine                           │   │
│ │  ┌────────────────────────────────────────────────────────────┐  │   │
│ │  │  PersonalizedChatEngine (继承自 Step 16)                    │  │   │
│ │  │  - 情感状态计算                                             │  │   │
│ │  │  - 短期记忆检索                                             │  │   │
│ │  │  - 长期记忆检索 (向量数据库)                                 │  │   │
│ │  └────────────────────────────────────────────────────────────┘  │   │
│ │                            ↓                                     │   │
│ │  ┌────────────────────────────────────────────────────────────┐  │   │
│ │  │              TravelIntentClassifier (意图识别)              │  │   │
│ │  │  • 规划行程  • 查询天气  • 预算计算  • 闲聊                │  │   │
│ │  └────────────────────────────────────────────────────────────┘  │   │
│ │                            ↓                                     │   │
│ │  ┌────────────────────────────────────────────────────────────┐  │   │
│ │  │            TravelerProfile (用户画像)                        │  │   │
│ │  │  - 旅行偏好 (目的地/风格/节奏/预算)                          │  │   │
│ │  │  - 历史行程                                                  │  │   │
│ │  │  - 重要记忆 (过敏/脚伤/蜜月等)                               │  │   │
│ │  └────────────────────────────────────────────────────────────┘  │   │
│ └──────────────────────────────────────────────────────────────────┘   │
│                              │                                          │
│  ┌───────────────────────────┼───────────────────────────┐              │
│  ▼                           ▼                           ▼              │
│ ┌──────────────┐      ┌──────────────┐      ┌──────────────┐          │
│ │   行程规划    │      │   工具调用    │      │   记忆管理    │          │
│ │  TripPlanner │      │ ToolExecutor │      │MemoryManager │          │
│ │              │      │              │      │              │          │
│ │ • 目的地推荐 │      │ • 天气查询   │      │ • 记忆提取   │          │
│ │ • 路线设计   │      │ • 航班搜索   │      │ • 关系追踪   │          │
│ │ • 活动安排   │      │ • 酒店比价   │      │ • 偏好学习   │          │
│ │ • 预算分配   │      │ • 景点推荐   │      │ • 重要标记   │          │
│ └──────────────┘      └──────────────┘      └──────────────┘          │
│                                                                         │
│  ┌──────────────────────────────────────────────────────────────────┐  │
│  │                       数据层                                      │  │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐          │  │
│  │  │ SQLite   │  │ Milvus   │  │ 缓存     │  │ 配置     │          │  │
│  │  │ (关系)   │  │ (向量)   │  │ (Redis)  │  │ (YAML)   │          │  │
│  │  └──────────┘  └──────────┘  └──────────┘  └──────────┘          │  │
│  └──────────────────────────────────────────────────────────────────┘  │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 🗂️ 项目结构

```
step17/
├── CMakeLists.txt
├── README.md
├── configs/
│   └── config.yaml          # 旅行助手配置
├── include/
│   └── travelpal/           # 旅行助手命名空间
│       ├── travel_profile.hpp       # 用户画像
│       ├── trip_planner.hpp         # 行程规划
│       ├── budget_calculator.hpp    # 预算计算
│       ├── packing_generator.hpp    # 行李清单
│       ├── travel_intent.hpp        # 意图识别
│       └── travelpal_chat.hpp       # 对话引擎
├── src/
│   ├── travel_profile.cpp
│   ├── trip_planner.cpp
│   ├── main.cpp
│   └── ...
└── tests/
    └── test_travelpal.cpp
```

---

## 📝 核心实现详解

### 1. 用户画像系统（继承 Step 16）

```cpp
// include/travelpal/travel_profile.hpp
#pragma once
#include <nuclaw/agent_state.hpp>
#include <vector>
#include <string>
#include <map>

namespace travelpal {

// 旅行风格枚举
enum class TravelStyle {
    BACKPACKER,      // 背包客
    COMFORT,         // 舒适型
    LUXURY,          // 奢华型
    FAMILY,          // 亲子游
    COUPLE,          // 情侣游
    SOLO             // 独行
};

// 活动偏好
struct ActivityPreferences {
    int nature = 5;        // 自然风光 1-10
    int culture = 5;       // 人文历史 1-10
    int food = 5;          // 美食探索 1-10
    int shopping = 5;      // 购物 1-10
    int adventure = 5;     // 冒险活动 1-10
    int relaxation = 5;    // 休闲度假 1-10
};

// 旅行历史记录
struct TripRecord {
    std::string trip_id;
    std::string destination;
    std::string start_date;
    std::string end_date;
    double total_cost;
    int rating;            // 1-5 评分
    std::vector<std::string> highlights;  // 亮点
    std::vector<std::string> regrets;    // 遗憾
    std::string accommodation;
    std::string notes;
};

// 用户画像 - 继承 Step 16 的 AgentState
class TravelerProfile : public nuclaw::AgentState {
public:
    // ========== 基础信息 ==========
    std::string user_id;
    std::string user_name;
    std::string created_at;
    
    // ========== 旅行偏好 ==========
    TravelStyle preferred_style = TravelStyle::COMFORT;
    ActivityPreferences activity_prefs;
    std::vector<std::string> preferred_destinations;  // 想去的地方
    std::vector<std::string> visited_destinations;    // 去过的地方
    std::vector<std::string> avoid_destinations;     // 不想去
    
    // 预算范围
    struct BudgetRange {
        double min_daily = 300;
        double max_daily = 1000;
        double preferred_daily = 500;
    } budget_range;
    
    // 时间偏好
    struct TimePreference {
        int preferred_trip_length = 5;        // 偏好行程天数
        int max_trip_length = 10;
        std::vector<std::string> preferred_months;  // 偏好月份
        bool avoid_crowds = true;             // 避开人潮
    } time_pref;
    
    // ========== 特殊需求 ==========
    struct SpecialNeeds {
        std::vector<std::string> dietary_restrictions;  // 饮食限制
        std::vector<std::string> accessibility_needs;   // 无障碍需求
        std::vector<std::string> allergies;             // 过敏
        std::vector<std::string> medical_notes;         // 医疗备注
    } special_needs;
    
    // ========== 历史数据 ==========
    std::vector<TripRecord> trip_history;
    int total_trips = 0;
    double total_spent = 0.0;
    
    // ========== 重要记忆 ==========
    // 这些是系统从对话中提取的关键信息
    std::map<std::string, std::string> key_memories;
    // 例如：
    // "last_japan_hotel" -> "京都格兰比亚"
    // "fear_of_heights" -> "true"
    // "honeymoon_destination" -> "马尔代夫"
    
    // ========== 方法 ==========
    
    // 从记忆中提取旅行偏好
    void extract_preferences_from_memory(nuclaw::MemoryManager& memory_mgr);
    
    // 根据偏好推荐目的地
    std::vector<std::string> recommend_destinations(
        double budget,
        int days,
        const std::string& season
    ) const;
    
    // 获取类似行程参考
    std::vector<TripRecord> find_similar_trips(
        const std::string& destination,
        double budget
    ) const;
    
    // 生成用户画像摘要（用于 Prompt）
    std::string generate_profile_summary() const;
    
    // 学习用户反馈
    void learn_from_feedback(
        const std::string& trip_id,
        int rating,
        const std::string& feedback
    );
};

} // namespace travelpal
```

**关键设计决策：**

1. **继承 AgentState**：复用情感状态、记忆管理、关系追踪
2. **结构化的偏好**：用枚举和结构体而非纯文本，便于程序处理
3. **特殊需求独立**：饮食、过敏等安全相关信息单独管理
4. **关键记忆映射**：快速查找重要信息，避免每次都向量检索

---

### 2. 意图识别系统

```cpp
// include/travelpal/travel_intent.hpp
#pragma once
#include <string>
#include <vector>
#include <optional>

namespace travelpal {

// 旅行相关意图
enum class TravelIntent {
    PLAN_TRIP,           // 规划行程
    MODIFY_PLAN,         // 修改行程
    QUERY_WEATHER,       // 查询天气
    QUERY_BUDGET,        // 查询预算
    GENERATE_PACKING,    // 生成行李清单
    SHARE_MEMORY,        // 分享旅行记忆
    GET_RECOMMENDATION,  // 获取推荐
    BOOK_SERVICE,        // 预订服务
    CANCEL_BOOKING,      // 取消预订
    CHITCHAT,            // 闲聊
    UNKNOWN              // 未知
};

// 意图识别的参数提取结果
struct IntentResult {
    TravelIntent intent;
    double confidence;
    
    // 提取的参数
    struct Params {
        std::optional<std::string> destination;
        std::optional<std::string> start_date;
        std::optional<std::string> end_date;
        std::optional<double> budget;
        std::optional<int> travelers;
        std::optional<std::string> purpose;      // "honeymoon", "business"
        std::map<std::string, std::string> others;
    } params;
};

// 意图分类器
class TravelIntentClassifier {
public:
    TravelIntentClassifier(std::shared_ptr<nuclaw::LLMClient> llm)
        : llm_(llm) {}
    
    // 规则 + LLM 混合识别
    IntentResult classify(const std::string& message) {
        // 1. 快速规则匹配
        auto rule_result = rule_based_classify(message);
        if (rule_result.confidence > 0.9) {
            return rule_result;
        }
        
        // 2. LLM 识别（复杂语境）
        auto llm_result = llm_based_classify(message);
        
        // 3. 融合结果
        return merge_results(rule_result, llm_result);
    }

private:
    IntentResult rule_based_classify(const std::string& message);
    IntentResult llm_based_classify(const std::string& message);
    IntentResult merge_results(const IntentResult& rule, 
                              const IntentResult& llm);
    
    std::shared_ptr<nuclaw::LLMClient> llm_;
    
    // 规则模式
    const std::vector<std::pair<std::regex, TravelIntent>> patterns_ = {
        {std::regex(R"(想去|计划去|打算去.*玩)"), TravelIntent::PLAN_TRIP},
        {std::regex(R"(天气怎么样|会下雨吗|温度如何)"), TravelIntent::QUERY_WEATHER},
        {std::regex(R"(预算|多少钱|花费)"), TravelIntent::QUERY_BUDGET},
        {std::regex(R"(带什么|行李|准备.*东西)"), TravelIntent::GENERATE_PACKING},
        {std::regex(R"(上次.*去了|记得.*旅行)"), TravelIntent::SHARE_MEMORY},
    };
};

} // namespace travelpal
```

---

### 3. 行程规划器

```cpp
// include/travelpal/trip_planner.hpp
#pragma once
#include "travel_profile.hpp"
#include <vector>
#include <memory>

namespace travelpal {

// 活动类型
enum class ActivityType {
    SIGHTSEEING,    // 观光
    DINING,         // 用餐
    SHOPPING,       // 购物
    TRANSPORT,      // 交通
    RELAXATION,     // 休息
    ENTERTAINMENT   // 娱乐
};

// 单个活动
struct Activity {
    std::string id;
    std::string name;
    std::string description;
    ActivityType type;
    std::string location;
    std::string start_time;    // "09:00"
    std::string end_time;      // "11:00"
    double cost = 0.0;
    int energy_cost = 3;       // 1-5 体力消耗
    std::vector<std::string> tags;  // "必去", "网红", "本地"
    std::string booking_url;
    std::string notes;
};

// 一天的行程
struct DayPlan {
    int day_number;
    std::string date;
    std::vector<Activity> activities;
    std::string accommodation;
    double daily_budget;
    std::string transport_card;  // 当日交通卡建议
    std::vector<std::string> tips;  // 当日小贴士
};

// 完整行程
struct TripPlan {
    std::string plan_id;
    std::string user_id;
    
    // 基本信息
    std::string destination;
    std::string start_date;
    std::string end_date;
    int total_days;
    
    // 预算
    struct Budget {
        double total;
        double accommodation;
        double transport;
        double food;
        double activities;
        double shopping;
        double emergency;  // 应急资金
    } budget;
    
    // 行程
    std::vector<DayPlan> itinerary;
    
    // 总体信息
    std::vector<std::string> highlights;   // 行程亮点
    std::vector<std::string> packing_list; // 建议携带物品
    std::vector<std::string> documents;    // 必备证件
    std::vector<std::string> warnings;    // 注意事项
    
    // 元数据
    std::string created_at;
    std::string status;  // "draft", "confirmed", "completed"
    int user_rating = 0; // 行程结束后用户评分
};

// 行程规划请求
struct TripRequest {
    std::string user_id;
    std::string destination;       // 可选，未指定则推荐
    std::string start_date;
    std::string end_date;
    int days;                      // 或直接指定天数
    double budget;                 // 总预算
    int travelers = 1;
    std::string purpose;
    std::vector<std::string> must_see;      // 必去景点
    std::vector<std::string> avoid;        // 避开
    std::map<std::string, std::string> preferences;
};

// 行程规划器
class TripPlanner {
public:
    TripPlanner(
        std::shared_ptr<TravelerProfile> profile,
        std::shared_ptr<ToolExecutor> tools
    ) : profile_(profile), tools_(tools) {}
    
    // 规划完整行程
    async::Task<TripPlan> plan_trip(const TripRequest& request) {
        TripPlan plan;
        plan.plan_id = generate_uuid();
        plan.user_id = request.user_id;
        
        // 1. 确定目的地（如果未指定）
        if (request.destination.empty()) {
            plan.destination = co_await recommend_destination(request);
        } else {
            plan.destination = request.destination;
        }
        
        // 2. 查询目的地信息
        auto destination_info = co_await query_destination_info(plan.destination);
        
        // 3. 分配预算
        plan.budget = allocate_budget(request.budget, plan.total_days);
        
        // 4. 生成每日行程
        for (int day = 1; day <= request.days; ++day) {
            DayPlan day_plan;
            day_plan.day_number = day;
            day_plan.date = calculate_date(request.start_date, day - 1);
            day_plan.daily_budget = plan.budget.total / request.days;
            
            // 根据用户偏好选择活动
            day_plan.activities = co_await plan_day_activities(
                plan.destination,
                day,
                request.days,
                profile_->activity_prefs,
                day_plan.daily_budget
            );
            
            // 生成当日贴士
            day_plan.tips = generate_day_tips(day_plan, destination_info);
            
            plan.itinerary.push_back(std::move(day_plan));
        }
        
        // 5. 生成整体信息
        plan.highlights = extract_highlights(plan.itinerary);
        plan.documents = generate_documents_list(plan.destination);
        plan.warnings = generate_warnings(plan.destination, profile_->special_needs);
        
        // 6. 生成行李清单
        auto weather = co_await query_weather(plan.destination, plan.start_date);
        plan.packing_list = generate_packing_list(
            plan.destination, 
            weather, 
            plan.itinerary,
            profile_->special_needs
        );
        
        co_return plan;
    }
    
    // 修改已有行程
    async::Task<TripPlan> modify_plan(
        const std::string& plan_id,
        const std::map<std::string, std::string>& modifications
    );

private:
    std::shared_ptr<TravelerProfile> profile_;
    std::shared_ptr<ToolExecutor> tools_;
    
    // 预算分配策略
    TripPlan::Budget allocate_budget(double total, int days);
    
    // 规划一天的活动
    async::Task<std::vector<Activity>> plan_day_activities(
        const std::string& destination,
        int day_number,
        int total_days,
        const ActivityPreferences& prefs,
        double daily_budget
    );
    
    // 考虑体力消耗的活动排序
    std::vector<Activity> optimize_activity_order(
        std::vector<Activity> activities,
        int expected_energy
    );
};

} // namespace travelpal
```

---

### 4. 对话引擎（整合所有组件）

```cpp
// include/travelpal/travelpal_chat.hpp
#pragma once
#include <nuclaw/personalized_chat.hpp>
#include "travel_profile.hpp"
#include "trip_planner.hpp"
#include "travel_intent.hpp"

namespace travelpal {

// 旅行小管家对话引擎
class TravelPalChat : public nuclaw::PersonalizedChatEngine {
public:
    TravelPalChat(
        std::shared_ptr<nuclaw::LLMClient> llm,
        std::shared_ptr<nuclaw::MemoryManager> memory_mgr,
        std::shared_ptr<ToolExecutor> tools
    ) : PersonalizedChatEngine(llm, memory_mgr, tools),
        intent_classifier_(llm) {}
    
    // 初始化用户（首次对话或加载已有用户）
    void initialize_user(const std::string& user_id, 
                        const std::string& user_name) {
        current_user_id_ = user_id;
        
        // 加载或创建用户画像
        profile_ = load_or_create_profile(user_id, user_name);
        
        // 创建专属的规划器
        planner_ = std::make_shared<TripPlanner>(profile_, tools_);
        
        // 从长期记忆提取偏好
        profile_->extract_preferences_from_memory(*memory_mgr_);
    }
    
    // 处理用户消息
    async::Task<std::string> chat(
        const std::string& user_id,
        const std::string& user_name,
        const std::string& message
    ) override {
        // 确保用户已初始化
        if (user_id != current_user_id_) {
            initialize_user(user_id, user_name);
        }
        
        // 1. 识别意图
        auto intent_result = intent_classifier_.classify(message);
        
        // 2. 存储用户消息到短期记忆
        memory_mgr_->add_short_term(*profile_, {
            .content = message,
            .category = "user_message",
            .timestamp = now()
        });
        
        std::string response;
        
        // 3. 根据意图处理
        switch (intent_result.intent) {
            case TravelIntent::PLAN_TRIP:
                response = co_await handle_plan_trip(intent_result.params);
                break;
                
            case TravelIntent::QUERY_WEATHER:
                response = co_await handle_query_weather(intent_result.params);
                break;
                
            case TravelIntent::QUERY_BUDGET:
                response = co_await handle_query_budget(intent_result.params);
                break;
                
            case TravelIntent::GENERATE_PACKING:
                response = co_await handle_generate_packing(intent_result.params);
                break;
                
            case TravelIntent::SHARE_MEMORY:
                response = handle_share_memory(message);
                break;
                
            case TravelIntent::CHITCHAT:
                response = co_await handle_chitchat(message);
                break;
                
            default:
                response = co_await PersonalizedChatEngine::chat(
                    user_id, user_name, message
                );
        }
        
        // 4. 存储回复到短期记忆
        memory_mgr_->add_short_term(*profile_, {
            .content = response,
            .category = "assistant_response",
            .timestamp = now()
        });
        
        // 5. 根据对话提取新的记忆
        extract_memories_from_conversation(message, response);
        
        co_return response;
    }

private:
    std::string current_user_id_;
    std::shared_ptr<TravelerProfile> profile_;
    std::shared_ptr<TripPlanner> planner_;
    TravelIntentClassifier intent_classifier_;
    
    // 处理器方法
    async::Task<std::string> handle_plan_trip(
        const IntentResult::Params& params
    );
    
    async::Task<std::string> handle_query_weather(
        const IntentResult::Params& params
    );
    
    async::Task<std::string> handle_query_budget(
        const IntentResult::Params& params
    );
    
    async::Task<std::string> handle_generate_packing(
        const IntentResult::Params& params
    );
    
    std::string handle_share_memory(const std::string& message);
    
    async::Task<std::string> handle_chitchat(const std::string& message);
    
    // 从对话中提取记忆
    void extract_memories_from_conversation(
        const std::string& user_msg,
        const std::string& assistant_msg
    ) {
        // 使用 LLM 提取关键信息
        auto prompt = fmt::format(R"(
从以下对话中提取旅行相关的关键记忆：

用户：{}
助手：{}

请提取以下类型的信息（JSON 格式）：
- 旅行偏好（目的地、风格、预算）
- 特殊需求（过敏、身体状况）
- 重要事件（蜜月、周年纪念）
- 过往经历（上次住的酒店、喜欢的餐厅）

如果没有可提取的信息，返回空 JSON。
)", user_msg, assistant_msg);
        
        auto result = llm_->complete(prompt).get();
        
        // 解析并存储到关键记忆
        try {
            auto memories = nlohmann::json::parse(result);
            for (auto& [key, value] : memories.items()) {
                profile_->key_memories[key] = value.get<std::string>();
                
                // 同时存入长期记忆
                memory_mgr_->store_long_term(profile_->agent_id, user_id_, {
                    .id = generate_uuid(),
                    .content = fmt::format("{}: {}", key, value.get<std::string>()),
                    .category = "preference",
                    .importance = 8.0f
                });
            }
        } catch (...) {
            // 提取失败，不影响主流程
        }
    }
};

} // namespace travelpal
```

---

## 💬 完整交互示例

### 场景 1：首次对话（冷启动）

```
用户：你好，我想去日本玩

小旅：你好呀！👋 很高兴为你规划旅行！

日本是个好选择！樱花季快来了呢 🌸

为了给你推荐最适合的行程，我想了解几个问题：

1️⃣ 你打算什么时候去？去几天？
2️⃣ 预算是多少？（人均）
3️⃣ 你更喜欢哪种风格？
   • A. 打卡景点，行程紧凑
   • B. 悠闲体验，深度游览
   • C. 美食为主，走到哪吃到哪
   • D. 购物血拼，买买买

另外，如果有特殊需求（比如饮食限制、过敏等）也请告诉我哦！
```

### 场景 2：有记忆后的对话

```
用户：我想去日本

小旅：日本！✨ 好选择！我记得你之前去过东京，
住的是新宿的格拉斯丽酒店，那次是商务出行对吧？

这次想去关西地区吗？那边有京都、大阪、奈良...
和你上次去的东京感觉很不一样！

你更喜欢：
1️⃣ 关西深度游（京都-大阪-奈良）- 古都风情
2️⃣ 北海道雪景 - 适合 12-2 月
3️⃣ 冲绳海岛 - 潜水+沙滩
4️⃣ 继续探索东京周边 - 轻井泽、箱根

（我是根据你之前说过喜欢历史古迹推荐的选项 1 😊）

---

用户：预算 1 万，5 天

小旅：收到！1 万预算 5 天关西，完全没问题！

让我根据你的偏好规划一下...
（我知道你爱吃美食、喜欢悠闲的行程，
所以每天安排 2-3 个景点，留出用餐时间）

🗓️ 行程建议：
Day 1: 抵达大阪，道顿堀美食
   • 推荐住宿：难波区域（交通方便，美食集中）
   • 晚餐：蟹道乐（记得提前预订！）

Day 2: 大阪城 → 京都（清水寺夜樱）
   • 上午：大阪城天守阁
   • 下午：前往京都
   • 晚上：清水寺夜樱（3 月底限定！）

Day 3: 京都和服体验 → 岚山竹林
   • 上午：和服租赁+伏见稻荷大社
   • 下午：岚山竹林（你之前提过想拍竹林照片 📸）
   • 晚餐：岚山豆腐料理

Day 4: 奈良看鹿 → 返回大阪
   • 上午：奈良公园（喂小鹿）
   • 下午：东大寺
   • 晚上：回到大阪心斋桥购物

Day 5: 黑门市场早餐 → 返程
   • 上午：黑门市场海鲜早餐
   • 根据航班时间灵活安排

💰 预算明细：
• 机票：3500（知道你偏好直飞，选了东航）
• 住宿：2500（民宿风格，类似你之前给好评的那种）
• 餐饮：2000（预留了 400/天美食预算）
• 交通：800（JR Pass 关西券）
• 门票+和服：1200
• 应急：300
总计：10300（超了一点，但餐饮可以灵活调整）

🎒 行李清单（基于 3 月天气，我看了预报）：
☑️ 春装外套（早晚温差大，10-18°C）
☑️ 舒适的 walking shoes（你脚伤刚恢复，别穿高跟鞋）
☑️ 相机+三脚架（你说要拍樱花夜景）
☑️ 便携雨伞（3 月是雨季）
☑️ 充电宝（拍照多，耗电快）
☑️ 胃药（你肠胃比较敏感，备用）

有什么想调整的吗？或者我帮你预订酒店？
```

### 场景 3：长期记忆的应用

```
用户：我去年蜜月去的马尔代夫，还想再去一次

小旅：啊！你们的蜜月！💕 我记得你提过那是 "人生中最浪漫的 7 天"，
住的是水上屋，每天醒来就是大海...

这次是什么特别的日子吗？结婚纪念日？

（如果是纪念日，我可以帮你安排类似的惊喜哦～
上次你说妻子特别喜欢日落巡航，这次要不要加上海滩晚餐？）
```

---

## 🧪 测试用例

```cpp
// tests/test_travelpal.cpp
#include <gtest/gtest.h>
#include <travelpal/travelpal_chat.hpp>

TEST(TravelPalTest, FirstTimeUser) {
    auto chat = create_test_chat_engine();
    
    auto response = chat.chat("user001", "小明", "我想去日本").get();
    
    // 应该询问详细信息
    EXPECT_TRUE(response.contains("什么时候") || response.contains("预算"));
}

TEST(TravelPalTest, RememberPreferences) {
    auto chat = create_test_chat_engine();
    
    // 第一次对话建立偏好
    chat.chat("user002", "小红", "我喜欢吃美食，不喜欢赶景点").get();
    
    // 第二次对话应该记住
    auto response = chat.chat("user002", "小红", "推荐个目的地").get();
    
    EXPECT_TRUE(response.contains("美食") || response.contains("吃"));
}

TEST(TravelPalTest, PlanTrip) {
    auto chat = create_test_chat_engine();
    chat.initialize_user("user003", "小李");
    
    auto response = chat.chat(
        "user003", "小李", 
        "我想去日本，预算 1 万，5 天"
    ).get();
    
    // 应该返回完整行程
    EXPECT_TRUE(response.contains("Day 1") || response.contains("行程"));
    EXPECT_TRUE(response.contains("预算"));
}

TEST(TravelPalTest, ExtractSpecialNeeds) {
    auto chat = create_test_chat_engine();
    
    chat.chat("user004", "小张", "我对海鲜过敏").get();
    
    // 应该提取到过敏信息
    auto profile = chat.get_profile("user004");
    EXPECT_TRUE(profile->special_needs.allergies.contains("海鲜"));
}
```

---

## 📝 实现步骤

1. **复制 Step 16 代码**
   ```bash
   cp -r step16 step17
   ```

2. **创建旅行助手模块**
   ```bash
   mkdir -p step17/include/travelpal
   touch step17/include/travelpal/{travel_profile,trip_planner,travel_intent,travelpal_chat}.hpp
   ```

3. **实现 TravelerProfile**（继承 AgentState）

4. **实现 TripPlanner**（使用 ToolExecutor 调用天气、景点 API）

5. **实现 TravelIntentClassifier**（规则 + LLM 混合）

6. **实现 TravelPalChat**（整合所有组件）

7. **编写测试用例**

8. **添加示例配置**

---

## 🎓 学习要点

| 知识点 | 说明 |
|:---|:---|
| **领域建模** | 如何将"旅行"业务抽象为代码结构 |
| **意图识别** | 规则 vs LLM，何时用哪个 |
| **Prompt 工程** | 如何引导 LLM 生成结构化行程 |
| **记忆应用** | 何时检索记忆、如何融入回复 |
| **情感设计** | 让 AI 回复有"人味"的技巧 |

---

## 📚 延伸阅读

- [RASA 对话系统](https://rasa.com/) - 意图识别和对话管理
- [LangChain Memory](https://python.langchain.com/docs/modules/memory/) - 记忆管理设计
- [TripIt API](https://www.tripit.com/developer) - 行程数据格式参考

---

**恭喜！你完成了第一个完整的 AI 应用！** 🎉

在 Step 18，我们将挑战更复杂的场景：**多个 Agent 在同一个虚拟空间中互动**。
