// ============================================================================
// main.cpp - Step 17: 多租户智能客服平台
// ============================================================================
// 演进说明：
//   基于 Step 16 的 StatefulAgent，添加多租户支持
//   
//   核心变化：
//     - Step 16: 单 Agent 服务所有用户（无隔离）
//     - Step 17: 多 Agent 实例，每个租户独立（数据隔离+定制化）
//
//   演示重点：
//     1. 同一用户在不同租户下的完全隔离体验
//     2. 每个租户有独立的 Agent 人设和配置
//     3. 人工转接判断（基于情绪+关键词）
// ============================================================================

#include "nuclaw/customer_service_agent.hpp"
#include <iostream>

using namespace nuclaw;

int main() {
    std::cout << "========================================\n";
    std::cout << "Step 17: 多租户智能客服平台\n";
    std::cout << "========================================\n";
    std::cout << "演进：Step 16 StatefulAgent → 多租户 CustomerServiceAgent\n";
    std::cout << "关键变化：添加 TenantContext 实现数据隔离和定制化\n\n";
    
    // ============================================================
    // 演示准备：创建两个租户
    // ============================================================
    
    // 租户 A：电商公司（快购商城）
    TenantContext tenant_a("tenant_ecommerce_001", "快购商城");
    tenant_a.agent_persona = {
        {"name", "小快"},
        {"style", "热情、高效、简洁"},
        {"domain", "电商客服"}
    };
    tenant_a.config.welcome_message = "欢迎光临快购商城！我是您的专属客服小快，很高兴为您服务~";
    
    // 租户 B：教育机构（智慧学堂）
    TenantContext tenant_b("tenant_edu_001", "智慧学堂");
    tenant_b.agent_persona = {
        {"name", "小智"},
        {"style", "耐心、专业、鼓励式"},
        {"domain", "教育咨询"}
    };
    tenant_b.config.welcome_message = "您好！欢迎来到智慧学堂，我是小智老师，有什么可以帮您的吗？";
    
    // 创建两个 Agent 实例（多租户核心）
    std::cout << "【初始化】创建两个租户 Agent 实例\n";
    std::cout << "----------------------------------------\n";
    CustomerServiceAgent agent_a(tenant_a);
    CustomerServiceAgent agent_b(tenant_b);
    
    std::cout << "\n";
    
    // ============================================================
    // 演示 1：同一用户在不同租户下的隔离体验
    // ============================================================
    
    std::cout << "【演示 1】同一用户（小明）分别咨询两个租户\n";
    std::cout << "----------------------------------------\n\n";
    
    std::string user_id = "user_13800138000";  // 同一手机号用户
    std::string user_name = "小明";
    
    // 场景 A：小明咨询快购商城（购物问题）
    std::cout << "【快购商城 - 购物咨询】\n";
    std::cout << "小明: 你好，我想买东西\n";
    std::cout << "小快: " << agent_a.handle_customer_message(user_id, user_name, 
                                                          "你好，我想买东西") << "\n\n";
    
    std::cout << "小明: 你们有什么优惠活动？\n";
    std::cout << "小快: " << agent_a.handle_customer_message(user_id, user_name, 
                                                          "你们有什么优惠活动？") << "\n\n";
    
    // 场景 B：同一个小明咨询智慧学堂（教育问题）
    std::cout << "【智慧学堂 - 课程咨询】\n";
    std::cout << "小明: 你好，我想买东西\n";
    std::cout << "小智: " << agent_b.handle_customer_message(user_id, user_name, 
                                                          "你好，我想买东西") << "\n\n";
    
    std::cout << "小明: 你们有什么课程？\n";
    std::cout << "小智: " << agent_b.handle_customer_message(user_id, user_name, 
                                                          "你们有什么课程？") << "\n\n";
    
    // ============================================================
    // 演示 2：验证数据隔离
    // ============================================================
    
    std::cout << "【演示 2】验证数据隔离 - 查看小明的会话记录\n";
    std::cout << "----------------------------------------\n\n";
    
    std::cout << "📊 快购商城的小明会话摘要:\n";
    auto summary_a = agent_a.get_session_summary(user_id);
    std::cout << "   租户: " << std::string(summary_a["tenant_id"].as_string()) << "\n";
    std::cout << "   互动次数: " << summary_a["interaction_count"].to_number<int>() << "\n";
    std::cout << "   消息数: " << summary_a["message_count"].to_number<int>() << "\n";
    
    std::cout << "\n📊 智慧学堂的小明会话摘要:\n";
    auto summary_b = agent_b.get_session_summary(user_id);
    std::cout << "   租户: " << std::string(summary_b["tenant_id"].as_string()) << "\n";
    std::cout << "   互动次数: " << summary_b["interaction_count"].to_number<int>() << "\n";
    std::cout << "   消息数: " << summary_b["message_count"].to_number<int>() << "\n";
    
    std::cout << "\n✅ 结论：同一 user_id 在不同租户下数据完全隔离！\n\n";
    
    // ============================================================
    // 演示 3：人工转接判断
    // ============================================================
    
    std::cout << "【演示 3】人工转接判断\n";
    std::cout << "----------------------------------------\n\n";
    
    // 创建新用户进行负面情绪积累
    std::string angry_user = "user_angry_001";
    std::string angry_name = "愤怒的用户";
    
    std::cout << "用户: 我要投诉！你们的服务太差了！\n";
    std::cout << "小快: " << agent_a.handle_customer_message(angry_user, angry_name,
                                                          "我要投诉！你们的服务太差了！") << "\n\n";
    
    std::cout << "用户: 我要找人工客服！\n";
    std::cout << "小快: " << agent_a.handle_customer_message(angry_user, angry_name,
                                                          "我要找人工客服！") << "\n\n";
    
    // ============================================================
    // 演示 4：Agent 内部状态（复用 Step 16 能力）
    // ============================================================
    
    std::cout << "【演示 4】Agent 内部状态（继承 Step 16）\n";
    std::cout << "----------------------------------------\n";
    agent_a.show_state();
    agent_a.show_tenant_info();
    
    // ============================================================
    // 演进总结
    // ============================================================
    
    std::cout << "\n========================================\n";
    std::cout << "Step 17 演进成果:\n";
    std::cout << "----------------------------------------\n";
    std::cout << "  ✅ 继承 Step 16 全部能力:\n";
    std::cout << "     - StatefulAgent 基类\n";
    std::cout << "     - 记忆系统（短期记忆）\n";
    std::cout << "     - 情感状态\n";
    std::cout << "     - 用户关系\n";
    std::cout << "\n";
    std::cout << "  ✅ Step 17 新增能力:\n";
    std::cout << "     - TenantContext 多租户隔离\n";
    std::cout << "     - 租户定制化（人设、欢迎语）\n";
    std::cout << "     - 人工转接判断\n";
    std::cout << "     - 会话摘要导出\n";
    std::cout << "\n";
    std::cout << "  📁 文件变更:\n";
    std::cout << "     复用: include/nuclaw/stateful_agent.hpp (Step 16)\n";
    std::cout << "     复用: include/nuclaw/memory.hpp (Step 16)\n";
    std::cout << "     复用: include/nuclaw/emotion.hpp (Step 16)\n";
    std::cout << "     新增: include/nuclaw/tenant.hpp\n";
    std::cout << "     新增: include/nuclaw/customer_service_agent.hpp\n";
    std::cout << "     修改: src/main.cpp\n";
    std::cout << "\n";
    std::cout << "  🎯 下一步 → Step 18: 服务拆分\n";
    std::cout << "     将 CustomerServiceAgent 拆分为:\n";
    std::cout << "     - ChatService（会话管理）\n";
    std::cout << "     - AIService（LLM 调用）\n";
    std::cout << "     - KnowledgeService（知识库）\n";
    std::cout << "========================================\n";
    
    return 0;
}
