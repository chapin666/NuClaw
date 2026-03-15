// ============================================================================
// transport_agent.hpp - 交通 Agent
// ============================================================================

#pragma once

#include "agent.hpp"
#include <iostream>

class TransportAgent : public Agent {
public:
    TransportAgent(const std::string& name = "transport_agent")
        : Agent(name, "transport") {}

protected:
    void process_message(const Message& msg) override {
        if (msg.type == "TASK_ASSIGN") {
            auto data = json::parse(msg.content);
            std::string task = data["task"].as_string();
            std::string task_id = data["task_id"].as_string();
            
            std::cout << "[" << name_ << "] 处理交通任务: " << task << std::endl;
            
            // 模拟查询交通信息
            json::object result;
            result["flights"] = json::array{
                json::object{{"flight", "CA1234"}, {"departure", "08:00"}, {"price", 1200}},
                json::object{{"flight", "MU5678"}, {"departure", "14:00"}, {"price", 980}}
            };
            
            // 返回结果
            Message response;
            response.id = Message::generate_id();
            response.from = name_;
            response.to = "coordinator";
            response.type = "TASK_RESULT";
            
            json::object content;
            content["task_id"] = task_id;
            content["result"] = result;
            response.content = json::serialize(content);
            
            send(response);
        }
    }
};
