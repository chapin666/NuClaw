// ============================================================================
// ai_service.hpp - Step 18: AI 智能回复服务
// ============================================================================
// 演进说明：
//   从 Step 17 CustomerServiceAgent 提取 AI 能力
//   
//   Step 17: CustomerServiceAgent 直接继承 StatefulAgent，处理完整对话
//   Step 18: AIService 专注于 LLM 调用和智能回复生成
//
//   演进方式：
//     - 复用 Step 17 CustomerServiceAgent 的情感/记忆/关系逻辑
//     - 新增 KnowledgeService 集成（RAG 检索）
//     - 支持多租户上下文注入
// ============================================================================

#pragma once
#include "nuclaw/customer_service_agent.hpp"
#include "nuclaw/tenant.hpp"
#include "nuclaw/services/knowledge_service.hpp"
#include <boost/asio.hpp>
#include <memory>
#include <map>
#include <mutex>

namespace nuclaw {

// 前置声明
class KnowledgeService;

// LLM 配置
struct LLMConfig {
    std::string provider = "openai";  // openai, anthropic, moonshot
    std::string model = "gpt-3.5-turbo";
    std::string api_key;
    float temperature = 0.7f;
    int max_tokens = 1000;
};

// AI 服务：处理智能回复生成（Step 18 核心新增）
class AIService {
public:
    AIService(boost::asio::io_context& io,
              const LLMConfig& config,
              std::shared_ptr<KnowledgeService> knowledge_service)
        : io_(io), config_(config), knowledge_service_(knowledge_service) {}
    
    // 核心：生成智能回复（演进自 Step 17 CustomerServiceAgent）
    std::string generate_response(const std::string& tenant_id,
                                   const std::string& user_id,
                                   const std::string& user_name,
                                   const std::string& message,
                                   const std::vector<std::pair<std::string, std::string>>& history) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // 获取租户 Agent
        auto it = tenant_agents_.find(tenant_id);
        if (it == tenant_agents_.end()) {
            return "[错误] 租户未注册: " + tenant_id;
        }
        
        // Step 1: 检索知识库（RAG）
        std::vector<std::string> knowledge_chunks;
        if (knowledge_service_) {
            auto results = knowledge_service_->retrieve(tenant_id, message, 3);
            for (const auto& r : results) {
                knowledge_chunks.push_back(r.content);
            }
        }
        
        // Step 2: 调用父类方法生成回复（复用 Step 17 能力）
        std::string response = it->second->handle_customer_message(user_id, user_name, message);
        
        // Step 3: 如果有知识库结果，增强回复
        if (!knowledge_chunks.empty()) {
            response += "\n[参考知识库] " + knowledge_chunks[0].substr(0, 50) + "...";
        }
        
        return response;
    }
    
    // 判断是否需要转人工（复用 Step 17 逻辑）
    bool should_escalate_to_human(const std::string& tenant_id,
                                   const std::string& user_id,
                                   const std::string& message) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = tenant_agents_.find(tenant_id);
        if (it != tenant_agents_.end()) {
            // 简化：只检查关键词
            return message.find("人工") != std::string::npos ||
                   message.find("投诉") != std::string::npos;
        }
        return false;
    }
    
    // 获取 Agent 状态（用于监控）
    struct AgentStatus {
        std::string tenant_id;
        std::string emotion_state;
        size_t memory_count;
        size_t relationship_count;
    };
    
    std::vector<AgentStatus> get_all_status() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<AgentStatus> statuses;
        for (const auto& [tenant_id, agent] : tenant_agents_) {
            statuses.push_back({
                tenant_id,
                agent->get_state().emotion.describe(),
                agent->get_state().short_term_memory.size(),
                agent->get_state().relationships.count()
            });
        }
        return statuses;
    }
    
    // 注册租户 Agent（每个租户一个 Agent 实例）
    void register_tenant_agent(const TenantContext& tenant) {
        std::lock_guard<std::mutex> lock(mutex_);
        tenant_agents_[tenant.tenant_id] = std::make_unique<CustomerServiceAgent>(tenant);
        std::cout << "[AIService] 注册租户 Agent: " << tenant.tenant_name << "\n";
    }

private:
    boost::asio::io_context& io_;
    LLMConfig config_;
    std::shared_ptr<KnowledgeService> knowledge_service_;
    
    // 每个租户一个 CustomerServiceAgent（复用 Step 17）
    std::map<std::string, std::unique_ptr<CustomerServiceAgent>> tenant_agents_;
    mutable std::mutex mutex_;
    
    // 模拟 LLM 调用（Step 18 简化版，Step 19 接入真实 API）
    std::string call_llm(const std::string& prompt);
    
    // 构建 Prompt（注入租户上下文和历史）
    std::string build_prompt(const TenantContext& tenant,
                             const std::string& user_name,
                             const std::string& message,
                             const std::vector<std::pair<std::string, std::string>>& history,
                             const std::vector<std::string>& knowledge_chunks);
};

} // namespace nuclaw
