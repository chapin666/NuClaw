// ============================================================================
// customer_service_agent.hpp - 多租户客服 Agent（Step 17 演进）
// ============================================================================
// 演进说明：
//   基类: StatefulAgent (Step 16)
//   演进: 添加租户隔离、人机协作判断
//   
//   git diff 可见的变化：
//     + 继承 StatefulAgent 的全部能力（记忆、情感、关系）
//     + 添加 TenantContext 实现多租户隔离
//     + 添加人工转接判断（为 Step 19 HumanService 铺垫）
// ============================================================================

#pragma once
#include "nuclaw/stateful_agent.hpp"  // 复用 Step 16
#include "nuclaw/tenant.hpp"           // 新增：租户上下文
#include <boost/json.hpp>
#include <map>
#include <memory>

namespace json = boost::json;

namespace nuclaw {

// 客服对话类型
enum class ConversationType {
    GREETING,       // 问候
    QUESTION,       // 问题咨询
    COMPLAINT,      // 投诉
    TASK_REQUEST,   // 任务请求
    SMALL_TALK,     // 闲聊
    ESCALATION      // 需要人工
};

// 客服 Agent（演进自 Step 16 StatefulAgent）
class CustomerServiceAgent : public StatefulAgent {
public:
    // 构造函数：必须传入租户上下文（Step 17 核心变化）
    CustomerServiceAgent(const TenantContext& tenant)
        : StatefulAgent(tenant.get_agent_name(), "客服Agent"),
          tenant_ctx_(tenant) {
        
        std::cout << "[初始化] 租户: " << tenant.tenant_name 
                  << " (" << tenant.tenant_id << ")\n";
        std::cout << "         Agent: " << tenant.get_agent_name()
                  << " | 风格: " << tenant.get_agent_style()
                  << " | 领域: " << tenant.get_domain() << "\n\n";
    }
    
    // 获取租户信息
    const TenantContext& get_tenant_context() const { return tenant_ctx_; }
    std::string get_tenant_name() const { return tenant_ctx_.tenant_name; }
    std::string get_tenant_id() const { return tenant_ctx_.tenant_id; }
    
    // Step 17 核心：带租户隔离的消息处理
    std::string handle_customer_message(const std::string& user_id,
                                         const std::string& user_name,
                                         const std::string& message) {
        // 生成租户隔离的用户ID
        TenantUserId tu{tenant_ctx_.tenant_id, user_id};
        std::string global_user_id = tu.to_global_id();
        
        // Step 1: 分类用户意图
        auto conv_type = classify_intent(message);
        
        // Step 2: 判断是否转人工（Step 17 新增能力）
        if (should_escalate_to_human(conv_type, user_id, message)) {
            return escalate_to_human(user_name, message);
        }
        
        // Step 3: 调用父类方法处理（复用 Step 16 的记忆和情感）
        std::string response = process_user_message(global_user_id, user_name, message);
        
        // Step 4: 添加租户定制的欢迎语/结束语
        response = customize_response(response, conv_type);
        
        return response;
    }
    
    // Step 17 新增：人工转接判断（为 Step 19 铺垫）
    bool should_escalate_to_human(ConversationType conv_type,
                                   const std::string& user_id, 
                                   const std::string& message) {
        // 规则 1: 用户明确要求人工
        if (message.find("人工") != std::string::npos ||
            message.find("客服") != std::string::npos ||
            message.find("找真人") != std::string::npos) {
            return true;
        }
        
        // 规则 2: 投诉类型
        if (conv_type == ConversationType::COMPLAINT) {
            return true;
        }
        
        // 规则 3: 情绪极化（投诉或愤怒）- 使用 happiness < 0 代替 is_negative()
        auto& rel = state_.relationships.get_or_create(tenant_ctx_.tenant_id + ":" + user_id);
        if (state_.emotion.happiness < -3 && rel.interaction_count > 3) {
            return true;
        }
        
        // 规则 4: 复杂问题（为 Step 18 RAG 铺垫）
        if (is_complex_question(message)) {
            return false;  // 先尝试知识库，Step 18 实现
        }
        
        return false;
    }
    
    // Step 17 新增：获取会话摘要（为数据分析铺垫）
    json::object get_session_summary(const std::string& user_id) const {
        TenantUserId tu{tenant_ctx_.tenant_id, user_id};
        std::string global_id = tu.to_global_id();
        
        // relationships.get 返回指针，需要检查 null
        const Relationship* rel = state_.relationships.get(global_id);
        auto memories = state_.short_term_memory.get_by_user(global_id);
        
        json::object summary;
        summary["tenant_id"] = tenant_ctx_.tenant_id;
        summary["user_id"] = user_id;
        
        if (rel) {
            summary["user_name"] = rel->user_name;
            summary["interaction_count"] = rel->interaction_count;
            summary["familiarity"] = rel->familiarity;
        } else {
            summary["user_name"] = "";
            summary["interaction_count"] = 0;
            summary["familiarity"] = 0.0f;
        }
        
        summary["message_count"] = memories.size();
        summary["emotion_state"] = state_.emotion.describe();
        
        return summary;
    }
    
    // 显示租户信息
    void show_tenant_info() const {
        std::cout << "\n🏢 租户信息:\n";
        std::cout << "  ID: " << tenant_ctx_.tenant_id << "\n";
        std::cout << "  名称: " << tenant_ctx_.tenant_name << "\n";
        std::cout << "  Agent: " << tenant_ctx_.get_agent_name() << "\n";
        std::cout << "  风格: " << tenant_ctx_.get_agent_style() << "\n";
    }

protected:
    TenantContext tenant_ctx_;  // Step 17 核心新增：租户上下文
    
    // 意图分类
    ConversationType classify_intent(const std::string& message) {
        if (message.find("你好") != std::string::npos || 
            message.find("您好") != std::string::npos ||
            message.find("hi") != std::string::npos) {
            return ConversationType::GREETING;
        }
        if (message.find("投诉") != std::string::npos ||
            message.find("不满") != std::string::npos ||
            message.find("差评") != std::string::npos) {
            return ConversationType::COMPLAINT;
        }
        if (message.find("?") != std::string::npos ||
            message.find("？") != std::string::npos ||
            message.find("怎么") != std::string::npos) {
            return ConversationType::QUESTION;
        }
        return ConversationType::SMALL_TALK;
    }
    
    // 转人工处理
    std::string escalate_to_human(const std::string& user_name, 
                                   const std::string& reason) {
        (void)user_name;  // 暂时未使用
        (void)reason;
        return "[" + tenant_ctx_.tenant_name + "] 为您转接人工客服，请稍候...\n"
               "(系统: 已创建工单，客服将在 2 分钟内接入)";
    }
    
    // 复杂问题判断（Step 18 会扩展）
    bool is_complex_question(const std::string& message) {
        return message.length() > 50 || 
               message.find("详细") != std::string::npos ||
               message.find("流程") != std::string::npos;
    }
    
    // 租户定制回复
    std::string customize_response(const std::string& base_response,
                                    ConversationType type) {
        // 问候语定制
        if (type == ConversationType::GREETING) {
            return "欢迎联系" + tenant_ctx_.tenant_name + "！\n" 
                   + tenant_ctx_.config.welcome_message + "\n\n"
                   + base_response;
        }
        return base_response;
    }
};

} // namespace nuclaw
