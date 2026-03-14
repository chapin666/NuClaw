# Step 18: 虚拟咖啡厅「星语轩」

> 目标：构建一个多 AI 角色共存的虚拟社交空间
> 
> 难度：⭐⭐⭐⭐⭐ (毕业项目)
> > 代码量：约 1500 行

## ☕ 项目简介

欢迎来到 **「星语轩」** —— 一家位于虚拟街角的温馨咖啡厅。

这里有多位性格各异的 AI "居民"：
- 👨‍🍳 **老王** —— 42岁咖啡师，沉默寡言但手艺精湛，总记得熟客的口味
- 🎨 **小雨** —— 26岁插画师，活泼开朗，坐在窗边画画，喜欢跟人聊梦想
- 💻 **阿杰** —— 30岁程序员，常带着笔记本加班，对新技术话题滔滔不绝
- 📚 **林阿姨** —— 58岁退休教师，每天下午来读诗，乐于倾听年轻人的烦恼
- 🐱 **橘子** —— 咖啡厅的猫，会蹭人、会挑食，偶尔捣乱

他们有自己的生活节奏、社交关系、情绪和记忆。你可以：
- ☕ 点一杯咖啡，观察他们之间的互动
- 💬 找任意角色聊天，影响他们的心情和关系
- 🔍 发现隐藏的故事线和角色秘密
- 🎭 见证随机事件：争吵、和解、暗恋表白...

## 🎭 角色设定

### 老王 (老板/咖啡师)

```yaml
name: 老王
age: 42
role: 咖啡师/老板
personality:
  - 外表冷漠，内心温暖
  - 不善言辞，但观察入微
  - 对咖啡极度执着
  - 对传统手艺有坚持

background: |
  年轻时在大城市做过金融，35岁 burnout 后回到老家开了这家咖啡厅。
  前妻带走了女儿，每月通一次电话。
  店里有一张女儿小时候的照片，压在收银台下。

preferences:
  likes: ["安静的早晨", "熟客", "雨天", "爵士乐"]
  dislikes: ["吵闹", "浪费食物", "速溶咖啡"]

secrets:
  - 其实偷偷关注女儿的微博
  - 晚上关店后会自己喝一杯威士忌
  - 对林阿姨有好感但不敢说

daily_routine:
  06:00: 到店准备
  07:00: 开门营业
  10:00: 检查咖啡豆库存
  14:00: 午休（在吧台后打盹）
  15:00: 继续做咖啡
  21:00: 关门
  22:00: 离开

memory_capacity: 50  # 能记住最近的 50 件事
```

### 小雨 (常客 - 插画师)

```yaml
name: 小雨
age: 26
role: 自由插画师
personality:
  - 元气满满，乐观主义
  - 有点迷糊但很有才华
  - 容易对人敞开心扉
  - 偶尔深夜 emo

background: |
  美院毕业后做自由职业，接商业插画维持生计，
  同时在创作自己的绘本。梦想是开个人画展。
  家境普通，父母希望她找个"正经工作"。

preferences:
  likes: ["晴天", "甜食", "被夸奖", "猫咪", "老王的拿铁"]
  dislikes: ["催稿", "否定她的梦想", "下雨天出门"]

relationships:
  老王: {feeling: "尊敬", trust: 8}
  阿杰: {feeling: "暧昧", trust: 7, notes: "经常聊天到很晚，不确定是不是喜欢"}
  林阿姨: {feeling: "亲近", trust: 9, notes: "像妈妈一样"}

triggers:
  - 提到"梦想"会兴奋
  - 提到"父母"会低落
  - 看到阿杰和其他女生说话会吃醋（不明显）
```

### 阿杰 (常客 - 程序员)

```yaml
name: 阿杰
age: 30
role: 远程工作程序员
personality:
  - 技术宅，说话带梗
  - 外冷内热，关心朋友但不说
  - 焦虑型，总担心被裁员
  - 对小雨有好感但怂

background: |
  大厂程序员，最近开始远程工作。
  存款有一些但不多，担心 35 岁危机。
  喜欢小雨但不知道怎么表达，
  经常"恰好"在她来的时候来店里。

preferences:
  likes: ["安静的环境", "美式咖啡", "技术讨论", "小雨的画"]
  dislikes: ["需求变更", "产品经理", "催婚"]

secrets:
  - 钱包里有小雨掉的头绳（偷偷捡的）
  - 偷偷买了小雨的绘本周边支持她
  - 其实不喜欢喝咖啡，只是喜欢来店里

behavior_patterns:
  - 每周三周五一定来（小雨常来的日子）
  - 会帮小雨解决电脑问题
  - 老王注意到他总偷看小雨
```

### 林阿姨 (常客 - 退休教师)

```yaml
name: 林阿姨
age: 58
role: 退休语文教师
personality:
  - 温和睿智，善于倾听
  - 偶尔语出惊人
  - 热爱生活，在学用智能手机
  - 有点孤独但不说

background: |
  教了 35 年书，老伴去世 3 年。
  儿子在国外，一年见一次。
  把店里的年轻人都当成自己孩子。
  正在写回忆录。

preferences:
  likes: ["下午的阳光", "读诗", "听年轻人聊天", "老王的咖啡"]
  dislikes: ["被叫阿姨（喜欢叫林老师）", "孤独"]

role_in_cafe: "精神导师"
  - 年轻人有心事会找她聊
  - 会偷偷撮合小雨和阿杰
  - 是唯一敢调侃老王的人
```

## 🏗️ 系统架构

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        星语轩虚拟咖啡厅                                  │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   ┌─────────────────────────────────────────────────────────────────┐  │
│   │                      世界状态管理器                              │  │
│   │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐           │  │
│   │  │ 时间系统  │ │ 事件调度  │ │ 关系网络  │ │ 空间管理  │           │  │
│   │  └──────────┘ └──────────┘ └──────────┘ └──────────┘           │  │
│   └─────────────────────────────────────────────────────────────────┘  │
│                              │                                          │
│   ┌──────────────────────────┼──────────────────────────┐              │
│   │                          │                          │              │
│   ▼                          ▼                          ▼              │
│ ┌──────────────┐      ┌──────────────┐      ┌──────────────┐          │
│ │    老王      │      │    小雨      │      │    阿杰      │          │
│ │   Agent      │◄────►│   Agent      │◄────►│   Agent      │          │
│ │              │      │              │      │              │          │
│ │ • 记忆系统   │      │ • 记忆系统   │      │ • 记忆系统   │          │
│ │ • 情绪状态   │      │ • 情绪状态   │      │ • 情绪状态   │          │
│ │ • 日程安排   │      │ • 日程安排   │      │ • 日程安排   │          │
│ │ • 对话引擎   │      │ • 对话引擎   │      │ • 对话引擎   │          │
│ └──────────────┘      └──────────────┘      └──────────────┘          │
│        ▲                       ▲                       ▲               │
│        └───────────────────────┼───────────────────────┘               │
│                                │                                       │
│                         ┌──────────────┐                               │
│                         │   林阿姨     │                               │
│                         │   Agent      │                               │
│                         └──────────────┘                               │
│                                                                         │
│   ┌─────────────────────────────────────────────────────────────────┐  │
│   │                      社交事件系统                                │  │
│   │  • NPC 间自发对话    • 关系变化    • 随机事件    • 故事触发      │  │
│   └─────────────────────────────────────────────────────────────────┘  │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

## 💻 核心代码实现

### 1. NPC Agent 基类

```cpp
struct NPCState {
    std::string id;
    std::string name;
    
    // 情绪状态 (-10 ~ +10)
    struct Emotion {
        float happiness = 0;    // 开心
        float energy = 5;       // 精力
        float anxiety = 0;      // 焦虑
        float loneliness = 0;   // 孤独
    } emotion;
    
    // 当前活动
    std::string current_activity;
    std::string location;  // "counter", "window_seat", "kitchen"
    
    // 记忆系统
    std::vector<Memory> short_term_memory;   // 最近发生的事
    std::vector<Memory> long_term_memory;    // 重要事件（存储到向量DB）
    
    // 日程
    std::map<Time, std::string> schedule;
};

struct Relationship {
    std::string target_id;
    float trust = 5;         // 信任度 0-10
    float affinity = 5;      // 好感度 0-10
    std::string feeling;     // "friend", "crush", "mentor", "annoyance"
    std::vector<std::string> shared_history;
};

class NPCAgent {
public:
    NPCAgent(const CharacterProfile& profile, 
             std::shared_ptr<WorldState> world)
        : profile_(profile), world_(world) {}
    
    // 感知环境变化
    void perceive(const Event& event) {
        // 将事件加入短期记忆
        state_.short_term_memory.push_back({
            .timestamp = world_->current_time(),
            .content = event.description,
            .emotion_impact = calculate_emotion_impact(event)
        });
        
        // 保持记忆容量
        if (state_.short_term_memory.size() > profile_.memory_capacity) {
            // 重要的转入长期记忆
            consolidate_memory();
        }
        
        // 更新情绪
        update_emotion(event);
    }
    
    // 决定下一步行动
    async::Task<Action> decide_action() {
        // 检查日程
        auto scheduled = check_schedule();
        if (scheduled) co_return *scheduled;
        
        // 检查是否需要响应其他 NPC
        auto social = co_await consider_social_interaction();
        if (social) co_return *social;
        
        // 根据当前状态选择默认行为
        co_return select_default_behavior();
    }
    
    // 对话处理
    async::Task<std::string> converse(
        const std::string& speaker_id,
        const std::string& message) {
        
        // 1. 理解对方意图和情绪
        auto analysis = co_await analyze_message(speaker_id, message);
        
        // 2. 检索相关记忆
        auto relevant_memories = co_await retrieve_memories(message);
        
        // 3. 考虑与说话者的关系
        auto& relation = relationships_[speaker_id];
        
        // 4. 考虑当前情绪状态
        auto current_mood = describe_emotion();
        
        // 5. 构建 prompt
        auto prompt = build_persona_prompt(
            analysis, relevant_memories, relation, current_mood
        );
        
        // 6. 生成回复
        auto response = co_await llm_.complete(prompt);
        
        // 7. 更新关系
        update_relationship(speaker_id, analysis.sentiment);
        
        // 8. 记录这次对话
        record_conversation(speaker_id, message, response);
        
        co_return response;
    }

private:
    std::string build_persona_prompt(
        const MessageAnalysis& analysis,
        const std::vector<Memory>& memories,
        const Relationship& relation,
        const std::string& mood) {
        
        return fmt::format(R"(
你是「星语轩」咖啡厅的{role}，名叫{name}。

你的性格：{personality}
你的背景：{background}
当前情绪状态：{mood}

与说话者的关系：
- 信任度：{trust}/10
- 好感度：{affinity}/10
- 整体感觉：{feeling}

相关记忆：
{memories}

对方说："{message}"
对方可能的意图：{intent}
对方情绪：{sentiment}

请用{name}的口吻回复，要自然、有性格，可以偶尔说些符合人设的话。
记住：你是活的角色，不是客服机器人。
)", fmt::arg("role", profile_.role),
   fmt::arg("name", profile_.name),
   fmt::arg("personality", profile_.personality),
   fmt::arg("background", profile_.background),
   fmt::arg("mood", mood),
   fmt::arg("trust", relation.trust),
   fmt::arg("affinity", relation.affinity),
   fmt::arg("feeling", relation.feeling),
   fmt::arg("memories", format_memories(memories)),
   fmt::arg("message", analysis.original_message),
   fmt::arg("intent", analysis.intent),
   fmt::arg("sentiment", analysis.sentiment)
        );
    }
    
    void update_emotion(const Event& event) {
        // 事件对情绪的影响
        state_.emotion.happiness += event.happiness_impact;
        state_.emotion.energy += event.energy_impact;
        state_.emotion.anxiety += event.anxiety_impact;
        
        // 情绪自然衰减/恢复
        state_.emotion.happiness *= 0.95;
        state_.emotion.energy = std::min(10.0f, state_.emotion.energy + 0.1f);
        
        // 限制范围
        clamp_emotions();
    }
    
    async::Task<std::optional<Action>> consider_social_interaction() {
        // 检查周围是否有其他 NPC
        auto nearby = world_->get_npcs_near(state_.location);
        
        for (const auto& other : nearby) {
            if (other->id() == state_.id) continue;
            
            // 检查关系和历史
            auto& relation = relationships_[other->id()];
            
            // 如果好感度高，主动聊天
            if (relation.affinity > 7 && state_.emotion.energy > 3) {
                auto topic = select_topic(other->id());
                co_return Action{
                    .type = ActionType::INITIATE_CHAT,
                    .target = other->id(),
                    .data = {{"topic", topic}}
                };
            }
            
            // 如果看到对方情绪低落，可能去关心
            if (other->emotion().happiness < -5 && relation.trust > 6) {
                co_return Action{
                    .type = ActionType::COMFORT,
                    .target = other->id()
                };
            }
        }
        
        co_return std::nullopt;
    }
};
```

### 2. 社交事件系统

```cpp
class SocialEventSystem {
public:
    // 随机触发 NPC 间互动
    void tick() {
        // 检查是否有事件应该发生
        for (auto& [trigger, event] : scheduled_events_) {
            if (trigger.should_trigger(world_state_)) {
                execute_event(event);
            }
        }
        
        // 随机生成自发互动
        if (random() < 0.1) {  // 10% 概率
            generate_spontaneous_interaction();
        }
    }
    
    // 执行剧本事件
    void execute_event(const StoryEvent& event) {
        switch (event.type) {
            case EventType::RAINY_DAY_CONVERSATION:
                // 下雨天，大家被困在店里，可能触发深度对话
                trigger_rainy_day_scene();
                break;
                
            case EventType::CONFLICT:
                // 两个 NPC 发生争吵
                trigger_conflict(event.participants);
                break;
                
            case EventType::CONFESSION:
                // 表白事件
                trigger_confession(event.participants[0], 
                                  event.participants[1]);
                break;
                
            case EventType::SECRET_REVEALED:
                // 秘密被发现
                trigger_secret_reveal(event.target, event.secret);
                break;
        }
    }

private:
    void trigger_rainy_day_scene() {
        // 让所有 NPC 留在店里
        for (auto& npc : npcs_) {
            npc->cancel_departure();
        }
        
        // 随机选择一个话题发起群聊
        auto initiator = select_random_npc();
        auto topic = select_deep_topic();
        
        broadcast_event(fmt::format(
            "外面下起大雨，{} 看着窗外说：\"{}\"",
            initiator->name(), topic
        ));
        
        // 触发连锁对话
        schedule_followup_conversations();
    }
    
    void trigger_confession(const std::string& from, const std::string& to) {
        // 表白事件会影响多个 NPC 的关系网
        auto from_npc = get_npc(from);
        auto to_npc = get_npc(to);
        
        // 执行表白
        auto result = from_npc->confess_to(to);
        
        if (result.accepted) {
            // 更新关系
            from_npc->relationships_[to].feeling = "lover";
            to_npc->relationships_[from].feeling = "lover";
            
            // 其他人会注意到
            for (auto& observer : get_nearby_npcs(from)) {
                if (observer->id() != from && observer->id() != to) {
                    observer->perceive(Event{
                        .description = fmt::format("看到 {} 向 {} 表白了", 
                                                  from_npc->name(), 
                                                  to_npc->name()),
                        .emotion_impact = observer->id() == "xiaoyu" ? -3 : 2
                        // 小雨会难过（如果她是暗恋者）
                    });
                }
            }
        }
    }
};
```

### 3. 玩家交互接口

```cpp
class CafeInterface {
public:
    // 玩家观察咖啡厅
    std::string observe() {
        std::string description = "☕ 星语轩咖啡厅\n\n";
        
        description += "现在是 " + world_->current_time_string() + "\n";
        description += "天气：" + world_->weather() + "\n\n";
        
        description += "店里的人：\n";
        for (const auto& [location, npc] : world_->npcs_by_location()) {
            description += fmt::format("• {} 在 {}，正在{}\n",
                npc->name(), location, npc->current_activity());
        }
        
        // 最近的对话片段
        description += "\n最近发生的事：\n";
        for (const auto& event : world_->recent_events(3)) {
            description += "• " + event.description + "\n";
        }
        
        return description;
    }
    
    // 与 NPC 对话
    async::Task<std::string> talk_to(const std::string& npc_id, 
                                    const std::string& message) {
        auto npc = world_->get_npc(npc_id);
        if (!npc) {
            return "店里没有这个人...";
        }
        
        // 检查 NPC 是否在附近
        if (!world_->is_nearby("player", npc_id)) {
            return fmt::format("{} 离得太远了，走过去再聊吧。", npc->name());
        }
        
        auto response = co_await npc->converse("player", message);
        co_return fmt::format("{}: \"{}\"", npc->name(), response);
    }
    
    // 听 NPC 们聊天
    async::Task<std::string> eavesdrop(const std::string& npc1_id,
                                       const std::string& npc2_id) {
        auto npc1 = world_->get_npc(npc1_id);
        auto npc2 = world_->get_npc(npc2_id);
        
        // 生成他们之间的对话
        auto conversation = co_await generate_npc_dialogue(npc1, npc2);
        
        return conversation;
    }
    
    // 点咖啡（影响老王对你的态度）
    std::string order_coffee(const std::string& type) {
        auto laowang = world_->get_npc("laowang");
        
        // 老王会根据你的选择有不同的反应
        if (type == "美式") {
            laowang->relationships_["player"].affinity += 0.5;
            return "老王默默点头，开始制作。他的美式是一绝。";
        } else if (type == "拿铁") {
            return "老王：\"今天拉花是树叶图案。\"";
        } else if (type == "速溶") {
            laowang->relationships_["player"].affinity -= 2;
            return "老王皱了皱眉：\"这里没有那种东西。\"";
        }
    }
    
    // 等待时间推进
    async::Task<std::string> wait(Duration duration) {
        world_->advance_time(duration);
        
        // 让 NPC 们行动
        for (auto& npc : world_->npcs()) {
            auto action = co_await npc->decide_action();
            world_->execute_action(npc->id(), action);
        }
        
        // 触发随机事件
        event_system_.tick();
        
        co_return observe();
    }
};
```

### 4. 使用示例

```
> 观察

☕ 星语轩咖啡厅

现在是 14:30，周五下午
天气：☀️ 晴朗

店里的人：
• 老王 在吧台，正在整理咖啡豆
• 小雨 在窗边座位，正在画画
• 阿杰 在角落，正在敲代码
• 林阿姨 在靠窗位置，正在看书

最近发生的事：
• 小雨向阿杰展示了她新画的插画
• 老王换了一张爵士乐唱片
• 橘子跳上了小雨的画纸（被轻轻赶下去了）

---

> 跟小雨说："你的画真好看，画的是什么？"

小雨："啊！谢谢！✨ 这是我在画的绘本插图，
讲一只寻找彩虹尽头的小狐狸。
（她眼睛亮晶晶的）

其实...我还在纠结要不要给狐狸画翅膀，
你觉得呢？有翅膀会不会太夸张？"

---

> 跟阿杰说："你好像经常来？"

阿杰："呃...（推了推眼镜）
这里环境好，适合写代码。

（压低声音）而且咖啡确实不错。

（他快速瞥了一眼窗边的小雨，又收回视线）
你...你也是来工作的吗？"

---

> 听小雨和阿杰聊天

小雨："阿杰阿杰，帮我看看这个网站为什么打不开？"

阿杰："我看看...（凑过去）哦，DNS 设置问题，
你把这行改成..."

小雨："哇解决了！谢谢！你真的好厉害~"

阿杰：（耳朵有点红）"小、小事一桩..."

老王：（在吧台后面看着，微微摇头笑了笑）

---

> 等待 30 分钟

[时间推进到 15:00]

突然外面开始下大雨 🌧️

老王看着窗外："看来一时半会儿停不了。"

小雨："哇——那我今天可以多画一会儿了！"

阿杰：（小声）"那我也...再待一会儿好了。"

林阿姨放下书："下雨天最适合聊天了。
年轻人，要不要听阿姨讲讲我年轻时候的故事？"

[触发事件：雨天的长谈]
```

---

## 🎮 高级特性

### 关系网络可视化

```cpp
// 可以导出角色关系图
{
    "nodes": [
        {"id": "laowang", "name": "老王", "group": "staff"},
        {"id": "xiaoyu", "name": "小雨", "group": "customer"},
        {"id": "ajie", "name": "阿杰", "group": "customer"},
        {"id": "linaiyi", "name": "林阿姨", "group": "customer"}
    ],
    "links": [
        {"source": "xiaoyu", "target": "ajie", 
         "relation": "mutual_crush", "strength": 7},
        {"source": "laowang", "target": "linaiyi", 
         "relation": "unspoken_love", "strength": 6},
        {"source": "linaiyi", "target": "xiaoyu", 
         "relation": "mentor", "strength": 9}
    ]
}
```

### 故事模式

预设的故事线：
- **「告白季」** — 阿杰终于鼓起勇气...
- ** **「女儿的电话」** — 老王接到久违的电话
- **「画展之夜」** — 小雨的第一个小型画展
- **「退休仪式」** — 林阿姨的告别

---

## 🎉 项目交付

完成本项目，你将拥有：

1. **多 Agent 系统框架** — 可扩展更多角色
2. **情感模拟系统** — NPC 有真实的情绪变化
3. **关系网络系统** — 复杂的社交动态
4. **事件驱动叙事** — 自动生成故事
5. **沉浸式体验** — 玩家可以真正影响虚拟世界

---

## 🎓 毕业快乐！

从 89 行的 Echo 服务器，到 1500+ 行的虚拟咖啡厅...

你现在已经掌握了：
- ✅ C++ 网络编程
- ✅ AI Agent 架构设计
- ✅ 多 Agent 协作系统
- ✅ 情感计算与社交模拟
- ✅ 事件驱动叙事

**欢迎来到 AI 工程师的世界！** ☕✨

---

**NuClaw 全部 18 章至此完结！**

愿你写出更多有趣的 AI 应用 🚀
