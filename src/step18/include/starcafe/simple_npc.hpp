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
    
    std::string location = "cafe";
    std::string activity = "idle";
    std::string mood = "neutral";
    float energy = 10.0f;
    
    std::map<std::string, float> relationships;
    
    // 构造函数
    SimpleNPC() = default;
    SimpleNPC(const std::string& i, const std::string& n, 
              const std::string& r, const std::vector<std::string>& t,
              const std::string& loc, const std::string& act, float e)
        : id(i), name(n), role(r), traits(t), location(loc), activity(act), energy(e) {}
    
    std::string decide_action(std::map<std::string, SimpleNPC*>& others) {
        if (energy < 3) {
            activity = "resting";
            energy += 2;
            return name + " 看起来有点累，正在休息...";
        }
        
        if (rand() % 100 < 30 && !others.empty()) {
            auto it = others.begin();
            std::advance(it, rand() % others.size());
            
            activity = "talking";
            energy -= 1;
            
            relationships[it->second->name] += 0.5f;
            
            return name + " 正在和 " + it->second->name + " 聊天...";
        }
        
        activity = "working";
        energy -= 0.5f;
        return name + " 在做自己的事情...";
    }
    
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
    int hour = 14;
    
    void init() {
        srand(time(nullptr));
        
        npcs["laowang"] = SimpleNPC("laowang", "老王", "咖啡师", 
                          {"沉默", "手艺好"}, "counter", "working", 8);
        npcs["xiaoyu"] = SimpleNPC("xiaoyu", "小雨", "插画师",
                         {"活泼", "迷糊"}, "window", "drawing", 9);
        npcs["ajie"] = SimpleNPC("ajie", "阿杰", "程序员",
                       {"内向", "技术宅"}, "corner", "coding", 7);
        npcs["linaiyi"] = SimpleNPC("linaiyi", "林阿姨", "退休教师",
                          {"温暖", "健谈"}, "sofa", "reading", 8);
        
        npcs["xiaoyu"].relationships["ajie"] = 7;
        npcs["ajie"].relationships["xiaoyu"] = 6;
        npcs["laowang"].relationships["linaiyi"] = 8;
    }
    
    void tick() {
        std::cout << "\n=== " << hour << ":00 ===\n";
        
        for (auto& [id, npc] : npcs) {
            std::map<std::string, SimpleNPC*> others;
            for (auto& [other_id, other] : npcs) {
                if (other_id != id) {
                    others[other_id] = &other;
                }
            }
            
            std::string action = npc.decide_action(others);
            std::cout << "• " << action << "\n";
        }
        
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