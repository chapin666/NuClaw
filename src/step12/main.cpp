// ============================================================================
// main.cpp - Step 12: 配置管理演示
// ============================================================================

#include "config.hpp"
#include <iostream>
#include <fstream>

// 创建示例配置文件
void create_config_file() {
    std::ofstream file("config.yaml");
    file << R"(
server:
  host: "0.0.0.0"
  port: 8080

llm:
  model: "gpt-4"
  timeout: 30
  temperature: 0.7

logging:
  level: "info"
  format: "json"
)";
}

int main() {
    std::cout << "========================================\n";
    std::cout << "   NuClaw Step 12: 配置管理\n";
    std::cout << "========================================\n\n";
    
    // 1. 创建示例配置
    create_config_file();
    std::cout << "[1] 创建配置文件 config.yaml\n\n";
    
    // 2. 加载配置
    Config config;
    if (!config.load_yaml("config.yaml")) {
        std::cerr << "加载配置失败\n";
        return 1;
    }
    std::cout << "[2] 加载配置完成\n";
    config.print_all();
    
    // 3. 读取配置
    std::cout << "\n[3] 读取配置项:\n";
    std::cout << "  server.host = " << config.get_string("server.host") << "\n";
    std::cout << "  server.port = " << config.get_int("server.port") << "\n";
    std::cout << "  llm.model = " << config.get_string("llm.model") << "\n";
    std::cout << "  llm.timeout = " << config.get_int("llm.timeout") << "\n";
    
    // 4. 环境变量覆盖
    std::cout << "\n[4] 加载环境变量 (NUCLAW_*)\n";
    // 注意：实际测试时需要设置环境变量
    // export NUCLAW_SERVER_PORT=9090
    config.load_env("NUCLAW_");
    
    // 5. 程序内修改
    std::cout << "\n[5] 程序内修改配置\n";
    config.set("app.name", "NuClaw");
    config.set("app.version", "1.0.0");
    
    std::cout << "\n[6] 最终配置:\n";
    config.print_all();
    
    // 清理
    std::remove("config.yaml");
    
    std::cout << "\n========================================\n";
    std::cout << "配置管理演示完成！\n";
    std::cout << "========================================\n";
    
    return 0;
}
