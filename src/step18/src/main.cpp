// src/main.cpp - Step 18: 虚拟咖啡厅「星语轩」
#include "starcafe/simple_npc.hpp"
#include <iostream>
#include <string>

using namespace starcafe;

int main() {
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
        std::cout << "> ";
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