// ============================================================================
// main.cpp - Step 19: 高级功能（完整演示版）
// ============================================================================

#include <iostream>
#include "nuclaw/services/tenant_service.hpp"
#include "nuclaw/services/human_service.hpp"
#include "nuclaw/services/billing_service.hpp"

using namespace nuclaw;

int main() {
    std::cout << "========================================\n";
    std::cout << "Step 19: 高级功能\n";
    std::cout << "========================================\n";
    std::cout << "演进：Step 18 核心服务 + 3 个高级服务\n";
    std::cout << "  - HumanService: 人工客服转接\n";
    std::cout << "  - TenantService: 租户管理 API\n";
    std::cout << "  - BillingService: 计费与配额\n\n";
    
    // ============================================================
    // 演示 1：租户管理（TenantService）
    // ============================================================
    
    std::cout << "【演示 1】租户管理\n";
    std::cout << "----------------------------------------\n";
    
    TenantService tenant_service;
    
    // 创建不同套餐的租户
    std::string tenant_free = tenant_service.create_tenant(
        "小商店", "shop@example.com", TenantPlan::FREE);
    
    std::string tenant_pro = tenant_service.create_tenant(
        "科技公司", "tech@example.com", TenantPlan::PRO);
    
    std::string tenant_enterprise = tenant_service.create_tenant(
        "大型企业", "enterprise@example.com", TenantPlan::ENTERPRISE);
    
    // 查看租户配额
    std::cout << "\n租户配额对比:\n";
    auto quota_free = tenant_service.get_quota(tenant_free);
    auto quota_pro = tenant_service.get_quota(tenant_pro);
    auto quota_ent = tenant_service.get_quota(tenant_enterprise);
    
    std::cout << "  免费版: 并发=" << quota_free.max_concurrent_sessions
              << " 消息/月=" << quota_free.max_messages_per_month
              << " 人工客服=" << quota_free.max_human_agents << "\n";
    std::cout << "  专业版: 并发=" << quota_pro.max_concurrent_sessions
              << " 消息/月=" << quota_pro.max_messages_per_month
              << " 人工客服=" << quota_pro.max_human_agents << "\n";
    std::cout << "  企业版: 并发=" << quota_ent.max_concurrent_sessions
              << " 消息/月=" << quota_ent.max_messages_per_month
              << " 人工客服=" << quota_ent.max_human_agents << "\n";
    
    std::cout << "\n";
    
    // ============================================================
    // 演示 2：人工客服（HumanService）
    // ============================================================
    
    std::cout << "【演示 2】人工客服管理\n";
    std::cout << "----------------------------------------\n";
    
    HumanService human_service;
    
    // 注册人工客服
    human_service.register_agent(tenant_pro, "agent_001", "张客服");
    human_service.register_agent(tenant_pro, "agent_002", "李客服");
    human_service.register_agent(tenant_enterprise, "agent_003", "王专员");
    
    // 设置客服状态
    human_service.set_agent_status("agent_001", HumanAgentStatus::ONLINE);
    human_service.set_agent_status("agent_002", HumanAgentStatus::ONLINE);
    human_service.set_agent_status("agent_003", HumanAgentStatus::AWAY);
    
    // 模拟转接请求
    std::cout << "\n客户A: 我要投诉！转人工！\n";
    std::string request_id = human_service.request_escalation(
        "session_001", tenant_pro, "user_001", "客户要求投诉", 8);
    
    std::cout << "张客服: 您好，我是客服小张\n";
    human_service.handover_to_human("session_001", "agent_001");
    
    // 会话交回 AI
    human_service.return_to_ai("session_001");
    std::cout << "张客服: 问题已记录，AI 将继续为您服务\n";
    
    std::cout << "\n";
    
    // ============================================================
    // 演示 3：计费系统（BillingService）
    // ============================================================
    
    std::cout << "【演示 3】计费与配额\n";
    std::cout << "----------------------------------------\n";
    
    BillingService billing_service;
    
    // 模拟使用量
    std::cout << "\n模拟租户使用...\n";
    
    // 专业版租户使用
    for (int i = 0; i < 100; ++i) {
        billing_service.record_ai_message(tenant_pro, 500, 150);
    }
    
    // 企业版租户使用（更多）
    for (int i = 0; i < 500; ++i) {
        billing_service.record_ai_message(tenant_enterprise, 800, 200);
    }
    // 企业版使用人工客服
    for (int i = 0; i < 20; ++i) {
        billing_service.record_human_message(tenant_enterprise);
    }
    
    // 添加知识库文档
    billing_service.record_knowledge_document(tenant_pro, 5);
    billing_service.record_knowledge_document(tenant_enterprise, 50);
    
    // 检查配额
    std::cout << "\n配额检查:\n";
    auto check1 = billing_service.check_message_quota(
        tenant_pro, 100, quota_pro.max_messages_per_month);
    std::cout << "  专业版消息配额: " << (check1.allowed ? "✅ 正常" : "❌ 超限") << "\n";
    
    // 打印用量报告
    billing_service.print_tenant_report(tenant_pro);
    billing_service.print_tenant_report(tenant_enterprise);
    
    std::cout << "\n";
    
    // ============================================================
    // 演示 4：综合统计
    // ============================================================
    
    std::cout << "【演示 4】综合统计\n";
    std::cout << "----------------------------------------\n";
    
    tenant_service.print_stats();
    human_service.print_stats();
    billing_service.print_stats();
    
    // ============================================================
    // 演进总结
    // ============================================================
    
    std::cout << "\n========================================\n";
    std::cout << "Step 19 演进成果:\n";
    std::cout << "----------------------------------------\n";
    std::cout << "  ✅ 在 Step 18 基础上新增 3 个高级服务:\n";
    std::cout << "\n";
    std::cout << "  📞 HumanService（人工客服）:\n";
    std::cout << "     - 客服注册与状态管理\n";
    std::cout << "     - 转接队列（优先级）\n";
    std::cout << "     - 会话接管与交回\n";
    std::cout << "\n";
    std::cout << "  🏢 TenantService（租户管理）:\n";
    std::cout << "     - 租户 CRUD\n";
    std::cout << "     - 套餐管理（免费/基础/专业/企业）\n";
    std::cout << "     - 配额配置\n";
    std::cout << "     - 租户状态（激活/暂停）\n";
    std::cout << "\n";
    std::cout << "  💰 BillingService（计费系统）:\n";
    std::cout << "     - 用量统计（消息/Token/存储）\n";
    std::cout << "     - 实时计费\n";
    std::cout << "     - 配额检查与告警\n";
    std::cout << "\n";
    std::cout << "  📁 文件变更:\n";
    std::cout << "     复用: Step 18 全部文件\n";
    std::cout << "     新增: include/nuclaw/services/human_service.hpp\n";
    std::cout << "     新增: include/nuclaw/services/tenant_service.hpp\n";
    std::cout << "     新增: include/nuclaw/services/billing_service.hpp\n";
    std::cout << "     修改: src/main.cpp\n";
    std::cout << "\n";
    std::cout << "  🎯 下一步 → Step 20: 生产部署\n";
    std::cout << "     添加:\n";
    std::cout << "     - Docker Compose（多服务编排）\n";
    std::cout << "     - K8s 部署配置\n";
    std::cout << "     - 监控和日志\n";
    std::cout << "========================================\n";
    
    return 0;
}
