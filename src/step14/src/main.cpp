// ============================================================================
// main.cpp - Step 11: 多 Agent 协作演示
// ============================================================================

#include "nuclaw/message_bus.hpp"
#include "nuclaw/coordinator.hpp"
#include "nuclaw/transport_agent.hpp"
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    std::cout << "========================================\n";
    std::cout << "   NuClaw Step 11: 多 Agent 协作\n";
    std::cout << "========================================\n\n";
    
    // 1. 创建消息总线
    MessageBus bus;
    
    // 2. 创建协调器
    auto coordinator = std::make_shared<Coordinator>("coordinator");
    bus.register_agent("coordinator", coordinator);
    
    // 3. 创建专业 Agent
    auto transport = std::make_shared<TransportAgent>();
    
    bus.register_agent("transport_agent", transport);
    
    // 4. 注册 Agent 信息到协调器
    coordinator->register_agent_info("transport_agent", "交通查询");
    
    // 5. 模拟任务分发
    std::cout << "\n[场景] 用户：帮我规划北京到上海的行程\n\n";
    
    coordinator->dispatch_task(
        "trip_001",
        "查询北京到上海的航班",
        {"transport_agent"}
    );
    
    // 等待任务完成
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    std::cout << "\n[协调器] 汇总结果，生成完整行程建议\n";
    
    // 6. 关闭
    bus.shutdown();
    
    std::cout << "\n========================================\n";
    std::cout << "多 Agent 演示完成！\n";
    std::cout << "========================================\n";
    
    return 0;
}
