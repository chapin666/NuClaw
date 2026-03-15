#include "smartsupport/services/human/service.hpp"
#include <iostream>

namespace smartsupport::services::human {

HumanService::HumanService(asio::io_context& io)
    : queue_timer_(io) {
    queue_timer_.expires_after(std::chrono::seconds(5));
    queue_timer_.async_wait([this](auto) { process_queue(); });
}

void HumanService::agent_login(const HumanAgent& agent) {
    std::lock_guard<std::mutex> lock(mutex_);
    agents_[agent.id] = agent;
    std::cout << "[HumanService] Agent " << agent.name << " logged in\n";
}

void HumanService::agent_logout(const std::string& agent_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    agents_.erase(agent_id);
    agent_sessions_.erase(agent_id);
}

void HumanService::update_status(const std::string& agent_id, AgentStatus status) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = agents_.find(agent_id);
    if (it != agents_.end()) {
        it->second.status = status;
    }
}

void HumanService::request_human(const QueuedSession& session) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto agent_id = find_best_agent(session);
    if (agent_id) {
        assign_session(*agent_id, session.session_id);
        notify_user_assigned(session.session_id, agents_[*agent_id].name);
    } else {
        waiting_queue_.push(session);
        notify_user_queued(session.session_id, waiting_queue_.size());
    }
}

std::optional<std::string> HumanService::find_best_agent(const QueuedSession& session) {
    std::optional<std::string> best_agent;
    int min_load = INT_MAX;
    
    for (const auto& [id, agent] : agents_) {
        if (agent.status != AgentStatus::ONLINE) continue;
        if (agent.current_sessions >= agent.max_sessions) continue;
        if (agent.tenant_id != session.tenant_id) continue;
        
        bool skills_match = true;
        for (const auto& skill : session.required_skills) {
            if (std::find(agent.skills.begin(), agent.skills.end(), skill) == agent.skills.end()) {
                skills_match = false;
                break;
            }
        }
        if (!skills_match) continue;
        
        if (agent.current_sessions < min_load) {
            min_load = agent.current_sessions;
            best_agent = id;
        }
    }
    
    return best_agent;
}

void HumanService::assign_session(const std::string& agent_id, const std::string& session_id) {
    agent_sessions_[agent_id].push_back(session_id);
    session_agent_[session_id] = agent_id;
    agents_[agent_id].current_sessions++;
}

void HumanService::process_queue() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::queue<QueuedSession> new_queue;
    
    while (!waiting_queue_.empty()) {
        auto session = waiting_queue_.front();
        waiting_queue_.pop();
        
        auto agent_id = find_best_agent(session);
        if (agent_id) {
            assign_session(*agent_id, session.session_id);
            notify_user_assigned(session.session_id, agents_[*agent_id].name);
        } else {
            auto wait_time = std::chrono::steady_clock::now() - session.queued_at;
            if (wait_time > std::chrono::minutes(5)) {
                notify_user_timeout(session.session_id);
            } else {
                new_queue.push(session);
            }
        }
    }
    
    waiting_queue_ = std::move(new_queue);
    
    queue_timer_.expires_after(std::chrono::seconds(5));
    queue_timer_.async_wait([this](auto) { process_queue(); });
}

void HumanService::notify_user_assigned(const std::string& session_id, const std::string& agent_name) {
    std::cout << "[HumanService] Session " << session_id << " assigned to " << agent_name << "\n";
}

void HumanService::notify_user_queued(const std::string& session_id, int position) {
    std::cout << "[HumanService] Session " << session_id << " queued at position " << position << "\n";
}

void HumanService::notify_user_timeout(const std::string& session_id) {
    std::cout << "[HumanService] Session " << session_id << " queue timeout\n";
}

void HumanService::agent_take_session(const std::string& agent_id, const std::string& session_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    assign_session(agent_id, session_id);
}

void HumanService::agent_send_message(const std::string& agent_id, const std::string& session_id,
                                      const std::string& content) {
    std::cout << "[HumanService] Agent " << agent_id << " sent message to session " << session_id << "\n";
}

void HumanService::agent_end_session(const std::string& agent_id, const std::string& session_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = agents_.find(agent_id);
    if (it != agents_.end()) {
        it->second.current_sessions--;
    }
    
    session_agent_.erase(session_id);
}

HumanService::Dashboard HumanService::get_agent_dashboard(const std::string& agent_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Dashboard dashboard;
    auto it = agent_sessions_.find(agent_id);
    if (it != agent_sessions_.end()) {
        dashboard.active_sessions = it->second;
    }
    
    std::queue<QueuedSession> temp_queue = waiting_queue_;
    while (!temp_queue.empty()) {
        dashboard.waiting_sessions.push_back(temp_queue.front());
        temp_queue.pop();
    }
    
    return dashboard;
}

} // namespace smartsupport::services::human
