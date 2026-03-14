// ============================================================================
// main.cpp - Step 18: 虚拟咖啡厅「星语轩」
// ============================================================================

#include "step18_progressive.hpp"
#include <iostream>

using namespace step18;

int main() {
    std::cout << "========================================\n";
    std::cout << "Step 18: 虚拟咖啡厅「星语轩」\n";
    std::cout << "========================================\n";
    std::cout << "演进：多个 StatefulAgent 协作\n\n";
    
    CafeWorld world;
    world.init();
    
    std::cout << "欢迎光临星语轩！\n";
    std::cout << "这里有几位常客:\n";
    std::cout << "  • 老王 - 咖啡师（暗恋林阿姨）\n";
    std::cout << "  • 小雨 - 插画师（喜欢阿杰）\n";
    std::cout << "  • 阿杰 - 程序员（喜欢小雨）\n";
    std::cout << "  • 林阿姨 - 退休教师（知道一切）\n\n";
    
    // 显示初始场景
    world.show_scene();
    world.show_relationships();
    
    std::cout << "\n【咖啡厅的一天】\n";
    std::cout << "----------------------------------------\n";
    
    // 模拟几小时
    for (int i = 0; i < 5; i++) {
        world.tick();
    }
    
    std::cout << "\n\n========================================\n";
    std::cout << "演进成果:\n";
    std::cout << "  ✓ 多个 StatefulAgent 实例\n";
    std::cout << "  ✓ NPC 自主决策系统\n";
    std::cout << "  ✓ NPC 间社交互动\n";
    std::cout << "  ✓ 关系网络动态变化\n";
    std::cout << "  ✓ 特殊事件触发\n";
    std::cout << "\n下一步 → Step 19: SaaS 多租户平台\n";
    
    return 0;
}