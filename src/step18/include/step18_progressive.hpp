// ============================================================================
// step18_progressive.hpp - Step 18: 虚拟咖啡厅「星语轩」
// ============================================================================
//
// 演进路径：
// Step 17: TravelAgent ──→ Step 18: NPC（社交型 Agent）
//                   多个 Agent 协作
// ============================================================================

#pragma once
#include "step17_progressive.hpp"
#include <vector>
#include <memory>
#include <algorithm>

namespace step18 {

using namespace step17;

// ============================================
// Step 18: NPC Agent（有社交能力的 Agent）
// ============================================

class NPCAgent : public StatefulAgent {
public:
    std::string location;      // 当前位置
    std::string current_action;
    float social_energy;       // 社交精力
    
    NPCAgent(const std::string& name, const std::string& role,
             const std::string& loc)
        : StatefulAgent(name, role), 
          location(loc), 
          current_action("idle"),
          social_energy(10.0f) {}
    
    // NPC 自主决策
    std::string decide_action(std::vector<NPCAgent*>& other_npcs, int hour) {
        // 如果精力低，休息
        if (social_energy < 3.0f) {
            current_action = "resting";
            social_energy += 2.0f;
            return name_ + " 感到疲倦，正在休息...";
        }
        
        // 晚上打烊
        if (hour >= 22) {
            current_action = "sleeping";
            return name_ + " 已经回家了";
        }
        
        // 随机找人聊天
        if (!other_npcs.empty() && (rand() % 100) < 40) {
            auto* other = other_npcs[rand() % other_npcs.size()];
            
            current_action = "talking_to_" + other->name_;
            social_energy -= 1.0f;
            
            // 增进关系
            state_.update_relationship(other->name_, other->name_, 0.3f);
            
            return name_ + " 正在和 " + other->name_ + " 聊天";
        }
        
        // 默认行为
        current_action = get_default_action(hour);
        social_energy -= 0.3f;
        return name_ + " 在" + location + " " + describe_action(current_action);
    }
    
    // NPC 间对话
    std::string interact_with(NPCAgent* other) {
        auto& rel = state_.get_or_create_relationship(other->name_);
        
        if (rel.affinity > 7) {
            return name_ + ": 见到" + other->name_ + "真开心！";
        } else if (rel.familiarity > 3) {
            return name_ + ": 嗨，" + other->name_ + "~";
        } else {
            return name_ + ": （点头）";
        }
    }

private:
    std::string get_default_action(int hour) {
        if (hour < 10) return "opening";
        if (hour < 12) return "working";
        if (hour < 14) return "lunch";
        if (hour < 18) return "working";
        return "closing";
    }
    
    std::string describe_action(const std::string& action) {
        if (action == "opening") return "准备开业";
        if (action == "working") return "工作";
        if (action == "lunch") return "吃午餐";
        if (action == "closing") return "整理收尾";
        return "发呆";
    }
};

// ============================================
// 咖啡厅世界（管理多个 NPC）
// ============================================

class CafeWorld {
public:
    std::vector<std::unique_ptr<NPCAgent>> npcs;
    int current_hour = 14;
    std::string weather = "sunny";
    
    void init() {
        srand(time(nullptr));
        
        // 创建 5 个 NPC
        auto laowang = std::make_unique<NPCAgent>("老王", "咖啡师", "吧台");
        auto xiaoyu = std::make_unique<NPCAgent>("小雨", "插画师", "窗边");
        auto ajie = std::make_unique<NPCAgent>("阿杰", "程序员", "角落");
        auto linaiyi = std::make_unique<NPCAgent>("林阿姨", "退休教师", "沙发");
        
        npcs.push_back(std::move(laowang));
        npcs.push_back(std::move(xiaoyu));
        npcs.push_back(std::move(ajie));
        npcs.push_back(std::move(linaiyi));
    }
    
    void tick() {
        std::cout << "\n=== " << current_hour << ":00 " << get_weather_emoji() << " ===\n";
        
        // 收集所有 NPC 指针（用于交互）
        std::vector<NPCAgent*> npc_ptrs;
        for (auto& npc : npcs) {
            npc_ptrs.push_back(npc.get());
        }
        
        // 每个 NPC 行动
        for (auto& npc : npcs) {
            std::string action = npc->decide_action(npc_ptrs, current_hour);
            std::cout << "• " << action << "\n";
        }
        
        // 触发社交事件
        if (rand() % 100 < 20) {
            trigger_social_event();
        }
        
        current_hour++;
    }
    
    void show_scene() {
        std::cout << "\n☕ 星语轩 " << get_weather_emoji() << "\n";
        std::cout << "时间: " << current_hour << ":00\n\n";
        std::cout << "店内:\n";
        
        for (const auto& npc : npcs) {
            std::cout << "  " << npc->get_name() << " (" << npc->get_role() << ")\n";
            std::cout << "    位置: " << npc->location << "\n";
            std::cout << "    状态: " << npc->current_action << "\n";
            std::cout << "    精力: " << int(npc->social_energy) << "/10\n\n";
        }
    }
    
    void show_relationships() {
        std::cout << "\n💕 人物关系:\n";
        for (const auto& npc : npcs) {
            std::cout << npc->get_name() << ":\n";
            for (const auto& [name, rel] : npc->get_state().relationships) {
                std::cout << "  → " << rel.user_name 
                          << " (好感度: " << int(rel.affinity) << "/10)\n";
            }
        }
    }
    
    NPCAgent* find_npc(const std::string& name) {
        for (auto& npc : npcs) {
            if (npc->get_name() == name) return npc.get();
        }
        return nullptr;
    }

private:
    std::string get_weather_emoji() {
        if (weather == "rainy") return "🌧️";
        if (weather == "sunny") return "☀️";
        return "☁️";
    }
    
    void trigger_social_event() {
        auto* xiaoyu = find_npc("小雨");
        auto* ajie = find_npc("阿杰");
        
        if (xiaoyu && ajie && 
            xiaoyu->current_action.find("talking") != std::string::npos &&
            ajie->current_action.find("talking") != std::string::npos) {
            
            auto& rel = xiaoyu->get_state().get_or_create_relationship("ajie");
            if (rel.affinity > 8 && current_hour >= 19) {
                std::cout << "\n💕 【特殊事件】小雨和阿杰在窗边聊得很开心...\n";
            }
        }
    }
};

} // namespace step18