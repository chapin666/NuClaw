// ============================================================================
// human_service.hpp - Step 19: 人工客服服务
// ============================================================================
// 演进说明：
//   从 Step 18 AIService 的 should_escalate_to_human() 演进为完整服务
//   
//   Step 18: AIService 只判断是否需要转人工，但不执行转接
//   Step 19: HumanService 实现完整的人机协作流程
//
//   新增功能：
//     - 人工客服工作台（客服状态管理）
//     - 转接队列（优先级排队）
//     - 会话接管（人工介入/AI 接管）
// ============================================================================

#pragma once
#include "nuclaw/tenant.hpp"
#include <boost/asio.hpp>
#include <memory>
#include <map>
#include <queue>
#include <mutex>
#include <set>

namespace nuclaw {

// 人工客服状态
enum class HumanAgentStatus {
    OFFLINE,      // 离线
    ONLINE,       // 在线（可分配）
    BUSY,         // 繁忙（已分配会话）
    AWAY          // 离开（暂停接新会话）
};

// 人工客服
struct HumanAgent {
    std::string agent_id;
    std::string name;
    std::string tenant_id;
    HumanAgentStatus status;
    std::set<std::string> assigned_sessions;  // 当前分配的会话
    std::chrono::steady_clock::time_point last_activity;
    
    HumanAgent(const std::string& id, const std::string& n, const std::string& t)
        : agent_id(id), name(n), tenant_id(t), status(HumanAgentStatus::OFFLINE) {}
};

// 转接请求
struct EscalationRequest {
    std::string request_id;
    std::string session_id;
    std::string tenant_id;
    std::string user_id;
    std::string reason;  // 转人工原因
    int priority;        // 优先级（1-10，数字越大越优先）
    std::chrono::steady_clock::time_point created_at;
    
    EscalationRequest(const std::string& req_id, const std::string& sid,
                      const std::string& tid, const std::string& uid,
                      const std::string& r, int p)
        : request_id(req_id), session_id(sid), tenant_id(tid),
          user_id(uid), reason(r), priority(p),
          created_at(std::chrono::steady_clock::now()) {}
};

// 会话接管状态
struct SessionHandover {
    std::string session_id;
    std::string human_agent_id;   // 人工客服ID（空表示AI处理）
    bool is_ai_paused;            // AI 是否暂停
    std::chrono::steady_clock::time_point handover_time;
};

// HumanService: 人工客服管理（Step 19 新增）
class HumanService {
public:
    HumanService() = default;
    
    // ============================================================
    // 人工客服管理
    // ============================================================
    
    // 注册人工客服
    void register_agent(const std::string& tenant_id,
                        const std::string& agent_id,
                        const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        agents_[agent_id] = std::make_unique<HumanAgent>(agent_id, name, tenant_id);
        std::cout << "[HumanService] 注册人工客服: " << name << " (租户: " << tenant_id << ")\n";
    }
    
    // 更新客服状态
    void set_agent_status(const std::string& agent_id, HumanAgentStatus status) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = agents_.find(agent_id);
        if (it != agents_.end()) {
            it->second->status = status;
            it->second->last_activity = std::chrono::steady_clock::now();
            
            std::string status_str;
            switch (status) {
                case HumanAgentStatus::ONLINE: status_str = "在线"; break;
                case HumanAgentStatus::BUSY: status_str = "繁忙"; break;
                case HumanAgentStatus::AWAY: status_str = "离开"; break;
                case HumanAgentStatus::OFFLINE: status_str = "离线"; break;
            }
            std::cout << "[HumanService] 客服 " << it->second->name 
                      << " 状态更新: " << status_str << "\n";
            
            // 如果客服上线，尝试分配队列中的请求
            if (status == HumanAgentStatus::ONLINE) {
                try_assign_pending_requests_unlocked();
            }
        }
    }
    
    // ============================================================
    // 转接队列管理
    // ============================================================
    
    // 创建转接请求
    std::string request_escalation(const std::string& session_id,
                                    const std::string& tenant_id,
                                    const std::string& user_id,
                                    const std::string& reason,
                                    int priority = 5) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::string req_id = "esc_" + std::to_string(++request_counter_);
        auto request = std::make_unique<EscalationRequest>(
            req_id, session_id, tenant_id, user_id, reason, priority);
        
        // 加入优先级队列
        escalation_queue_.push({priority, req_id});
        pending_requests_[req_id] = std::move(request);
        
        std::cout << "[HumanService] 转接请求创建: " << req_id 
                  << " (会话: " << session_id << ", 原因: " << reason << ")\n";
        
        // 尝试立即分配
        try_assign_pending_requests_unlocked();
        
        return req_id;
    }
    
    // 取消转接请求
    void cancel_escalation(const std::string& request_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = pending_requests_.find(request_id);
        if (it != pending_requests_.end()) {
            std::cout << "[HumanService] 转接请求取消: " << request_id << "\n";
            pending_requests_.erase(it);
        }
    }
    
    // ============================================================
    // 会话接管
    // ============================================================
    
    // 人工客服接管会话
    bool handover_to_human(const std::string& session_id,
                           const std::string& agent_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto agent_it = agents_.find(agent_id);
        if (agent_it == agents_.end()) return false;
        
        // 创建接管记录
        SessionHandover handover;
        handover.session_id = session_id;
        handover.human_agent_id = agent_id;
        handover.is_ai_paused = true;
        handover.handover_time = std::chrono::steady_clock::now();
        
        active_handovers_[session_id] = handover;
        agent_it->second->assigned_sessions.insert(session_id);
        agent_it->second->status = HumanAgentStatus::BUSY;
        
        std::cout << "[HumanService] 会话接管: " << session_id 
                  << " → 客服 " << agent_it->second->name << "\n";
        return true;
    }
    
    // 会话交回 AI
    bool return_to_ai(const std::string& session_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = active_handovers_.find(session_id);
        if (it == active_handovers_.end()) return false;
        
        std::string agent_id = it->second.human_agent_id;
        active_handovers_.erase(it);
        
        // 更新客服状态
        auto agent_it = agents_.find(agent_id);
        if (agent_it != agents_.end()) {
            agent_it->second->assigned_sessions.erase(session_id);
            if (agent_it->second->assigned_sessions.empty()) {
                agent_it->second->status = HumanAgentStatus::ONLINE;
            }
        }
        
        std::cout << "[HumanService] 会话交回 AI: " << session_id << "\n";
        return true;
    }
    
    // 查询会话状态
    bool is_handled_by_human(const std::string& session_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        return active_handovers_.find(session_id) != active_handovers_.end();
    }
    
    // 获取处理当前会话的客服 ID
    std::string get_assigned_agent(const std::string& session_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = active_handovers_.find(session_id);
        if (it != active_handovers_.end()) {
            return it->second.human_agent_id;
        }
        return "";
    }
    
    // ============================================================
    // 统计信息
    // ============================================================
    
    void print_stats() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::cout << "\n[HumanService 统计]\n";
        std::cout << "  注册客服: " << agents_.size() << "\n";
        
        int online = 0, busy = 0, away = 0, offline = 0;
        for (const auto& [id, agent] : agents_) {
            switch (agent->status) {
                case HumanAgentStatus::ONLINE: online++; break;
                case HumanAgentStatus::BUSY: busy++; break;
                case HumanAgentStatus::AWAY: away++; break;
                case HumanAgentStatus::OFFLINE: offline++; break;
            }
        }
        std::cout << "  状态分布: 在线=" << online << " 繁忙=" << busy 
                  << " 离开=" << away << " 离线=" << offline << "\n";
        std::cout << "  待分配转接: " << pending_requests_.size() << "\n";
        std::cout << "  人工处理中: " << active_handovers_.size() << "\n";
    }

private:
    // 尝试分配待处理的转接请求
    void try_assign_pending_requests_unlocked() {
        // 简化实现：找在线且空闲的客服
        for (auto& [agent_id, agent] : agents_) {
            if (agent->status != HumanAgentStatus::ONLINE) continue;
            
            // 从队列中取最高优先级的请求
            if (!escalation_queue_.empty()) {
                auto [priority, req_id] = escalation_queue_.top();
                escalation_queue_.pop();
                
                auto req_it = pending_requests_.find(req_id);
                if (req_it != pending_requests_.end()) {
                    // 分配会话
                    handover_to_human(req_it->second->session_id, agent_id);
                    pending_requests_.erase(req_it);
                }
            }
        }
    }
    
    std::map<std::string, std::unique_ptr<HumanAgent>> agents_;
    std::map<std::string, std::unique_ptr<EscalationRequest>> pending_requests_;
    std::map<std::string, SessionHandover> active_handovers_;
    
    // 优先级队列（priority, request_id）
    std::priority_queue<std::pair<int, std::string>> escalation_queue_;
    
    mutable std::mutex mutex_;
    int request_counter_ = 0;
};

} // namespace nuclaw
