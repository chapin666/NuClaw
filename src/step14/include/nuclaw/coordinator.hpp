// ============================================================================
// coordinator.hpp - 协调器 Agent
// ============================================================================

#pragma once

#include "agent.hpp"
#include <map>
#include <set>

class Coordinator : public Agent {
public:
    Coordinator(const std::string& name = "coordinator")
        : Agent(name, "coordinator") {}
    
    void register_agent_info(const std::string& name, const std::string& capability) {
        std::lock_guard<std::mutex> lock(agents_mutex_);
        agent_capabilities_[name] = capability;
        agent_status_[name] = "idle";
    }
    
    void dispatch_task(const std::string& task_id,
                       const std::string& task,
                       const std::vector<std::string>& target_agents) {
        
        std::cout << "[协调器] 分发任务: " << task_id << " -> ";
        for (const auto& agent : target_agents) {
            std::cout << agent << " ";
        }
        std::cout << std::endl;
        
        // 发送任务给各个 Agent
        for (const auto& agent_name : target_agents) {
            Message msg;
            msg.id = Message::generate_id();
            msg.from = name_;
            msg.to = agent_name;
            msg.type = "TASK_ASSIGN";
            
            json::object content;
            content["task_id"] = task_id;
            content["task"] = task;
            msg.content = json::serialize(content);
            msg.parent_id = task_id;
            
            send(msg);
            
            {
                std::lock_guard<std::mutex> lock(agents_mutex_);
                agent_status_[agent_name] = "busy";
            }
        }
        
        // 记录任务
        {
            std::lock_guard<std::mutex> lock(tasks_mutex_);
            pending_tasks_[task_id] = {
                {"task", task},
                {"agents", target_agents},
                {"completed", false}
            };
        }
    }

protected:
    void process_message(const Message& msg) override {
        if (msg.type == "TASK_RESULT") {
            handle_task_result(msg);
        }
    }
    
    void handle_task_result(const Message& msg) {
        auto data = json::parse(msg.content);
        std::string task_id = data["task_id"].as_string();
        
        std::cout << "[协调器] 收到结果: " << msg.from << " 完成任务 " << task_id << std::endl;
        
        {
            std::lock_guard<std::mutex> lock(agents_mutex_);
            agent_status_[msg.from] = "idle";
        }
        
        // 检查任务是否完成
        check_task_completion(task_id);
    }
    
    void check_task_completion(const std::string& task_id) {
        std::lock_guard<std::mutex> lock(tasks_mutex_);
        // 简化：实际应该检查所有子任务是否完成
        std::cout << "[协调器] 任务 " << task_id << " 处理完成" << std::endl;
    }
    
    mutable std::mutex agents_mutex_;
    std::map<std::string, std::string> agent_capabilities_;
    std::map<std::string, std::string> agent_status_;
    
    mutable std::mutex tasks_mutex_;
    std::map<std::string, json::value> pending_tasks_;
};
