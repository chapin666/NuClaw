// ============================================================================
// memory.hpp - 记忆系统
// ============================================================================

#pragma once
#include <string>
#include <chrono>
#include <vector>
#include <algorithm>

namespace nuclaw {

// 记忆条目
struct Memory {
    std::string id;
    std::string content;
    std::string category;      // conversation, fact, preference
    float importance = 5.0f;   // 1 ~ 10，重要性
    std::string user_id;       // 关联用户
    std::chrono::system_clock::time_point timestamp;
    
    static std::string generate_id() {
        static int counter = 0;
        return "mem_" + std::to_string(++counter);
    }
};

// 短期记忆存储（固定容量，FIFO）
class ShortTermMemory {
public:
    explicit ShortTermMemory(size_t capacity = 10) : capacity_(capacity) {}
    
    void add(const Memory& memory) {
        memories_.push_back(memory);
        if (memories_.size() > capacity_) {
            memories_.erase(memories_.begin());
        }
    }
    
    void add(const std::string& content, 
             const std::string& category,
             const std::string& user_id = "") {
        Memory mem;
        mem.id = Memory::generate_id();
        mem.content = content;
        mem.category = category;
        mem.importance = 5.0f;
        mem.user_id = user_id;
        mem.timestamp = std::chrono::system_clock::now();
        add(mem);
    }
    
    const std::vector<Memory>& get_all() const { return memories_; }
    
    std::vector<Memory> get_by_user(const std::string& user_id) const {
        std::vector<Memory> result;
        for (const auto& m : memories_) {
            if (m.user_id == user_id) {
                result.push_back(m);
            }
        }
        return result;
    }
    
    size_t size() const { return memories_.size(); }
    void clear() { memories_.clear(); }

private:
    std::vector<Memory> memories_;
    size_t capacity_;
};

} // namespace nuclaw
