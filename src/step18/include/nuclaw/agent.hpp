// ============================================================================
// agent.hpp - Agent 基类（Step 11 多 Agent 架构）
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <functional>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <boost/json.hpp>

namespace json = boost::json;

// 消息结构
struct Message {
    std::string id;
    std::string from;
    std::string to;           // 空表示广播
    std::string type;         // TASK_ASSIGN, TASK_RESULT, etc.
    std::string content;      // JSON 格式
    std::string parent_id;    // 父消息 ID
    
    static std::string generate_id() {
        static std::atomic<int> counter{0};
        return "msg_" + std::to_string(++counter);
    }
};

// Agent 基类
class Agent {
public:
    Agent(const std::string& name, const std::string& role)
        : name_(name), role_(role), running_(false) {}
    
    virtual ~Agent() {
        stop();
    }
    
    void start() {
        running_ = true;
        worker_ = std::thread(&Agent::run, this);
    }
    
    void stop() {
        running_ = false;
        cv_.notify_all();
        if (worker_.joinable()) {
            worker_.join();
        }
    }
    
    void send_message(const Message& msg) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            inbox_.push(msg);
        }
        cv_.notify_one();
    }
    
    std::string get_name() const { return name_; }
    std::string get_role() const { return role_; }
    
    void set_send_callback(std::function<void(const Message&)> callback) {
        send_callback_ = callback;
    }

protected:
    virtual void run() {
        while (running_) {
            Message msg;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock, [this] { return !inbox_.empty() || !running_; });
                
                if (!running_) break;
                
                msg = inbox_.front();
                inbox_.pop();
            }
            
            process_message(msg);
        }
    }
    
    virtual void process_message(const Message& msg) = 0;
    
    void send(const Message& msg) {
        if (send_callback_) {
            send_callback_(msg);
        }
    }
    
    std::string name_;
    std::string role_;
    bool running_;
    
    std::queue<Message> inbox_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::thread worker_;
    
    std::function<void(const Message&)> send_callback_;
};
