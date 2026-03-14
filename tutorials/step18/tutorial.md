# Step 18: 虚拟咖啡厅「星语轩」

> **目标**：基于 Step 17 的单 Agent 应用，扩展到多 Agent 协作的虚拟世界
> 
> **难度**：⭐⭐⭐⭐⭐ (毕业项目)
> 
> **代码量**：约 1500 行
> 
> **预计时间**：6-8 小时

!!! tip "快速开始"
    如果你想先跑起来再说，查看 [**30 分钟快速上手指南**](quickstart.md)，
    实现一个最简版的多 NPC 虚拟咖啡厅！

---

## 🎯 项目概述

### 从单 Agent 到多 Agent

Step 17 我们实现了一个**有记忆、有情感**的旅行助手。但现实世界不是一对一的对话——

**「星语轩」是一个虚拟咖啡厅，里面有多个 NPC，他们有自己的生活、情感、关系。**

你可以：
- ☕ 点一杯咖啡，听他们聊天
- 👂 偷听 NPC 之间的对话
- 💬 参与他们的故事
- 🎭 影响他们之间的关系

### 核心挑战

| 挑战 | 单 Agent (Step 17) | 多 Agent (Step 18) |
|:---|:---|:---|
| 记忆管理 | 一个用户的记忆 | 多个 NPC × 多个对象的记忆 |
| 决策逻辑 | 响应用户输入 | 自主决策 + 环境感知 |
| 情感计算 | 单向（对用户） | 网状（NPC 之间也有情感） |
| 时序推进 | 用户驱动 | 时间自动流逝，NPC 自主行动 |

---

## 📖 故事背景

### 欢迎来到星语轩

> 这是一家藏在城市角落的小咖啡厅。
> 木质招牌上刻着「星语轩」三个字，
> 推开门，咖啡香混着爵士乐飘出来...

### NPC 角色设定

#### 老王 👨‍🍳（咖啡师，42岁）
```yaml
性格: 沉默寡言，手艺精湛，内心温暖
背景: 做了20年咖啡，曾在大城市打拼，现在回归小城
秘密: 暗恋林阿姨，但说不出口
日程: 6:00-22:00 在店里
特殊技能: 能记住每个熟客的口味
台词:
  - "...要还是老样子？"
  - "（沉默地擦杯子，但嘴角带着笑）"
```

#### 小雨 🎨（插画师，26岁）
```yaml
性格: 活泼开朗，有点迷糊，充满创意
背景: 自由插画师，远程工作
日常: 在窗边座位画画，偶尔接商稿
秘密: 暗恋阿杰，但假装只是朋友
爱好: 甜食、猫咪、雨天
台词:
  - "老王！今天的特调是什么～"
  - "阿杰阿杰，帮我看看这个颜色搭配！"
```

#### 阿杰 💻（程序员，30岁）
```yaml
性格: 技术宅，内向，但一聊技术就停不下来
背景: 某大厂程序员，远程办公
日常: 角落座位，两台显示器，一杯美式
秘密: 暗恋小雨，但不敢表白
特点: 会修电脑，被小雨当工具人但心甘情愿
台词:
  - "...这个需求做不了。"
  - "（看到小雨过来，默默把游戏窗口关掉）"
```

#### 林阿姨 📚（退休教师，58岁）
```yaml
性格: 温暖，健谈，喜欢撮合年轻人
背景: 退休语文教师，独居
日常: 下午来店里看书，和年轻人聊天
特殊: 像大家的长辈，会做饼干带来
观察: 早就看出小雨和阿杰互相喜欢
台词:
  - "年轻人啊，要勇敢一点。"
  - "老王，这杯我请，你别老不收我钱。"
```

#### 橘子 🐱（店猫，3岁）
```yaml
性格: 高冷，但会撒娇
日常: 在店里巡逻，睡在各种奇怪的地方
互动: 谁有猫条跟谁好
特殊: 似乎能感知人的情绪
```

### 关系网络

```
        ┌──────────┐
        │   橘子   │
        └────┬─────┘
             │（被所有人喜欢）
    ┌────────┼────────┐
    │        │        │
    ▼        ▼        ▼
┌──────┐  ┌──────┐  ┌──────┐
│ 老王 │  │ 小雨 │  │ 阿杰 │
└──┬───┘  └──┬───┘  └──┬───┘
   │暗恋     │双向暗恋  │
   │         ▼          │
   │      ┌──────┐      │
   └─────→│林阿姨│←─────┘
          └──────┘
         （撮合者）
```

---

## 📐 系统架构

```
┌──────────────────────────────────────────────────────────────────────────┐
│                         星语轩虚拟咖啡厅                                  │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│   ┌──────────────────────────────────────────────────────────────────┐  │
│   │                        WorldEngine                                │  │
│   │  ┌────────────────────────────────────────────────────────────┐ │  │
│   │  │                      TimeSystem                             │ │  │
│   │  │  • 游戏时间推进    • 日夜循环    • 特殊日期触发事件        │ │  │
│   │  └────────────────────────────────────────────────────────────┘ │  │
│   │  ┌────────────────────────────────────────────────────────────┐ │  │
│   │  │                    WeatherSystem                            │ │  │
│   │  │  • 天气变化        • 影响 NPC 心情    • 触发特殊事件       │ │  │
│   │  └────────────────────────────────────────────────────────────┘ │  │
│   │  ┌────────────────────────────────────────────────────────────┐ │  │
│   │  │                    SpaceManager                             │ │  │
│   │  │  • 店内布局        • NPC 位置追踪    • 接近检测            │ │  │
│   │  └────────────────────────────────────────────────────────────┘ │  │
│   │  ┌────────────────────────────────────────────────────────────┐ │  │
│   │  │                   EventScheduler                            │ │  │
│   │  │  • 定时事件        • 随机事件        • 故事线触发          │ │  │
│   │  └────────────────────────────────────────────────────────────┘ │  │
│   └──────────────────────────────────────────────────────────────────┘  │
│                                    │                                     │
│   ┌────────────────────────────────┼────────────────────────────────┐   │
│   │                                │                                │   │
│   ▼                                ▼                                ▼   │
│ ┌──────────┐                ┌──────────┐                ┌──────────┐   │
│ │  老王    │◄──────────────►│  小雨    │◄──────────────►│  阿杰    │   │
│ │ NPCAgent │                │ NPCAgent │                │ NPCAgent │   │
│ │          │                │          │                │          │   │
│ │ • 个性   │                │ • 个性   │                │ • 个性   │   │
│ │ • 记忆   │                │ • 记忆   │                │ • 记忆   │   │
│ │ • 情感   │                │ • 情感   │                │ • 情感   │   │
│ │ • 日程   │                │ • 日程   │                │ • 日程   │   │
│ └────┬─────┘                └────┬─────┘                └────┬─────┘   │
│      │                          │                          │          │
│      └──────────────────────────┼──────────────────────────┘          │
│                                 │                                      │
│                          ┌──────┴──────┐                              │
│                          │   林阿姨    │                              │
│                          │  NPCAgent   │                              │
│                          └─────────────┘                              │
│                                                                          │
│   ┌──────────────────────────────────────────────────────────────────┐  │
│   │                    SocialEventSystem                              │  │
│   │  • NPC 间自发对话    • 关系变化计算    • 故事线推进              │  │
│   │  • 群体事件（生日会）• 冲突处理        • 表白/和好等特殊事件     │  │
│   └──────────────────────────────────────────────────────────────────┘  │
│                                                                          │
│   ┌──────────────────────────────────────────────────────────────────┐  │
│   │                      PlayerInterface                              │  │
│   │  • 观察世界        • 与 NPC 对话       • 影响事件走向            │  │
│   │  • 点单/购买       • 触发特殊互动                               │  │
│   └──────────────────────────────────────────────────────────────────┘  │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## 🗂️ 项目结构

```
step18/
├── CMakeLists.txt
├── README.md
├── configs/
│   ├── world_config.yaml      # 世界配置
│   └── npcs/                  # NPC 配置
│       ├── laowang.yaml
│       ├── xiaoyu.yaml
│       ├── ajie.yaml
│       └── linaiyi.yaml
├── include/
│   ├── starcafe/              # 咖啡厅命名空间
│   │   ├── world_engine.hpp
│   │   ├── time_system.hpp
│   │   ├── weather_system.hpp
│   │   ├── space_manager.hpp
│   │   ├── event_scheduler.hpp
│   │   ├── social_event_system.hpp
│   │   ├── npc_agent.hpp
│   │   └── player_interface.hpp
│   └── npcs/                  # NPC 具体实现
│       ├── laowang.hpp
│       ├── xiaoyu.hpp
│       ├── ajie.hpp
│       └── linaiyi.hpp
├── src/
│   ├── world_engine.cpp
│   ├── npc_agent.cpp
│   └── main.cpp
└── tests/
    └── test_starcafe.cpp
```

---

## 📝 核心实现详解

### 1. NPC Agent（继承 Step 17）

```cpp
// include/starcafe/npc_agent.hpp
#pragma once
#include <travelpal/travel_profile.hpp>
#include <map>
#include <vector>
#include <functional>

namespace starcafe {

// 前置声明
class WorldEngine;
class SocialEvent;

// NPC 之间的关系
struct Relationship {
    std::string target_npc_id;
    float trust = 5.0f;           // 信任度 0-10
    float affinity = 5.0f;        // 好感度 0-10（负数=讨厌）
    float familiarity = 0.0f;     // 熟悉度（见面次数）
    std::string feeling_type;     // "friend", "crush", "mentor", "family", "rival"
    
    // 共享记忆
    std::vector<std::string> shared_memories;
    
    // 最后一次互动
    std::string last_interaction;
    std::chrono::system_clock::time_point last_interaction_time;
    
    // 关系变化趋势
    float trend = 0.0f;  // 正数=关系升温，负数=降温
};

// NPC 的当前状态
struct NPCState {
    std::string location;         // "counter", "window_seat", "kitchen", "out"
    std::string activity;         // "working", "relaxing", "talking", "eating"
    std::string mood;             // "happy", "neutral", "sad", "excited", "tired"
    float energy = 8.0f;          // 体力 0-10
    float hunger = 3.0f;          // 饥饿度 0-10
    std::vector<std::string> current_goals;  // 当前目标
};

// NPC 的日程
struct Schedule {
    struct TimeSlot {
        int hour;                 // 0-23
        int minute;
        std::string location;
        std::string activity;
        float probability;        // 发生概率（不是每天都一样）
    };
    std::vector<TimeSlot> daily_schedule;
    std::vector<std::string> days_off;  // 休息日
};

// NPC Agent - 继承 Step 17 的 TravelerProfile
class NPCAgent : public travelpal::TravelerProfile {
public:
    NPCAgent(const std::string& id, const std::string& name);
    
    // ========== NPC 特有属性 ==========
    std::string npc_id;
    std::string npc_name;
    std::string role_description;  // "咖啡师", "插画师"...
    
    NPCState state;
    Schedule schedule;
    
    // 与其他 NPC 的关系
    std::map<std::string, Relationship> relationships;
    
    // 对世界的观察
    std::vector<std::string> observed_events;  // 最近观察到的事件
    
    // ========== 核心方法 ==========
    
    // 每 tick 被调用，决定下一步行动
    virtual Action decide_action(WorldEngine& world) = 0;
    
    // 感知环境变化
    void perceive(const std::string& event_type, 
                  const std::map<std::string, std::string>& details);
    
    // 与另一个 NPC 互动
    virtual async::Task<InteractionResult> interact_with(
        NPCAgent& other,
        const std::string& context
    );
    
    // 对玩家说话
    virtual async::Task<std::string> talk_to_player(
        const std::string& player_message
    );
    
    // 自主对话（NPC 之间）
    virtual async::Task<std::string> initiate_conversation(
        NPCAgent& target,
        const std::string& topic
    );
    
    // 更新关系
    void update_relationship(
        const std::string& target_id,
        float affinity_delta,
        const std::string& memory
    );
    
    // 情感变化
    void update_emotion(const std::string& event, float intensity);

protected:
    // 工具方法
    bool is_at_location(const std::string& loc) const;
    bool is_near(const NPCAgent& other) const;
    std::vector<NPCAgent*> get_nearby_npcs(WorldEngine& world);
    
    // 决策辅助
    std::string select_conversation_topic(const NPCAgent& target);
    bool should_initiate_conversation(const NPCAgent& target);
    float calculate_social_desire() const;  // 社交欲望
};

// 行动类型
struct Action {
    enum Type {
        MOVE,               // 移动
        DO_ACTIVITY,        // 做某事
        INITIATE_CHAT,      // 发起对话
        RESPOND_TO_CHAT,    // 回应对话
        USE_ITEM,           // 使用物品
        LEAVE,              // 离开
        IDLE                // 空闲
    } type;
    
    std::map<std::string, std::string> params;
};

// 互动结果
struct InteractionResult {
    std::string dialogue;         // 说的话
    std::string emotion_change;   // 情感变化
    std::map<std::string, float> relationship_changes;
    std::vector<std::string> new_memories;
};

} // namespace starcafe
```

---

### 2. 具体 NPC 实现（老王示例）

```cpp
// include/npcs/laowang.hpp
#pragma once
#include <starcafe/npc_agent.hpp>

namespace starcafe {

class Laowang : public NPCAgent {
public:
    Laowang() : NPCAgent("laowang", "老王") {
        // ========== 基础设定 ==========
        personality.name = "老王";
        personality.role = "咖啡师";
        personality.traits = {"沉默寡言", "手艺精湛", "内心温暖", "不善表达"};
        
        // ========== 偏好（复用 Step 17 结构）==========
        travel_prefs.interests = {"咖啡", "爵士乐", "雨天", "老电影"};
        travel_prefs.preferred_pace = 2;  // 慢节奏
        
        // ========== 日程 ==========
        schedule.daily_schedule = {
            {6, 0, "kitchen", "准备开店", 1.0},
            {7, 0, "counter", "做早餐咖啡", 1.0},
            {12, 0, "kitchen", "午餐休息", 1.0},
            {14, 0, "counter", "下午营业", 1.0},
            {18, 0, "kitchen", "晚餐", 1.0},
            {20, 0, "counter", "晚间营业", 1.0},
            {22, 0, "out", "关店回家", 1.0}
        };
        
        // ========== 初始关系 ==========
        // 对林阿姨有好感（但还没意识到是暗恋）
        relationships["linaiyi"] = {
            .target_npc_id = "linaiyi",
            .trust = 8.0f,
            .affinity = 7.0f,
            .familiarity = 5.0f,
            .feeling_type = "friend",  // 自己以为是朋友
            .shared_memories = {"去年中秋一起做月饼", "她推荐的那本书"}
        };
        
        // 把小雨当妹妹看
        relationships["xiaoyu"] = {
            .target_npc_id = "xiaoyu",
            .trust = 7.0f,
            .affinity = 6.0f,
            .familiarity = 4.0f,
            .feeling_type = "family"
        };
        
        // 和阿杰交流不多
        relationships["ajie"] = {
            .target_npc_id = "ajie",
            .trust = 5.0f,
            .affinity = 5.0f,
            .familiarity = 2.0f,
            .feeling_type = "acquaintance"
        };
    }
    
    // ========== 决策逻辑 ==========
    Action decide_action(WorldEngine& world) override {
        auto now = world.get_time();
        
        // 1. 检查是否有紧急事件
        if (has_urgent_task()) {
            return handle_urgent_task();
        }
        
        // 2. 检查是否有玩家点单
        if (auto customer = world.get_waiting_customer_at("counter")) {
            return Action{
                .type = Action::INITIATE_CHAT,
                .params = {
                    {"target", customer->id()},
                    {"topic", "take_order"},
                    {"tone", "professional_but_warm"}
                }
            };
        }
        
        // 3. 检查林阿姨是否在
        auto linaiyi = world.get_npc("linaiyi");
        if (linaiyi && is_near(*linaiyi)) {
            auto& rel = relationships["linaiyi"];
            
            // 如果心情好，可能会主动搭话
            if (emotion.happiness > 5 && calculate_social_desire() > 0.6) {
                return Action{
                    .type = Action::INITIATE_CHAT,
                    .params = {
                        {"target", "linaiyi"},
                        {"topic", select_topic_for_linaiyi()},
                        {"tone", "gentle"}
                    }
                };
            }
            
            // 偷偷看她（增加熟悉度，但不说话）
            if (rel.familiarity > 3 && rel.familiarity < 7) {
                observe_linaiyi_quietly();
            }
        }
        
        // 4. 观察小雨和阿杰
        auto xiaoyu = world.get_npc("xiaoyu");
        auto ajie = world.get_npc("ajie");
        if (xiaoyu && ajie && is_near(*xiaoyu) && is_near(*ajie)) {
            // 如果他们在一起，老王会微笑
            if (xiaoyu->is_talking_with("ajie")) {
                emotion.happiness += 0.5f;  // 看他们互动很开心
                return Action{
                    .type = Action::DO_ACTIVITY,
                    .params = {{"activity", "smile_while_working"}}
                };
            }
        }
        
        // 5. 按日程执行
        return follow_schedule(now);
    }
    
    // ========== 对话生成 ==========
    async::Task<std::string> initiate_conversation(
        NPCAgent& target,
        const std::string& topic
    ) override {
        if (target.npc_id == "linaiyi") {
            return generate_dialogue_for_linaiyi(topic);
        }
        
        // 对其他人话很少
        co_return pick_one({
            "...",
            "嗯。",
            "（点头）",
            "要喝点什么？"
        });
    }
    
    async::Task<std::string> talk_to_player(
        const std::string& player_message
    ) override {
        // 老王对熟客话会多一点
        auto familiarity = get_player_familiarity();
        
        if (familiarity > 5) {
            return generate_warm_response(player_message);
        }
        
        return generate_terse_response(player_message);
    }

private:
    std::string select_topic_for_linaiyi() {
        auto& rel = relationships["linaiyi"];
        
        if (rel.shared_memories.contains("那本书")) {
            return "那本书我看完了";
        }
        
        if (weather.is_raining()) {
            return "雨天适合听爵士";  // 共同喜好
        }
        
        return "今天的咖啡很好喝";  // 安全话题
    }
    
    async::Task<std::string> generate_dialogue_for_linaiyi(
        const std::string& topic
    ) {
        // 用 LLM 生成，但加入老王的语气词
        auto base_response = co_await llm_->complete(
            build_prompt_for_laowang(topic)
        );
        
        // 老王说话简洁，加省略号
        if (base_response.length() > 20) {
            base_response = "..." + base_response.substr(0, 20) + "...";
        }
        
        co_return base_response;
    }
};

} // namespace starcafe
```

---

### 3. 世界引擎

```cpp
// include/starcafe/world_engine.hpp
#pragma once
#include <npc_agent.hpp>
#include <time_system.hpp>
#include <weather_system.hpp>
#include <space_manager.hpp>
#include <event_scheduler.hpp>
#include <social_event_system.hpp>
#include <map>
#include <memory>

namespace starcafe {

// 世界配置
struct WorldConfig {
    std::string cafe_name = "星语轩";
    std::string location = "城市角落";
    
    struct OpeningHours {
        int open_hour = 7;
        int close_hour = 22;
    } hours;
    
    std::vector<std::string> locations = {
        "counter",      // 吧台
        "window_seat",  // 窗边座位
        "corner_seat",  // 角落座位
        "sofa_area",    // 沙发区
        "kitchen",      // 后厨
        "entrance"      // 门口
    };
};

// 世界引擎 - 管理整个咖啡厅
class WorldEngine {
public:
    WorldEngine(const WorldConfig& config);
    
    // ========== 初始化 ==========
    void initialize();
    void register_npc(std::unique_ptr<NPCAgent> npc);
    void spawn_player(const std::string& player_id);
    
    // ========== 主循环 ==========
    void run();
    void stop();
    
    // 推进一帧
    void tick();
    
    // ========== 查询接口 ==========
    
    // 获取当前时间
    GameTime get_time() const { return time_system_.get_current_time(); }
    
    // 获取当前天气
    Weather get_weather() const { return weather_system_.get_current(); }
    
    // 获取 NPC
    NPCAgent* get_npc(const std::string& npc_id);
    std::vector<NPCAgent*> get_all_npcs();
    std::vector<NPCAgent*> get_npcs_at(const std::string& location);
    
    // 获取某位置的描述
    std::string describe_location(const std::string& location) const;
    
    // 获取世界状态描述（给玩家看）
    std::string get_world_state_description() const;
    
    // ========== 玩家交互接口 ==========
    
    // 玩家观察
    std::string player_observe(const std::string& player_id);
    
    // 玩家移动
    bool player_move(const std::string& player_id, const std::string& destination);
    
    // 玩家与 NPC 对话
    async::Task<std::string> player_talk_to(
        const std::string& player_id,
        const std::string& npc_id,
        const std::string& message
    );
    
    // 玩家等待时间推进
    void player_wait(const std::string& player_id, int minutes);
    
    // ========== 事件广播 ==========
    void broadcast_event(const std::string& event_type,
                        const std::map<std::string, std::string>& details,
                        const std::string& location = "");

private:
    WorldConfig config_;
    bool running_ = false;
    
    // 子系统
    TimeSystem time_system_;
    WeatherSystem weather_system_;
    SpaceManager space_manager_;
    EventScheduler event_scheduler_;
    SocialEventSystem social_event_system_;
    
    // NPC 池
    std::map<std::string, std::unique_ptr<NPCAgent>> npcs_;
    
    // 玩家状态
    struct PlayerState {
        std::string location;
        std::string current_activity;
        std::vector<std::string> observed_events;
    };
    std::map<std::string, PlayerState> players_;
    
    // 世界事件历史
    std::vector<WorldEvent> event_history_;
    
    // 内部方法
    void update_npcs();
    void process_npc_actions();
    void check_social_events();
    void update_world_state();
};

// 世界事件
struct WorldEvent {
    std::string id;
    std::string type;
    std::map<std::string, std::string> details;
    GameTime timestamp;
    std::vector<std::string> participants;
    std::string location;
};

} // namespace starcafe
```

---

### 4. 社交事件系统

```cpp
// include/starcafe/social_event_system.hpp
#pragma once
#include <npc_agent.hpp>
#include <vector>
#include <functional>

namespace starcafe {

// 社交事件类型
enum class SocialEventType {
    CONVERSATION,       // 普通对话
    CONFESSION,         // 表白
    CONFLICT,           // 冲突
    RECONCILIATION,     // 和好
    CELEBRATION,        // 庆祝
    GOSSIP,             // 八卦
    ADVICE_SEEKING,     // 求助
    GROUP_ACTIVITY      // 群体活动
};

// 社交事件定义
struct SocialEventDef {
    std::string id;
    SocialEventType type;
    std::string name;
    std::string description;
    
    // 触发条件
    std::function<bool(WorldEngine&)> trigger_condition;
    float base_probability = 0.1f;
    
    // 参与者要求
    int min_participants = 2;
    int max_participants = 2;
    std::vector<std::string> required_roles;  // 必须有的角色类型
    
    // 前置条件
    std::vector<std::string> prerequisite_events;
    std::map<std::string, std::pair<float, float>> required_relationships;
    // 例如：{"xiaoyu-ajie", {5.0, 10.0}} 表示关系值需在 5-10 之间
};

// 正在进行的社交事件
struct ActiveSocialEvent {
    std::string event_id;
    SocialEventType type;
    std::vector<std::string> participant_ids;
    std::string current_stage;
    std::vector<std::string> dialogue_history;
    bool is_completed = false;
    std::map<std::string, std::string> outcome;
};

class SocialEventSystem {
public:
    SocialEventSystem();
    
    // 注册事件定义
    void register_event(const SocialEventDef& event_def);
    
    // 每 tick 检查触发
    void tick(WorldEngine& world);
    
    // 执行特定事件
    void execute_event(const std::string& event_id, 
                      WorldEngine& world,
                      const std::vector<std::string>& participants);
    
    // 查询正在发生的事件
    std::vector<ActiveSocialEvent*> get_active_events();
    
    // 获取可触发的事件列表（给玩家选择）
    std::vector<std::string> get_available_events_for_player(
        const std::string& player_id,
        WorldEngine& world
    );

private:
    std::vector<SocialEventDef> event_definitions_;
    std::vector<std::unique_ptr<ActiveSocialEvent>> active_events_;
    
    // 内置事件
    void register_builtin_events();
    
    // 具体事件执行
    void execute_confession(NPCAgent& from, NPCAgent& to, WorldEngine& world);
    void execute_group_conversation(const std::vector<std::string>& participants, 
                                    WorldEngine& world);
    void execute_gossip(NPCAgent& gossiper, NPCAgent& listener, 
                       const std::string& target_id);
};

} // namespace starcafe
```

---

## 💬 交互示例

### 场景 1：初次进入咖啡厅

```
> 进入星语轩

☕ 星语轩
推开门，温暖的咖啡香扑面而来。
墙上挂着复古爵士海报，黑胶唱片机正放着 Miles Davis。

现在是周三下午 2:30，天气晴朗。

你看到了：
• 老王 在吧台后面，正在认真地拉花
• 小雨 坐在窗边，面前摊着速写本，嘴里叼着笔
• 阿杰 缩在角落，笔记本屏幕的蓝光映在脸上

角落的沙发空着，窗边的位置也空着。
吧台还有一把高脚凳。

[提示：输入 "观察" 看更详细描述，或 "移动到窗边" 换位置]

---

> 坐到吧台

你走到吧台，在老王的对面坐下。

老王抬眼看了你一下，点点头：
"...要喝点什么？"

菜单：
1. 美式咖啡 - ¥25
2. 拿铁 - ¥32
3. 卡布奇诺 - ¥32
4. 特调：雨后 - ¥38（老王推荐）
5. 手冲：埃塞俄比亚 - ¥45

---

> 来一杯雨后

老王嘴角微微上扬：
"...好品味。"

他开始准备咖啡，动作很流畅。
蒸汽升腾中，你闻到淡淡的柑橘香。

[获得物品：雨后 × 1]
[老王对你的好感度 +0.2]
```

### 场景 2：观察 NPC 互动

```
> 等待 30 分钟

[时间推进到 15:00]

突然外面开始下大雨 🌧️
雨滴敲打着玻璃窗，声音很催眠。

老王看着窗外，手里的抹布停了一下：
"看来一时半会儿停不了。"

小雨抬起头，眼睛亮起来：
"哇——那我今天可以多画一会儿了！
阿杰阿杰，你看外面的雨！"

阿杰从屏幕前抬起头，推了推眼镜：
"...嗯，挺好的。"
（他的手指无意识地敲着键盘，没有继续敲代码）

林阿姨把书放到一边，笑着说：
"下雨天最适合聊天了。
年轻人，要不要听阿姨讲讲年轻时候的故事？"

[触发事件：雨天的长谈]
[阿杰决定今天待到很晚]
```

### 场景 3：偷听对话

```
> 听小雨和阿杰聊天

小雨："阿杰阿杰，帮我看看这个网站为什么打不开？
我要提交稿子的..."

阿杰放下手里的代码，起身走过去：
"我看看..."

他俯身看小雨的屏幕，两人挨得很近。

阿杰："哦，DNS 设置问题，你把这个改成 8.8.8.8..."

小雨："哇解决了！谢谢！你真的好厉害~
每次都要麻烦你..."

阿杰的耳朵有点红：
"小、小事一桩..."

老王：（在吧台后面看着，微微摇头笑了笑）

[关系变化：小雨对阿杰的好感度 +0.3]
[关系变化：阿杰对小雨的好感度 +0.2]
[老王观察到：阿杰喜欢小雨]
```

### 场景 4：触发特殊事件

```
> 等待到 6 点

[时间推进到 18:00]

下班时间，店里的人多了起来。

突然，一个外卖小哥冲进来：
"请问是林先生吗？您的花！"

所有人都看向门口。

老王愣在原地。

小哥把一大束向日葵放到吧台上：
"麻烦签收一下！"

林阿姨从书上抬起头，看到花，笑了：
"老王，这是...？"

老王的耳朵通红，手忙脚乱地找笔：
"...我...不知道..."

卡片上写着：「每天看到你是最开心的事 —— L」

小雨起哄："哇哦～～老王有情况！"
阿杰难得地笑了："... L 是谁啊？"

老王："......"
（他把花小心地收到吧台下面，但嘴角的笑藏不住）

[触发事件：神秘的告白]
[老王的心情：excited]
[林阿姨的心情：curious]
[老王的 key_memories 新增："有人给我送花了"]
```

---

## 🧪 测试用例

```cpp
TEST(StarCafeTest, NPCAutonomousBehavior) {
    WorldEngine world;
    
    // 注册 NPC
    world.register_npc(std::make_unique<Laowang>());
    world.register_npc(std::make_unique<Xiaoyu>());
    world.register_npc(std::make_unique<Ajie>());
    
    // 推进时间
    world.tick();
    
    // 检查 NPC 是否按日程行动
    auto laowang = world.get_npc("laowang");
    EXPECT_EQ(laowang->state.location, "counter");
}

TEST(StarCafeTest, RelationshipEvolution) {
    WorldEngine world;
    world.register_npc(std::make_unique<Xiaoyu>());
    world.register_npc(std::make_unique<Ajie>());
    
    auto xiaoyu = world.get_npc("xiaoyu");
    auto ajie = world.get_npc("ajie");
    
    float initial_affinity = xiaoyu->relationships["ajie"].affinity;
    
    // 模拟多次互动
    for (int i = 0; i < 5; ++i) {
        xiaoyu->interact_with(*ajie, "help_with_computer").get();
    }
    
    // 关系应该提升
    EXPECT_GT(xiaoyu->relationships["ajie"].affinity, initial_affinity);
}

TEST(StarCafeTest, SocialEventTrigger) {
    WorldEngine world;
    world.register_npc(std::make_unique<Laowang>());
    world.register_npc(std::make_unique<Linaiyi>());
    
    auto laowang = world.get_npc("laowang");
    laowang->relationships["linaiyi"].affinity = 8.5f;
    
    // 设置下雨天气（增加告白概率）
    world.set_weather(Weather::RAINY);
    
    // 推进多个 tick
    for (int i = 0; i < 100; ++i) {
        world.tick();
    }
    
    // 应该触发告白事件
    auto events = world.get_social_event_system().get_active_events();
    bool has_confession = false;
    for (auto* e : events) {
        if (e->type == SocialEventType::CONFESSION) {
            has_confession = true;
            break;
        }
    }
    EXPECT_TRUE(has_confession);
}
```

---

## 📝 实现步骤

1. **复制 Step 17 代码**
   ```bash
   cp -r step17 step18
   ```

2. **创建模块结构**
   ```bash
   mkdir -p step18/include/{starcafe,npcs}
   touch step18/include/starcafe/{world_engine,npc_agent,time_system,...}.hpp
   touch step18/include/npcs/{laowang,xiaoyu,ajie,linaiyi}.hpp
   ```

3. **实现 NPCAgent 基类**（继承 TravelerProfile）

4. **实现 WorldEngine**（整合所有子系统）

5. **实现具体 NPC**（每个角色独特的决策逻辑）

6. **实现 SocialEventSystem**（事件触发和执行）

7. **实现 PlayerInterface**（玩家交互）

8. **编写测试和场景脚本**

---

## 🎓 学习要点

| 知识点 | 说明 |
|:---|:---|
| **涌现行为** | 简单的规则如何产生复杂的社会现象 |
| **关系网络** | 图结构在情感计算中的应用 |
| **自主决策** | 基于目标和情感的决策树设计 |
| **叙事生成** | 如何让 AI 产生"故事感" |
| **玩家代理** | 玩家在虚拟世界中的存在感设计 |

---

**恭喜！你完成了最复杂的多 Agent 项目！** 🎉

在 Step 19，我们将把这个技术应用到**生产环境**：构建一个 SaaS 化的多租户 AI 平台。
