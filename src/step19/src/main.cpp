// ============================================================================
// main.cpp - Step 18: 核心服务拆分
// ============================================================================
// 演进说明：
//   基于 Step 17 CustomerServiceAgent，拆分为三个微服务
//   
//   演进对比：
//     Step 17: Application → CustomerServiceAgent (单类处理全部)
//     Step 18: Application → ChatService + AIService + KnowledgeService (职责分离)
//
//   职责划分：
//     - ChatService:    WebSocket 连接、会话管理、消息路由
//     - AIService:      LLM 调用、智能回复生成（复用 CustomerServiceAgent）
//     - KnowledgeService: 向量知识库、RAG 检索（复用 Step 10 VectorStore）
// ============================================================================

#include "nuclaw/services/chat_service.hpp"
#include "nuclaw/services/ai_service.hpp"
#include "nuclaw/services/knowledge_service.hpp"
#include <iostream>

using namespace nuclaw;

int main() {
    std::cout << "========================================\n";
    std::cout << "Step 18: 核心服务拆分\n";
    std::cout << "========================================\n";
    std::cout << "演进：Step 17 单体 Agent → 3 个微服务\n";
    std::cout << "  - ChatService: 会话管理\n";
    std::cout << "  - AIService: 智能回复（复用 CustomerServiceAgent）\n";
    std::cout << "  - KnowledgeService: 知识库（复用 Step 10 RAG）\n\n";
    
    // ============================================================
    // 初始化服务（依赖注入）
    // ============================================================
    
    boost::asio::io_context io;
    
    // 1. KnowledgeService（最底层，无依赖）
    std::cout << "【初始化】KnowledgeService\n";
    KnowledgeBaseConfig kb_config;
    auto knowledge_service = std::make_shared<KnowledgeService>(kb_config);
    
    // 2. AIService（依赖 KnowledgeService）
    std::cout << "【初始化】AIService\n";
    LLMConfig llm_config;
    llm_config.model = "gpt-3.5-turbo";
    auto ai_service = std::make_shared<AIService>(io, llm_config, knowledge_service);
    
    // 3. ChatService（依赖 AIService）
    std::cout << "【初始化】ChatService\n";
    auto chat_service = std::make_shared<ChatService>(io, ai_service);
    
    std::cout << "\n服务初始化完成！\n\n";
    
    // ============================================================
    // 演示 1：初始化租户
    // ============================================================
    
    std::cout << "【演示 1】初始化租户数据\n";
    std::cout << "----------------------------------------\n";
    
    // 租户 A：快购商城
    TenantContext tenant_a("tenant_ecommerce", "快购商城");
    tenant_a.agent_persona = {{"name", "小快"}, {"style", "热情"}};
    
    // 注册到各服务
    knowledge_service->initialize_tenant_kb(tenant_a.tenant_id);
    ai_service->register_tenant_agent(tenant_a);
    
    // 添加知识库文档
    knowledge_service->add_document(
        tenant_a.tenant_id,
        "faq_shipping",
        "我们支持全国包邮，下单后 24 小时内发货，偏远地区 3-5 天到达。"
    );
    knowledge_service->add_document(
        tenant_a.tenant_id,
        "faq_return",
        "7 天无理由退货，质量问题免运费，退款将在 3 个工作日内到账。"
    );
    
    // 租户 B：智慧学堂
    TenantContext tenant_b("tenant_edu", "智慧学堂");
    tenant_b.agent_persona = {{"name", "小智"}, {"style", "耐心"}};
    
    knowledge_service->initialize_tenant_kb(tenant_b.tenant_id);
    ai_service->register_tenant_agent(tenant_b);
    
    knowledge_service->add_document(
        tenant_b.tenant_id,
        "course_python",
        "Python 入门课程共 20 课时，从基础语法到实战项目，适合零基础学员。"
    );
    
    std::cout << "\n";
    
    // ============================================================
    // 演示 2：客户连接和对话
    // ============================================================
    
    std::cout << "【演示 2】客户连接和对话流程\n";
    std::cout << "----------------------------------------\n\n";
    
    // 客户 A 连接快购商城
    std::cout << "【快购商城】客户接入\n";
    std::string conn_a = chat_service->connect_customer(
        tenant_a.tenant_id, "user_001", "小明");
    
    std::cout << "小明: 你好，请问发货要多久？\n";
    std::string resp_a1 = chat_service->handle_message(conn_a, "你好，请问发货要多久？");
    std::cout << "小快: " << resp_a1 << "\n\n";
    
    // 客户 B 连接智慧学堂
    std::cout << "【智慧学堂】客户接入\n";
    std::string conn_b = chat_service->connect_customer(
        tenant_b.tenant_id, "user_001", "小明");  // 同 user_id，不同租户
    
    std::cout << "小明: 我想学 Python\n";
    std::string resp_b1 = chat_service->handle_message(conn_b, "我想学 Python");
    std::cout << "小智: " << resp_b1 << "\n\n";
    
    // ============================================================
    // 演示 3：知识库检索
    // ============================================================
    
    std::cout << "【演示 3】知识库检索（RAG）\n";
    std::cout << "----------------------------------------\n";
    
    auto results_a = knowledge_service->retrieve(tenant_a.tenant_id, "发货时间", 2);
    std::cout << "租户 A 检索 '发货时间':\n";
    for (const auto& r : results_a) {
        std::cout << "  [得分: " << r.score << "] " << r.content.substr(0, 30) << "...\n";
    }
    
    auto results_b = knowledge_service->retrieve(tenant_b.tenant_id, "Python 课程", 2);
    std::cout << "\n租户 B 检索 'Python 课程':\n";
    for (const auto& r : results_b) {
        std::cout << "  [得分: " << r.score << "] " << r.content.substr(0, 30) << "...\n";
    }
    
    std::cout << "\n";
    
    // ============================================================
    // 演示 4：服务统计
    // ============================================================
    
    std::cout << "【演示 4】服务统计\n";
    std::cout << "----------------------------------------\n";
    
    chat_service->print_stats();
    
    std::cout << "\n[KnowledgeService 统计]\n";
    auto kb_stats = knowledge_service->get_stats();
    for (const auto& s : kb_stats) {
        std::cout << "  租户 " << s.tenant_id << ": " 
                  << s.document_count << " 文档\n";
    }
    
    // ============================================================
    // 演进总结
    // ============================================================
    
    std::cout << "\n========================================\n";
    std::cout << "Step 18 演进成果:\n";
    std::cout << "----------------------------------------\n";
    std::cout << "  ✅ 从 Step 17 单体架构演进为微服务:\n";
    std::cout << "     - CustomerServiceAgent (单体)\n";
    std::cout << "       ↓ 拆分\n";
    std::cout << "     - ChatService (会话管理)\n";
    std::cout << "     - AIService (智能回复)\n";
    std::cout << "     - KnowledgeService (知识库)\n";
    std::cout << "\n";
    std::cout << "  ✅ 职责分离带来的好处:\n";
    std::cout << "     - ChatService 可独立扩展（WebSocket 连接数）\n";
    std::cout << "     - AIService 可独立升级（LLM 模型切换）\n";
    std::cout << "     - KnowledgeService 可独立优化（向量检索）\n";
    std::cout << "\n";
    std::cout << "  ✅ 复用 Step 17 核心能力:\n";
    std::cout << "     - CustomerServiceAgent（情感、记忆、关系）\n";
    std::cout << "     - 多租户隔离机制\n";
    std::cout << "\n";
    std::cout << "  📁 文件变更:\n";
    std::cout << "     复用: include/nuclaw/customer_service_agent.hpp (Step 17)\n";
    std::cout << "     复用: include/nuclaw/tenant.hpp (Step 17)\n";
    std::cout << "     复用: include/nuclaw/vector_store.hpp (Step 10)\n";
    std::cout << "     新增: include/nuclaw/services/chat_service.hpp\n";
    std::cout << "     新增: include/nuclaw/services/ai_service.hpp\n";
    std::cout << "     新增: include/nuclaw/services/knowledge_service.hpp\n";
    std::cout << "     修改: src/main.cpp\n";
    std::cout << "\n";
    std::cout << "  🎯 下一步 → Step 19: 高级功能\n";
    std::cout << "     添加:\n";
    std::cout << "     - HumanService（人工客服转接）\n";
    std::cout << "     - TenantService（租户管理 API）\n";
    std::cout << "     - BillingService（计费系统）\n";
    std::cout << "========================================\n";
    
    return 0;
}
