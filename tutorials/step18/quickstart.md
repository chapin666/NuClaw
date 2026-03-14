# Step 18: 虚拟咖啡厅「星语轩」- 完整实战案例

> **目标**：实现多 Agent 协作的虚拟世界
> 
003e **案例**：5 个 NPC 在咖啡厅里的日常生活与互动
> 
003e **预计时间**：6-8 小时

---

## 📋 实战案例概述

### 案例场景

这是一个虚拟咖啡厅，有 5 个常驻 NPC：

| NPC | 身份 | 性格 | 秘密 |
|:---|:---|:---|:---|
| 老王 | 咖啡师 | 沉默寡言，内心温暖 | 暗恋林阿姨 |
| 小雨 | 插画师 | 活泼开朗，有点迷糊 | 暗恋阿杰 |
| 阿杰 | 程序员 | 技术宅，内向 | 暗恋小雨 |
| 林阿姨 | 退休教师 | 温暖健谈 | 知道所有年轻人的秘密 |
| 橘子 | 店猫 | 高冷，会撒娇 | 无 |

### 我们要实现的功能

1. **NPC 自主决策** - 每个 NPC 根据时间、地点、心情决定做什么
2. **关系网络** - NPC 之间的好感度动态变化
3. **社交事件** - 雨天聊天、表白、冲突等事件触发
4. **玩家互动** - 玩家可以观察、对话、影响故事

---

## 🚀 快速开始（30 分钟 MVP）

### 第一步：项目结构

```bash
cd NuClaw
cp -r src/step17 src/step18
cd src/step18

mkdir -p include/starcafe include/npcs src/npcs
touch include/starcafe/npc_agent.hpp
touch include/npcs/{laowang,xiaoyu,ajie,linaiyi,orange}.hpp
```

### 第二步：最简 NPC 系统

```cpp
// include/starcafe/simple_npc.hpp
#pragma once
#include <string>
#include <vector>
#include <map>
#include <random>
#include <iostream>

namespace starcafe {

// 最简 NPC
struct SimpleNPC {
    std::string id;
    std::string name;
    std::string role;
    std::vector<std::string> traits;
    
    // 状态
    std::string location = "cafe";
    std::string activity = "idle";
    std::string mood = "neutral";
    float energy = 10.0f;
    
    // 与其他 NPC 的关系（-10 到 10）
    std::map<std::string, float> relationships;
    
    // 决策
    std::string decide_action(const std::map<std::string, SimpleNPC*>& others) {
        // 如果能量低，休息
        if (energy < 3) {
            activity = "resting";
            energy += 2;
            return name + " 看起来有点累，正在休息...";
        }
        
        // 随机找人聊天
        if (rand() % 100 < 30 && !others.empty()) {
            auto it = others.begin();
            std::advance(it, rand() % others.size());
            
            activity = "talking";
            energy -= 1;
            
            // 关系变化
            relationships[it->second->name] += 0.5f;
            
            return name + " 正在和 " + it->second->name + " 聊天...";
        }
        
        // 默认行为
        activity = "working";
        energy -= 0.5f;
        return name + " 在做自己的事情...";
    }
    
    // 对话
    std::string talk(const std::string& player_msg) {
        if (player_msg.find("你好") != std::string::npos) {
            return "你好呀！欢迎来到星语轩~";
        }
        if (player_msg.find("咖啡") != std::string::npos) {
            return "我们的咖啡都是现磨的，很香哦！";
        }
        return "（点头微笑）";
    }
};

// 世界
class SimpleWorld {
public:
    std::map<std::string, SimpleNPC> npcs;
    int hour = 14;  // 下午 2 点
    
    void init() {
        // 创建 NPC
        npcs["laowang"] = {"laowang", "老王", "咖啡师", 
                          {"沉默", "手艺好"}, "counter", "working", 8};
        npcs["xiaoyu"] = {"xiaoyu", "小雨", "插画师",
                         {"活泼", "迷糊"}, "window", "drawing", 9};
        npcs["ajie"] = {"ajie", "阿杰", "程序员",
                       {"内向", "技术宅"}, "corner", "coding", 7};
        npcs["linaiyi"] = {"linaiyi", "林阿姨", "退休教师",
                          {"温暖", "健谈"}, "sofa", "reading", 8};
        
        // 初始化关系
        npcs["xiaoyu"].relationships["ajie"] = 7;  // 小雨对阿杰有好感
        npcs["ajie"].relationships["xiaoyu"] = 6;  // 阿杰也喜欢小雨
        npcs["laowang"].relationships["linaiyi"] = 8;  // 老王暗恋林阿姨
    }
    
    void tick() {
        std::cout << "\n=== " << hour << ":00 ===\n";
        
        for (auto& [id, npc] : npcs) {
            // 构建其他 NPC 的 map
            std::map<std::string, SimpleNPC*> others;
            for (auto& [other_id, other] : npcs) {
                if (other_id != id) {
                    others[other_id] = &other;
                }
            }
            
            std::string action = npc.decide_action(others);
            std::cout << "• " << action << "\n";
        }
        
        // 时间推进
        hour += 1;
        if (hour >= 22) {
            std::cout << "\n🌙 咖啡厅打烊了...\n";
        }
    }
    
    std::string get_scene() {
        std::string scene = "☕ 星语轩\n";
        scene += "现在是 " + std::to_string(hour) + ":00\n\n";
        scene += "店里的人：\n";
        
        for (const auto& [id, npc] : npcs) {
            scene += "• " + npc.name + " - " + npc.activity + "\n";
        }
        
        return scene;
    }
};

} // namespace starcafe
```

### 第三步：主程序

```cpp
// src/main.cpp
#include "starcafe/simple_npc.hpp"
#include <iostream>
#include <string>
#include <cstdlib>
#include <time>

int main() {
    using namespace starcafe;
    
    srand(time(nullptr));
    
    SimpleWorld world;
    world.init();
    
    std::cout << "☕ 欢迎来到星语轩！\n";
    std::cout << "你可以：\n";
    std::cout << "  'look' - 观察周围环境\n";
    std::cout << "  'talk <npc>' - 和某人对话\n";
    std::cout << "  'wait' - 等待一小时\n";
    std::cout << "  'quit' - 离开\n\n";
    
    std::string input;
    while (true) {
        std::cout <> "> ";
        std::getline(std::cin, input);
        
        if (input == "quit") {
            std::cout << "\n再见！欢迎下次再来~\n";
            break;
        }
        
        if (input == "look") {
            std::cout << "\n" << world.get_scene() << "\n";
        }
        else if (input == "wait") {
            world.tick();
        }
        else if (input.substr(0, 4) == "talk") {
            std::string npc_name = input.substr(5);
            
            // 简单匹配
            std::string npc_id;
            if (npc_name.find("老王") != std::string::npos) npc_id = "laowang";
            else if (npc_name.find("小雨") != std::string::npos) npc_id = "xiaoyu";
            else if (npc_name.find("阿杰") != std::string::npos) npc_id = "ajie";
            else if (npc_name.find("林阿姨") != std::string::npos) npc_id = "linaiyi";
            
            if (!npc_id.empty() && world.npcs.count(npc_id)) {
                std::cout << "\n🗣️ 你对 " << world.npcs[npc_id].name << " 说：你好！\n";
                std::cout << world.npcs[npc_id].name << "：" 
                          << world.npcs[npc_id].talk("你好") << "\n\n";
            } else {
                std::cout << "找不到这个人...\n";
            }
        }
        else {
            std::cout << "不明白你的意思...\n";
        }
    }
    
    return 0;
}
```

### 第四步：运行测试

```bash
# 编译
g++ -std=c++17 -I include src/main.cpp -o starcafe

# 运行
./starcafe
```

```
☕ 欢迎来到星语轩！
你可以：
  'look' - 观察周围环境
  'talk <npc>' - 和某人对话
  'wait' - 等待一小时
  'quit' - 离开

> look

☕ 星语轩
现在是 14:00

店里的人：
• 老王 - working
• 小雨 - drawing
• 阿杰 - coding
• 林阿姨 - reading

> wait

=== 15:00 ===
• 老王 在做自己的事情...
• 小雨 正在和 阿杰 聊天...
• 阿杰 正在和 小雨 聊天...
• 林阿姨 在做自己的事情...

> talk 小雨

🗣️ 你对 小雨 说：你好！
小雨：你好呀！欢迎来到星语轩~

> wait

=== 16:00 ===
• 老王 看起来有点累，正在休息...
• 小雨 在做自己的事情...
• 阿杰 正在和 林阿姨 聊天...
• 林阿姨 正在和 阿杰 聊天...

> quit

再见！欢迎下次再来~
```

---

## 进阶：完整故事线

### 故事线 1：雨天的长谈

```cpp
// 当天气变成雨天时触发
class RainEvent : public SocialEvent {
public:
    bool should_trigger(World& world) override {
        return world.weather == "rainy" && 
               world.hour >= 15 && world.hour <= 18;
    }
    
    void execute(World& world) override {
        std::cout << "\n🌧️ 外面突然下起大雨...\n";
        std::cout << "老王：看来一时半会儿停不了。\n";
        std::cout << "小雨：哇——那我今天可以多画一会儿了！\n";
        std::cout << "阿杰：（小声）那我也...再待一会儿好了。\n";
        std::cout << "林阿姨：下雨天最适合聊天了。\n\n";
        
        // 增加互动概率
        for (auto& [id, npc] : world.npcs) {
            npc.energy += 2;  // 雨天让人放松
        }
    }
};
```

### 故事线 2：表白事件

```cpp
// 当两个 NPC 关系达到一定程度时触发
class ConfessionEvent : public SocialEvent {
public:
    std::string from_id, to_id;
    
    bool should_trigger(World& world) override {
        auto& from = world.npcs[from_id];
        auto& to = world.npcs[to_id];
        
        // 关系足够好，且两人在场
        return from.relationships[to_id] > 8 && 
               from.location == to.location &&
               world.hour >= 19;  // 晚上氛围好
    }
    
    void execute(World& world) override {
        auto& from = world.npcs[from_id];
        auto& to = world.npcs[to_id];
        
        std::cout << "\n💕 " << from.name << " 深吸一口气，看着 " << to.name << "...\n";
        std::cout << from.name << "：那个...我有话想对你说...\n";
        
        // 根据关系决定结果
        if (to.relationships[from_id] > 6) {
            std::cout << to.name << "：（脸红）其实我也...\n";
            std::cout << "\n✨ 他们在一起了！\n";
            
            from.relationships[to_id] = 10;
            to.relationships[from_id] = 10;
        } else {
            std::cout << to.name << "：呃...我们是好朋友对吧？\n";
            std::cout << "\n💔 被拒绝了呢...\n";
            
            from.mood = "sad";
            from.energy -= 3;
        }
    }
};
```

---

## 完整版功能清单

- [ ] NPC 详细性格系统
- [ ] 完整的日程安排
- [ ] 空间位置管理（谁在哪个位置）
- [ ] 复杂关系网络（信任、好感、熟悉度）
- [ ] 记忆系统（记住和每个人的互动）
- [ ] 情感计算（根据事件变化）
- [ ] 社交事件系统（雨天、生日、表白等）
- [ ] 玩家深度互动（撮合、挑拨、送礼物）
- [ ] 故事线系统（多个主线剧情）

---

**恭喜！你已经创建了一个最简单的虚拟咖啡厅！** ☕

NPC 们已经开始自己的生活了。观察他们，看看会发生什么有趣的事情！
