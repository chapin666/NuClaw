// src/main.cpp - Step 19: SaaS 化 AI 助手平台演示
#include "saas/simple_tenant.hpp"
#include <iostream>
#include <string>

using namespace saas;

void print_menu() {
    std::cout << "\n🏢 SaaS AI 平台管理\n";
    std::cout << "==================\n";
    std::cout << "1. 创建租户\n";
    std::cout << "2. 列出所有租户\n";
    std::cout << "3. 添加知识库\n";
    std::cout << "4. 模拟用户对话\n";
    std::cout << "5. 退出\n";
    std::cout << "\n选择: ";
}

int main() {
    srand(time(nullptr));
    
    TenantManager tenant_manager;
    ChatService chat_service(tenant_manager);
    
    std::cout << "🏢 SaaS AI 平台演示\n";
    std::cout << "==================\n\n";
    
    // 预创建一些租户
    auto tenant_a = tenant_manager.create_tenant("示例电商");
    auto tenant_b = tenant_manager.create_tenant("示例教育");
    
    // 为租户 A 添加知识
    tenant_manager.get_tenant(tenant_a.api_key)->knowledge_base.push_back(
        "我们支持7天无理由退货");
    tenant_manager.get_tenant(tenant_a.api_key)->knowledge_base.push_back(
        "满99元包邮");
    
    // 为租户 B 添加知识
    tenant_manager.get_tenant(tenant_b.api_key)->knowledge_base.push_back(
        "我们的课程有Python、Java、C++");
    
    std::cout << "已预创建两个租户:\n";
    std::cout << "  租户A: " << tenant_a.name << " (API Key: " << tenant_a.api_key << ")\n";
    std::cout << "  租户B: " << tenant_b.name << " (API Key: " << tenant_b.api_key << ")\n";
    
    int choice;
    while (true) {
        print_menu();
        std::cin >> choice;
        std::cin.ignore();
        
        if (choice == 1) {
            std::cout << "输入租户名称: ";
            std::string name;
            std::getline(std::cin, name);
            
            auto tenant = tenant_manager.create_tenant(name);
            std::cout << "\n✅ 创建成功！\n";
            std::cout << "租户ID: " << tenant.tenant_id << "\n";
            std::cout << "API Key: " << tenant.api_key << "\n";
        }
        else if (choice == 2) {
            auto tenants = tenant_manager.list_all();
            std::cout << "\n📋 租户列表:\n";
            for (const auto& t : tenants) {
                std::cout << "  • " << t.name << "\n";
                std::cout << "    ID: " << t.tenant_id << "\n";
                std::cout << "    用量: " << t.used_conversations << "/" << t.max_conversations << "\n";
            }
        }
        else if (choice == 3) {
            std::cout << "输入 API Key: ";
            std::string api_key;
            std::getline(std::cin, api_key);
            
            Tenant* tenant = tenant_manager.get_tenant(api_key);
            if (!tenant) {
                std::cout << "❌ 无效的 API Key\n";
                continue;
            }
            
            std::cout << "输入知识内容: ";
            std::string knowledge;
            std::getline(std::cin, knowledge);
            
            tenant->knowledge_base.push_back(knowledge);
            std::cout << "✅ 知识已添加\n";
        }
        else if (choice == 4) {
            std::cout << "输入 API Key: ";
            std::string api_key;
            std::getline(std::cin, api_key);
            
            std::cout << "输入用户消息: ";
            std::string message;
            std::getline(std::cin, message);
            
            ChatService::ChatRequest req;
            req.api_key = api_key;
            req.message = message;
            
            auto response = chat_service.chat(req);
            
            if (response.success) {
                std::cout << "\n🤖 回复: " << response.message << "\n";
                std::cout << "💳 剩余配额: " << response.remaining_quota << "\n";
            } else {
                std::cout << "\n❌ 错误: " << response.error << "\n";
            }
        }
        else if (choice == 5) {
            std::cout << "\n再见！\n";
            break;
        }
    }
    
    return 0;
}