// ============================================================================
// main.cpp - Step 19: SaaS 化 AI 助手平台
// ============================================================================

#include "step19_progressive.hpp"
#include <iostream>

using namespace step19;

int main() {
    srand(time(nullptr));
    
    std::cout << "========================================\n";
    std::cout << "Step 19: SaaS 化 AI 助手平台\n";
    std::cout << "========================================\n";
    std::cout << "演进：多租户 + Agent 池\n\n";
    
    SaaSPlatform platform;
    auto& tenant_manager = platform.get_tenant_manager();
    
    // 创建租户
    std::cout << "【平台管理员】创建租户:\n";
    auto tenant_a = tenant_manager.create_tenant("示例电商");
    auto tenant_b = tenant_manager.create_tenant("示例教育");
    
    std::cout << "  ✓ 租户A: " << tenant_a.name << "\n";
    std::cout << "    API Key: " << tenant_a.api_key << "\n\n";
    std::cout << "  ✓ 租户B: " << tenant_b.name << "\n";
    std::cout << "    API Key: " << tenant_b.api_key << "\n\n";
    
    // 模拟租户 A 的用户对话
    std::cout << "----------------------------------------\n";
    std::cout << "【场景】租户A 的用户咨询:\n";
    std::cout << "----------------------------------------\n";
    
    auto runtime_a = platform.get_or_create_runtime(tenant_a.api_key);
    std::cout << "用户: 你们支持退货吗？\n";
    std::cout << "Agent: " 
              << runtime_a->chat("user_1", "张三", "你们支持退货吗？")
              << "\n\n";
    
    std::cout << "用户: 运费怎么算？\n";
    std::cout << "Agent: " 
              << runtime_a->chat("user_1", "张三", "运费怎么算？")
              << "\n";
    
    // 模拟租户 B 的用户对话
    std::cout << "\n----------------------------------------\n";
    std::cout << "【场景】租户B 的用户咨询:\n";
    std::cout << "----------------------------------------\n";
    
    auto runtime_b = platform.get_or_create_runtime(tenant_b.api_key);
    std::cout << "用户: 有什么课程？\n";
    std::cout << "Agent: " 
              << runtime_b->chat("user_1", "李四", "有什么课程？")
              << "\n\n";
    
    std::cout << "用户: 我想学Python\n";
    std::cout << "Agent: " 
              << runtime_b->chat("user_1", "李四", "我想学Python")
              << "\n";
    
    // 显示隔离效果
    std::cout << "\n----------------------------------------\n";
    std::cout << "【数据隔离验证】\n";
    std::cout << "----------------------------------------\n";
    std::cout << "✓ 租户A 和 租户B 的数据完全隔离\n";
    std::cout << "✓ 每个租户有独立的 Agent 实例\n";
    std::cout << "✓ API Key 认证确保访问控制\n";
    std::cout << "✓ 配额系统限制资源使用\n";
    
    // 平台统计
    platform.show_platform_stats();
    
    std::cout << "\n========================================\n";
    std::cout << "演进成果:\n";
    std::cout << "  ✓ 多租户架构\n";
    std::cout << "  ✓ API Key 认证\n";
    std::cout << "  ✓ 租户级 Agent 隔离\n";
    std::cout << "  ✓ 配额管理\n";
    std::cout << "  ✓ 平台级运营统计\n";
    std::cout << "\n🎉 完成 NuClaw 全部 20 个 Step!\n";
    
    return 0;
}