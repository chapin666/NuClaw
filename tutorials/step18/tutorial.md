# Step 18: 虚拟咖啡厅「星语轩」

> 目标：基于 Step 17 的单 Agent 应用，扩展到多 Agent 协作的虚拟世界
> 
> 难度：⭐⭐⭐⭐⭐ (毕业项目)
> 
> 代码量：约 1500 行

## 项目简介

**虚拟咖啡厅「星语轩」**是 Step 17 的进阶版。Step 17 我们实现了一个有记忆的旅行助手，Step 18 我们要实现**多个有记忆、有情感、有关系的 Agent 在同一个空间中互动**。

这不再是「用户-助手」的一对一对话，而是一个**多角色共存的虚拟世界**。

## 从 Step 17 到 Step 18 的演进

```
Step 17: 单 Agent 应用
    └── TravelerProfile (继承 AgentState)
        └── 记忆、情感、关系 → 服务一个用户

Step 18: 多 Agent 世界
    ├── NPC_Agent (继承 AgentState)
    │   ├── 老王：咖啡师，内向，对林阿姨有好感
    │   ├── 小雨：插画师，活泼，暗恋阿杰
    │   ├── 阿杰：程序员，技术宅，暗恋小雨
    │   └── 林阿姨：退休教师，温暖，撮合年轻人
    │
    └── NPC 之间的关系网
        └── 老王 → 林阿姨（暗恋）
        └── 小雨 ↔ 阿杰（双向暗恋）
        └── 林阿姨 → 小雨（如女儿）
```

## 核心架构

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        星语轩虚拟咖啡厅                                  │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   ┌─────────────────────────────────────────────────────────────────┐  │
│   │                      WorldState (世界状态)                       │  │
│   │  • 时间系统    • 空间管理    • 事件调度    • 全局记忆            │  │
│   └─────────────────────────────────────────────────────────────────┘  │
│                              │                                          │
│   ┌──────────────────────────┼──────────────────────────┐              │
│   │                          │                          │              │
│   ▼                          ▼                          ▼              │
│ ┌──────────────┐      ┌──────────────┐      ┌──────────────┐          │
│ │    老王      │      │    小雨      │      │    阿杰      │          │
│ │   NPCAgent   │◄────►│   NPCAgent   │◄────►│   NPCAgent   │          │
│ │              │      │              │      │              │          │
│ │ • 个性/记忆  │      │ • 个性/记忆  │      │ • 个性/记忆  │          │
│ │ • 情感状态   │      │ • 情感状态   │      │ • 情感状态   │          │
│ │ • 日程安排   │      │ • 日程安排   │      │ • 日程安排   │          │
│ └──────────────┘      └──────────────┘      └──────────────┘          │
│        ▲                       ▲                       ▲               │
│        └───────────────────────┼───────────────────────┘               │
│                                │                                       │
│                         ┌──────────────┐                               │
│                         │   林阿姨     │                               │
│                         │   NPCAgent   │                               │
│                         └──────────────┘                               │
│                                                                         │
│   ┌─────────────────────────────────────────────────────────────────┐  │
│   │                      SocialEventSystem                           │  │
│   │  • NPC 间自发对话    • 关系变化    • 随机事件    • 故事触发      │  │
│   └─────────────────────────────────────────────────────────────────┘  │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

## 核心实现

### 1. NPC Agent（继承并扩展 Step 17）

```cpp
// npc_agent.hpp
#pragma once
#include <travelpal/travel_profile.hpp>  // 复用 Step 17 的 AgentState 扩展

namespace starcafe {

// NPC 之间的关系
struct Relationship {
    std::string target_id;
    float trust = 5.0f;         // 信任度
    float affinity = 5.0f;      // 好感度
    std::string feeling;        // "friend", "crush", "mentor"
    std::vector<std::string> shared_history;
};

// NPC Agent（继承 Step 17 的 TravelerProfile，但用于角色扮演）
class NPCAgent : public travelpal::TravelerProfile {
public:
    // Step 17 的 TravelerProfile 已有：
    // - AgentState: 情感、记忆、个性
    // - TravelPreferences: 可复用为 NPC 偏好
    
    // Step 18 新增：NPC 特有属性
    struct Schedule {
        Time arrive_time;
        Time leave_time;
        std::vector<ScheduledActivity> activities;
    } schedule;
    
    std::string current_location;  // "counter", "window_seat", "kitchen"
    std::string current_activity;  // "making_coffee", "drawing", "coding"
    
    // NPC 间关系网络
    std::map<std::string, Relationship> relationships;
    
    // NPC 自主决策
    virtual Action decide_action(const WorldState& world);
    
    // NPC 间对话（不是对用户，而是对其他 NPC）
    virtual async::Task<std::string> converse_with(
        const std::string& other_npc_id,
        const std::string& message);
};

} // namespace starcafe
```

### 2. 具体 NPC 角色（使用 Step 17 的用户画像模式）

```cpp
// npcs/laowang.hpp
#pragma once
#include "../npc_agent.hpp"

namespace starcafe {

class Laowang : public NPCAgent {
public:
    Laowang() {
        // 使用 Step 17 的 personality 结构
        personality.name = "老王";
        personality.role = "咖啡师";
        personality.traits = {"沉默寡言", "手艺精湛", "内心温暖"};
        
        // 使用 Step 17 的 travel_prefs 结构存储 NPC 偏好
        //（虽然名字叫 travel_prefs，但可复用为 general preferences）
        travel_prefs.interests = {"咖啡", "爵士乐", "雨天"};
        travel_prefs.preferred_pace = 2;  // 慢节奏
        
        // 日程
        schedule.arrive_time = Time(6, 0);
        schedule.leave_time = Time(22, 0);
    }
    
    Action decide_action(const WorldState& world) override {
        auto now = world.current_time();
        
        // 检查是否有客人要点单
        auto customers = world.get_customers_at("counter");
        if (!customers.empty()) {
            return Action{
                .type = ActionType::MAKE_COFFEE,
                .target = customers[0]->id()
            };
        }
        
        // 检查林阿姨是否在（特殊互动）
        auto linaiyi = world.get_npc("linaiyi");
        if (linaiyi && linaiyi->current_location == "cafe") {
            if (relationships["linaiyi"].affinity > 6 && 
                emotion.happiness > 3) {
                return Action{
                    .type = ActionType::INITIATE_CHAT,
                    .target = "linaiyi",
                    .data = {{"topic", "今天的天气不错"}}
                };
            }
        }
        
        // 默认行为
        return Action{.type = ActionType::IDLE, .data = {{"activity", "整理吧台"}}};
    }
};

} // namespace starcafe
```

### 3. 社交事件系统（新增）

```cpp
// social_event.hpp
#pragma once
#include <vector>
#include <npc_agent.hpp>

namespace starcafe {

// 社交事件
struct SocialEvent {
    std::string id;
    std::string type;           // "conversation", "conflict", "confession"
    std::vector<std::string> participants;
    std::string trigger;        // 触发条件
    float probability = 0.1f;   // 发生概率
};

class SocialEventSystem {
public:
    // 每 tick 检查是否有社交事件应该发生
    void tick(WorldState& world) {
        for (const auto& event_def : event_definitions_) {
            if (should_trigger(event_def, world)) {
                execute_event(event_def, world);
            }
        }
        
        // 随机生成自发互动
        if (random() < 0.1) {
            generate_spontaneous_interaction(world);
        }
    }
    
    // 执行表白事件（复杂社交）
    void execute_confession(const std::string& from_id, 
                           const std::string& to_id,
                           WorldState& world) {
        auto from = world.get_npc(from_id);
        auto to = world.get_npc(to_id);
        
        // 执行表白对话
        auto confession = generate_confession_dialogue(from, to);
        broadcast_to_world(world, confession);
        
        // 更新关系
        if (accept_confession(to, from)) {
            from->relationships[to_id].feeling = "lover";
            to->relationships[from_id].feeling = "lover";
            
            // 其他 NPC 会注意到并反应
            for (auto& observer : world.get_nearby_npcs(from_id)) {
                if (observer->id() != from_id && observer->id() != to_id) {
                    observer->perceive_social_event("confession_accepted", 
                                                    {from_id, to_id});
                }
            }
        }
    }

private:
    std::vector<SocialEvent> event_definitions_;
};

} // namespace starcafe
```

### 4. 世界状态管理（新增）

```cpp
// world_state.hpp
#pragma once
#include <npc_agent.hpp>

namespace starcafe {

class WorldState {
public:
    Time current_time_;
    std::string weather_;
    
    // 所有 NPC
    std::map<std::string, std::unique_ptr<NPCAgent>> npcs_;
    
    // 空间管理
    std::map<std::string, std::vector<std::string>> location_occupants_;
    
    // 全局事件历史
    std::vector<WorldEvent> event_history_;
    
    // 模拟推进
    void advance_time(Duration delta);
    
    // 让所有 NPC 行动
    void tick_all_npcs() {
        for (auto& [id, npc] : npcs_) {
            auto action = npc->decide_action(*this);
            execute_action(id, action);
        }
    }
    
    // 获取附近 NPC
    std::vector<NPCAgent*> get_nearby_npcs(const std::string& location) {
        std::vector<NPCAgent*> nearby;
        for (const auto& id : location_occupants_[location]) {
            nearby.push_back(npcs_[id].get());
        }
        return nearby;
    }
};

} // namespace starcafe
```

## 从 Step 17 复用的组件

| Step 17 组件 | Step 18 复用方式 |
|:---:|:---|
| `TravelerProfile` | `NPCAgent` 的基类 |
| `TripPlanner` | 改为 `DailyScheduler`（日程规划） |
| `TravelPreferences` | 改为 `NPCPreferences`（角色偏好） |
| `MemoryManager` | 完全复用，每个 NPC 独立实例 |
| `PersonalizedChatEngine` | 复用为 `NPCDialogueEngine` |

## Step 18 新增组件

- `WorldState` — 全局世界状态
- `SocialEventSystem` — 社交事件系统
- `RelationshipGraph` — 关系网络
- `SpaceManager` — 空间位置管理

## 交互示例

```
> 观察

☕ 星语轩咖啡厅
现在是 14:30，周五下午，☀️ 晴朗

店里的人：
• 老王 在吧台，正在整理咖啡豆
• 小雨 在窗边座位，正在画画
• 阿杰 在角落，正在敲代码
• 林阿姨 在靠窗位置，正在看书

> 等待 30 分钟

[时间推进到 15:00]

突然外面开始下大雨 🌧️

老王看着窗外："看来一时半会儿停不了。"

小雨："哇——那我今天可以多画一会儿了！"

阿杰：（小声）"那我也...再待一会儿好了。"

林阿姨放下书："下雨天最适合聊天了。
年轻人，要不要听阿姨讲讲我年轻时候的故事？"

[触发事件：雨天的长谈]

---

> 听小雨和阿杰聊天

小雨："阿杰阿杰，帮我看看这个网站为什么打不开？"

阿杰："我看看...（凑过去）哦，DNS 设置问题..."

小雨："哇解决了！谢谢！你真的好厉害~"

阿杰：（耳朵有点红）"小、小事一桩..."

老王：（在吧台后面看着，微微摇头笑了笑）

[关系变化：小雨对阿杰的好感度 +0.5]
```

---

**从 Step 17 到 Step 18 的学习路径：**

```
Step 17: 学会给单个 Agent 加记忆和情感
         ↓
Step 18: 学会让多个 Agent 共享一个世界，
         彼此影响、产生故事
```

这是从「智能助手」到「虚拟世界」的跃迁！
