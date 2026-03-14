# Step 16: 期中实战 — 旅行小管家

> 目标：构建一个懂你的智能旅行规划助手
> 
> 难度：⭐⭐⭐⭐ (综合实战)
> 
> 代码量：约 1200 行

## 🌏 项目简介

**旅行小管家**是一个智能旅行规划助手，它能：

- ✈️ 根据你的预算、时间、喜好推荐目的地
- 🏨 查询实时天气、机票、酒店信息
- 🗺️ 生成个性化行程（景点、餐厅、交通）
- 📝 自动创建行李清单和备忘提醒
- 💬 像朋友一样聊天，记得你的旅行偏好

### 使用场景

```
你：我想下个月去日本看樱花，预算 1 万，5 天时间

小管家：🌸 樱花季！好选择！让我帮你规划一下...

✈️ 推荐路线：上海 → 大阪 → 京都 → 东京
📅 最佳时间：3 月底 - 4 月初
💰 预算分配：
   - 机票：3000-4000
   - 住宿：2500（民宿/胶囊旅馆）
   - 交通：800（JR Pass）
   - 餐饮：2000
   - 门票购物：1700

🗓️ 行程建议：
Day 1: 抵达大阪，道顿堀美食
Day 2: 大阪城 → 京都（清水寺夜樱）
Day 3: 京都和服体验 → 岚山竹林
Day 4: 新干线到东京 → 新宿御苑
Day 5: 浅草寺 → 返程

需要我帮你查询具体日期的机票和酒店吗？
```

## 🏗️ 系统架构

```
┌─────────────────────────────────────────────────────────────────────┐
│                        旅行小管家 TravelPal                         │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  ┌──────────────┐     ┌──────────────┐     ┌──────────────┐        │
│  │   用户对话    │────▶│  意图理解    │────▶│  记忆管理    │        │
│  │   界面       │     │  & 实体提取  │     │  (偏好/历史) │        │
│  └──────────────┘     └──────────────┘     └──────────────┘        │
│                              │                                      │
│                              ▼                                      │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │                     规划引擎 Planner                         │   │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐       │   │
│  │  │ 目的地   │ │ 预算分析 │ │ 行程生成 │ │ 清单创建 │       │   │
│  │  │ 推荐     │ │ 器       │ │ 器       │ │ 器       │       │   │
│  │  └──────────┘ └──────────┘ └──────────┘ └──────────┘       │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                              │                                      │
│                              ▼                                      │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │                      工具层 (Tools)                          │   │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐       │   │
│  │  │ 天气查询 │ │ 航班搜索 │ │ 酒店比价 │ │ 景点推荐 │       │   │
│  │  └──────────┘ └──────────┘ └──────────┘ └──────────┘       │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

## 💻 核心代码实现

### 1. 用户偏好记忆

```cpp
// 用户旅行画像
struct TravelerProfile {
    std::string user_id;
    
    // 偏好设置
    struct Preferences {
        std::vector<std::string> travel_styles;  // "背包客", "舒适型", "奢华型"
        std::vector<std::string> interests;      // "美食", "历史", "自然", "购物"
        std::vector<std::string> dietary_restrictions; // "素食", "清真", "无麸质"
        int preferred_pace = 3;  // 行程节奏 1-5 (紧凑-悠闲)
        bool prefers_early_flights = false;
    } preferences;
    
    // 历史记录
    struct History {
        std::vector<std::string> visited_destinations;
        std::vector<std::string> favorite_restaurants;
        double avg_trip_budget = 0;
        int avg_trip_duration = 0;
    } history;
    
    // 学习到的偏好（从对话中提取）
    json learned_facts;  // {"不喜欢红眼航班": true, "对海鲜过敏": true}
};

class ProfileManager {
public:
    // 从对话中提取并更新用户画像
    async::Task<void> update_from_conversation(
        const std::string& user_id,
        const std::vector<Message>& conversation) {
        
        auto prompt = fmt::format(R"(
分析以下对话，提取用户的旅行偏好信息，返回 JSON：

对话：
{}

提取规则：
1. 喜欢的旅行方式（背包客/舒适/奢华）
2. 兴趣爱好（美食/历史/自然/购物等）
3. 饮食禁忌
4. 行程节奏偏好
5. 任何重要的"不喜欢"或"必须"的事项

输出格式：
{{
    "travel_style": "...",
    "interests": ["..."],
    "dietary": ["..."],
    "pace_preference": 3,
    "important_notes": ["不喜欢红眼航班", "对海鲜过敏"]
}}
)", format_conversation(conversation));
        
        auto response = co_await llm_.complete(prompt);
        auto extracted = json::parse(response);
        
        // 合并到现有画像
        auto& profile = profiles_[user_id];
        merge_preferences(profile.preferences, extracted);
        
        // 保存到向量数据库用于长期记忆
        co_await vector_store_.upsert(
            fmt::format("profile:{}", user_id),
            extracted.dump(),
            /* metadata */ {{"user_id", user_id}, {"type", "preference"}}
        );
    }
    
    // 检索相关记忆
    async::Task<std::vector<std::string>> retrieve_relevant_memories(
        const std::string& user_id,
        const std::string& query) {
        
        return co_await vector_store_.search(
            fmt::format("profile:{}", user_id),
            query,
            5
        );
    }
};
```

### 2. 智能行程规划器

```cpp
struct TripPlan {
    std::string destination;
    DateRange dates;
    double budget;
    
    struct Day {
        int day_number;
        std::vector<Activity> activities;
        std::string accommodation;
        Transport transport;
    };
    std::vector<Day> itinerary;
    
    struct BudgetBreakdown {
        double flights;
        double accommodation;
        double food;
        double transport;
        double activities;
        double shopping;
    } budget_breakdown;
    
    std::vector<std::string> packing_list;
    std::vector<Reminder> reminders;
};

class TripPlanner {
public:
    async::Task<TripPlan> plan_trip(const TripRequest& request) {
        TripPlan plan;
        
        // 1. 如果没有明确目的地，推荐目的地
        if (request.destination.empty()) {
            plan.destination = co_await recommend_destination(request);
        } else {
            plan.destination = request.destination;
        }
        
        // 2. 查询目的地信息
        auto destination_info = co_await query_destination_info(plan.destination);
        
        // 3. 生成预算分配建议
        plan.budget_breakdown = generate_budget_breakdown(
            request.budget, 
            destination_info.cost_level,
            request.duration_days
        );
        
        // 4. 规划每日行程
        for (int day = 1; day <= request.duration_days; ++day) {
            DayPlan day_plan;
            day_plan.day_number = day;
            
            // 根据用户偏好选择景点
            auto attractions = co_await select_attractions(
                plan.destination,
                request.user_profile.preferences.interests,
                3  // 每天 3 个主要景点
            );
            
            // 安排时间
            day_plan.activities = schedule_activities(
                attractions,
                request.user_profile.preferences.preferred_pace
            );
            
            // 推荐餐厅（考虑饮食偏好）
            day_plan.activities.push_back(co_await recommend_lunch(
                plan.destination,
                day_plan.activities[1].location,  // 在第二个景点附近
                request.user_profile.preferences.dietary_restrictions
            ));
            
            plan.itinerary.push_back(std::move(day_plan));
        }
        
        // 5. 生成行李清单
        plan.packing_list = generate_packing_list(
            plan.destination,
            request.dates,
            plan.itinerary
        );
        
        // 6. 创建提醒事项
        plan.reminders = generate_reminders(plan, request.dates);
        
        co_return plan;
    }

private:
    // 目的地推荐
    async::Task<std::string> recommend_destination(const TripRequest& req) {
        auto prompt = fmt::format(R"(
根据以下条件推荐旅行目的地：

- 预算：{} 元
- 时间：{} 天
- 偏好：{}
- 季节：{}
- 已去过：{}

推荐 3 个目的地，说明理由，按匹配度排序。
)", req.budget, req.duration_days,
   fmt::join(req.user_profile.preferences.interests, ", "),
   get_season(req.dates.start),
   fmt::join(req.user_profile.history.visited_destinations, ", "));
        
        auto response = co_await llm_.complete(prompt);
        // 解析推荐结果...
        co_return parse_top_recommendation(response);
    }
    
    // 生成行李清单
    std::vector<std::string> generate_packing_list(
        const std::string& destination,
        const DateRange& dates,
        const std::vector<Day>& itinerary) {
        
        // 基础清单 + 目的地特殊物品 + 季节物品
        std::vector<std::string> base = {
            "身份证/护照", "手机充电器", "洗漱用品", "换洗衣物"
        };
        
        // 检查天气添加衣物
        auto weather = weather_service_.forecast(destination, dates);
        if (weather.avg_temp < 15) {
            base.push_back("外套/毛衣");
        }
        if (weather.rain_probability > 0.3) {
            base.push_back("雨伞/雨衣");
        }
        
        // 根据活动添加物品
        for (const auto& day : itinerary) {
            for (const auto& act : day.activities) {
                if (act.type == "hiking") {
                    base.push_back("运动鞋/登山鞋");
                }
                if (act.type == "beach") {
                    base.push_back("泳衣", "防晒霜");
                }
            }
        }
        
        // 去重
        std::sort(base.begin(), base.end());
        base.erase(std::unique(base.begin(), base.end()), base.end());
        
        return base;
    }
};
```

### 3. 工具实现

```cpp
// 天气查询工具
TOOL(weather_query, "查询目的地天气预报") {
    auto destination = ctx.get_param<std::string>("destination");
    auto date = ctx.get_param<std::string>("date");
    
    auto forecast = co_await weather_api_.get_forecast(destination, date);
    
    co_return json{
        {"location", destination},
        {"date", date},
        {"weather", forecast.condition},  // "晴", "多云", "小雨"
        {"temperature", {
            {"high", forecast.temp_high},
            {"low", forecast.temp_low}
        }},
        {"rain_probability", forecast.rain_prob},
        {"suggestion", generate_weather_advice(forecast)}
    };
}

// 航班搜索工具
TOOL(flight_search, "搜索航班") {
    auto origin = ctx.get_param<std::string>("origin");
    auto destination = ctx.get_param<std::string>("destination");
    auto date = ctx.get_param<std::string>("date");
    auto budget = ctx.get_param<double>("max_price", 5000);
    
    auto flights = co_await flight_api_.search({
        .origin = origin,
        .destination = destination,
        .departure_date = date,
        .max_price = budget
    });
    
    json results = json::array();
    for (const auto& f : flights) {
        results.push_back({
            {"airline", f.airline},
            {"flight_no", f.flight_no},
            {"departure", f.departure_time},
            {"arrival", f.arrival_time},
            {"price", f.price},
            {"duration", f.duration}
        });
    }
    
    co_return results;
}

// 景点推荐工具
TOOL(attraction_recommend, "推荐当地景点") {
    auto destination = ctx.get_param<std::string>("destination");
    auto interests = ctx.get_param<std::vector<std::string>>("interests");
    auto budget = ctx.get_param<std::string>("budget_level", "medium");
    
    // 从向量数据库检索
    auto query = fmt::format("{} {} attractions", destination, 
                            fmt::join(interests, " "));
    auto attractions = co_await vector_store_.search(
        "attractions", query, 10
    );
    
    // 过滤和排序
    std::vector<Attraction> filtered;
    for (const auto& a : attractions) {
        if (matches_budget(a, budget) && matches_interests(a, interests)) {
            filtered.push_back(a);
        }
    }
    
    co_return filtered;
}
```

### 4. 对话管理

```cpp
class TravelPalChat {
public:
    async::Task<Response> chat(const std::string& user_id, 
                               const std::string& message) {
        // 1. 加载用户画像和相关记忆
        auto profile = co_await profile_mgr_.get(user_id);
        auto memories = co_await profile_mgr_.retrieve_relevant_memories(
            user_id, message
        );
        
        // 2. 判断意图
        auto intent = co_await classify_intent(message);
        
        // 3. 根据意图处理
        switch (intent) {
            case Intent::PLAN_TRIP:
                co_return co_await handle_trip_planning(user_id, message, profile);
                
            case Intent::QUERY_INFO:
                co_return co_await handle_info_query(message);
                
            case Intent::MODIFY_PLAN:
                co_return co_await handle_plan_modification(user_id, message);
                
            case Intent::CASUAL_CHAT:
                co_return co_await handle_casual_chat(message, profile, memories);
        }
    }

private:
    async::Task<Response> handle_trip_planning(
        const std::string& user_id,
        const std::string& message,
        const TravelerProfile& profile) {
        
        // 提取需求参数
        auto params = co_await extract_trip_params(message);
        
        TripRequest req{
            .user_id = user_id,
            .user_profile = profile,
            .destination = params.destination,
            .dates = params.dates,
            .budget = params.budget,
            .duration_days = params.duration
        };
        
        // 生成行程
        auto plan = co_await planner_.plan_trip(req);
        
        // 格式化回复
        auto response = format_plan_response(plan);
        
        // 保存到用户行程列表
        co_await trip_store_.save(user_id, plan);
        
        co_return Response{
            .content = response,
            .attachments = {{
                .type = "itinerary",
                .data = plan
            }}
        };
    }
};
```

### 5. 配置文件

```yaml
# config/travelpal.yaml
travelpal:
  name: "旅行小管家"
  personality: "友好、幽默、专业"
  
  llm:
    model: gpt-4
    temperature: 0.8  # 稍微有创意一些
    
  memory:
    vector_store: "chroma"
    embedding_model: "text-embedding-3-small"
    
  tools:
    weather:
      provider: "openweathermap"
      api_key: "${WEATHER_API_KEY}"
      
    flights:
      provider: "skyscanner"  # 或携程 API
      api_key: "${FLIGHT_API_KEY}"
      
    hotels:
      provider: "booking"
      api_key: "${HOTEL_API_KEY}"
      
  knowledge_base:
    destinations: "data/destinations.json"
    attractions: "data/attractions.json"
    restaurants: "data/restaurants.json"
    
  # 预设模板
  templates:
    packing_list:
      base: ["身份证", "手机", "充电器", "洗漱用品"]
      winter: ["羽绒服", "围巾", "手套"]
      summer: ["防晒霜", "太阳镜", "泳衣"]
      business: ["正装", "名片"]
```

## 🎮 互动示例

```
用户：我想周末去周边玩，预算 1000，从上海出发

小管家：🚗 周末短途好主意！根据你的预算，推荐几个地方：

1️⃣ **莫干山**（2h 车程）
   - 民宿 + 徒步 + 农家菜
   - 预算：800-1000

2️⃣ **千岛湖**（3h 车程）
   - 湖景酒店 + 骑行
   - 预算：900-1100

3️⃣ **乌镇**（1.5h 高铁）
   - 江南水乡 + 夜景
   - 预算：600-800

你更喜欢自然风光还是人文古镇？我可以详细规划~ 🎯

---

用户：帮我选莫干山吧，我喜欢徒步

小管家：⛰️ 好的！莫干山徒步路线我来帮你安排！

📋 **莫干山 2 天 1 夜行程**

🗓️ Day 1 (周六)
- 08:00 上海自驾出发
- 10:30 到达莫干山，入住山间民宿
- 11:30 午餐：竹林鸡汤（当地特色！）
- 13:30 剑池徒步（轻量级，2h）
- 16:00 回民宿休息
- 18:00 晚餐：农家土菜
- 20:00 民宿阳台看星星 ✨

🗓️ Day 2 (周日)
- 08:00 早餐
- 09:00 大坑景区徒步（中级，3h）
- 12:30 午餐
- 14:00 返程
- 16:30 到达上海

💰 预算明细：
- 油费过路费：300
- 民宿：400
- 餐饮：200
- 门票：100
- 合计：1000 ✅

🎒 行李清单：
☑️ 身份证
☑️ 徒步鞋
☑️ 防晒霜
☑️ 薄外套（山上温差大）
☑️ 充电宝

需要我帮你查一下这周末的天气吗？☀️
```

## 📱 接入方式

```cpp
// main.cpp
int main() {
    // 初始化组件
    auto profile_mgr = std::make_shared<ProfileManager>();
    auto planner = std::make_shared<TripPlanner>();
    auto chat = std::make_shared<TravelPalChat>(profile_mgr, planner);
    
    // 启动 Web 服务
    WebServer server(8080);
    
    server.on_message([chat](const Request& req) -> Response {
        return chat->chat(req.user_id, req.message);
    });
    
    // 也可以接入微信/飞书机器人
    WechatBot wechat_bot;
    wechat_bot.on_message([chat](const std::string& user, 
                                  const std::string& msg) {
        auto reply = chat->chat(user, msg);
        return reply.content;
    });
    
    server.run();
}
```

---

## 🎯 项目亮点

1. **个性化推荐** — 基于用户画像和历史行为的智能推荐
2. **多模态规划** — 整合交通、住宿、景点、餐饮的一站式规划
3. **实用工具链** — 天气、航班、酒店等真实数据接入
4. **记忆系统** — 记住用户偏好，越用越懂你
5. **友好交互** — 像朋友一样聊天，而非冷冰冰的查询

---

## 下一步

→ **Step 17: 期末实战 — 虚拟咖啡厅 NPC 世界**

多个有个性、有记忆的 AI 角色在同一个虚拟空间中生活、互动，你可以和他们聊天、观察他们的社交关系，甚至影响故事走向！
