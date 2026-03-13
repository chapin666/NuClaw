// ============================================================================
// main.cpp - NuClaw 主程序入口
// ============================================================================

#include "nuclaw/chat_engine.hpp"
#include "nuclaw/config.hpp"
#include "nuclaw/tool_executor.hpp"
#include <iostream>

void print_banner() {
    std::cout << R"(
    _   __                      __        
   / | / /__  _  ___________ _/ /_  _____
  /  |/ / _ \| |/_/ ___/ __ `/ __ \/ ___/
 / /|  /  __/>  <(__  ) /_/ / / / (__  ) 
/_/ |_/\___/_/|_/____/\__,_/_/ /_/____/  
                                          
    AI Agent Framework v1.0.0
    )" << std::endl;
    std::cout << std::endl;
}

int main(int argc, char* argv[]) {
    print_banner();
    
    // 加载配置
    Config config;
    std::string config_path = (argc > 1) ? argv[1] : "configs/config.yaml";
    
    if (!config.load_yaml(config_path)) {
        std::cerr << "Failed to load config: " << config_path << std::endl;
        return 1;
    }
    
    // 初始化工具
    ToolExecutor::init_tools();
    
    // 创建 ChatEngine
    ChatEngine engine;
    ChatContext ctx;
    
    std::cout << engine.get_welcome_message() << std::endl;
    std::cout << "\nType 'exit' to quit.\n\n";
    
    // 交互循环
    std::string input;
    while (true) {
        std::cout << "👤 You: ";
        std::getline(std::cin, input);
        
        if (input == "exit" || input == "quit") {
            break;
        }
        
        if (input.empty()) continue;
        
        std::string reply = engine.process(input, ctx);
        std::cout << "🤖 NuClaw: " << reply << std::endl;
        std::cout << std::endl;
    }
    
    std::cout << "Goodbye! 👋" << std::endl;
    return 0;
}
