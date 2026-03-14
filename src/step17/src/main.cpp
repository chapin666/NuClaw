// ============================================================================
// main.cpp - Step 17: 旅行小管家（基于 Step 16 演进）
// ============================================================================

#include "step17_progressive.hpp"
#include <iostream>

using namespace step17;

int main() {
    std::cout << "========================================\n";
    std::cout << "Step 17: 旅行小管家\n";
    std::cout << "========================================\n";
    std::cout << "演进：StatefulAgent → TravelAgent\n\n";
    
    TravelAgent agent("travelpal");
    std::string user_id = "user_xiaoli";
    std::string user_name = "小李";
    
    std::cout << "【场景】用户 " << user_name << " 咨询旅行:\n";
    std::cout << "----------------------------------------\n\n";
    
    // 第一轮：表达意向
    std::string query = "你好，我想去日本旅游，预算1万左右，5天";
    std::cout << user_name << ": " << query << "\n";
    std::cout << "Agent: " << agent.handle_travel_query(user_id, user_name, query) << "\n\n";
    
    // 第二轮：提及特殊需求
    query = "我有海鲜过敏，脚伤刚恢复不能走太多路";
    std::cout << user_name << ": " << query << "\n";
    std::cout << "Agent: " << agent.handle_travel_query(user_id, user_name, query) << "\n\n";
    
    // 第三轮：请求推荐
    query = "推荐个目的地吧";
    std::cout << user_name << ": " << query << "\n";
    std::cout << "Agent: " << agent.handle_travel_query(user_id, user_name, query) << "\n\n";
    
    // 第四轮：行程规划
    query = "帮我规划行程";
    std::cout << user_name << ": " << query << "\n";
    std::cout << "Agent: " << agent.handle_travel_query(user_id, user_name, query) << "\n\n";
    
    // 第五轮：预算分配
    query = "预算怎么分配";
    std::cout << user_name << ": " << query << "\n";
    std::cout << "Agent: " << agent.handle_travel_query(user_id, user_name, query) << "\n\n";
    
    // 第六轮：美食推荐
    query = "有什么好吃的";
    std::cout << user_name << ": " << query << "\n";
    std::cout << "Agent: " << agent.handle_travel_query(user_id, user_name, query) << "\n";
    
    // 显示记忆
    std::cout << "\n【Agent 记忆状态】\n";
    agent.show_travel_memory();
    
    std::cout << "\n========================================\n";
    std::cout << "演进成果:\n";
    std::cout << "  ✓ 继承 StatefulAgent\n";
    std::cout << "  ✓ 添加旅行知识库\n";
    std::cout << "  ✓ 提取旅行偏好（预算/目的地/特殊需求）\n";
    std::cout << "  ✓ 意图识别（推荐/规划/预算/美食）\n";
    std::cout << "  ✓ 个性化推荐（根据用户情况调整）\n";
    std::cout << "\n下一步 → Step 18: 多 NPC 虚拟咖啡厅\n";
    
    return 0;
}