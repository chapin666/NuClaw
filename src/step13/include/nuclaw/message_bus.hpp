// ============================================================================
// message_bus.hpp - 消息总线（Agent 间通信）
// ============================================================================

#pragma once

#include "agent.hpp"
#include <map>
#include <memory>
#include <iostream>

class MessageBus {
public:
    void register_agent(const std::string& name, std::shared_ptr<Agent> agent) {
        agents_[name] = agent;
        
        agent->set_send_callback([this](const Message& msg) {
            route_message(msg);
        });
        
        agent->start();
        std::cout << "[+] 注册 Agent: " << name << std::endl;
    }
    
    void route_message(const Message& msg) {
        if (msg.to.empty()) {
            broadcast(msg);
        } else {
            auto it = agents_.find(msg.to);
            if (it != agents_.end()) {
                it->second->send_message(msg);
            } else {
                std::cerr << "[!] 未知接收者: " << msg.to << std::endl;
            }
        }
    }
    
    void broadcast(const Message& msg) {
        for (const auto& [name, agent] : agents_) {
            if (name != msg.from) {
                agent->send_message(msg);
            }
        }
    }
    
    std::shared_ptr<Agent> get_agent(const std::string& name) {
        auto it = agents_.find(name);
        if (it != agents_.end()) {
            return it->second;
        }
        return nullptr;
    }
    
    void shutdown() {
        for (auto& [name, agent] : agents_) {
            agent->stop();
        }
        agents_.clear();
    }

private:
    std::map<std::string, std::shared_ptr<Agent>> agents_;
};
