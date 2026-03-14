# Step 16: Agent 状态与记忆系统

> 目标：为 Agent 添加持久化状态、记忆和情感计算能力
> 
> 难度：⭐⭐⭐⭐ (较难)
> 
> 代码量：约 950 行

## 问题引入

**前一章 (Step 15) 的问题：**

我们的 Agent 已经能接入各种 IM 平台，但它是"无状态"的——每次对话都是独立的：

```
用户: 我叫小明
Agent: 你好小明！
...
用户: 我叫什么？
Agent: （完全忘了）抱歉，我不知道...
```

更严重的是，Agent 没有**长期记忆**、**情感状态**和**个性特征**。

**本章目标：**
让 Agent 拥有：
- 🧠 **短期记忆**：当前对话的上下文
- 📚 **长期记忆**：跨对话的用户信息、历史事件
- 😊 **情感状态**：心情、精力、对用户的印象
- 👤 **个性特征**：性格、偏好、行为模式

## 系统架构

```
┌─────────────────────────────────────────────────────────────────────┐
│                        Agent 状态与记忆系统                          │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  ┌──────────────────────────────────────────────────────────────┐  │
│  │                     AgentState (运行时状态)                   │  │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐        │  │
│  │  │ 情感状态  │ │ 当前活动  │ │ 能量水平  │ │ 社交关系  │        │  │
│  │  └──────────┘ └──────────┘ └──────────┘ └──────────┘        │  │
│  └──────────────────────────────────────────────────────────────┘  │
│                              │                                      │
│  ┌───────────────────────────┼───────────────────────────┐          │
│  │                           │                           │          │
│  ▼                           ▼                           ▼          │
│ ┌──────────────┐     ┌──────────────┐     ┌──────────────┐          │
│ │  短期记忆    │     │  工作记忆    │     │  长期记忆    │          │
│ │  (对话历史)  │     │  (当前任务)  │     │  (向量 DB)   │          │
│ └──────────────┘     └──────────────┘     └──────────────┘          │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

## 核心实现

### 1. Agent 状态定义

```cpp
// agent_state.hpp
#pragma once
#include <chrono>
#include <string>
#include <vector>
#include <map>
#include <nlohmann/json.hpp>

namespace nuclaw {

// 情感状态 (-10 ~ +10)
struct EmotionState {
    float happiness = 0.0f;     // 开心程度
    float energy = 5.0f;        // 精力水平 (0-10)
    float trust = 5.0f;         // 对当前用户的信任度
    float interest = 5.0f;      // 对当前话题的兴趣度
    
    void decay(float rate = 0.95f) {
        happiness *= rate;
        energy = std::min(10.0f, energy + 0.1f);  // 精力自然恢复
    }
    
    std::string describe() const {
        if (happiness > 5) return "开心";
        if (happiness < -5) return "难过";
        if (energy < 3) return "疲惫";
        return "平静";
    }
};

// 记忆条目
struct Memory {
    std::string id;
    std::string content;
    std::string category;       // "fact", "event", "preference", "emotion"
    float importance = 5.0f;    // 重要性 0-10
    std::chrono::system_clock::time_point timestamp;
    std::map<std::string, std::string> metadata;
    
    nlohmann::json to_json() const {
        return {
            {"id", id},
            {"content", content},
            {"category", category},
            {"importance", importance},
            {"timestamp", std::chrono::system_clock::to_time_t(timestamp)},
            {"metadata", metadata}
        };
    }
};

// 与特定用户的关系
struct Relationship {
    std::string user_id;
    std::string user_name;
    float familiarity = 0.0f;   // 熟悉度 0-10
    float affinity = 5.0f;      // 好感度 0-10
    int interaction_count = 0;  // 交互次数
    std::chrono::system_clock::time_point last_interaction;
    std::vector<std::string> shared_experiences;  // 共同经历
};

// Agent 完整状态
class AgentState {
public:
    std::string agent_id;
    std::string current_activity = "idle";
    EmotionState emotion;
    
    // 三层记忆系统
    std::vector<Memory> short_term_memory;    // 最近 10 条
    std::vector<Memory> working_memory;       // 当前任务相关
    std::map<std::string, Relationship> relationships;  // 与各用户的关系
    
    // 个性特征
    struct Personality {
        std::string name;
        std::string role;
        std::vector<std::string> traits;       // ["开朗", "幽默", "细心"]
        std::vector<std::string> likes;
        std::vector<std::string> dislikes;
        nlohmann::json custom_data;
    } personality;
    
    void update_emotion(const std::string& event_type, float intensity);
    void add_memory(const Memory& memory);
    std::vector<Memory> retrieve_relevant_memories(
        const std::string& query, 
        size_t limit = 5
    ) const;
    Relationship& get_or_create_relationship(const std::string& user_id);
};

} // namespace nuclaw
```

### 2. 记忆管理器

```cpp
// memory_manager.hpp
#pragma once
#include "agent_state.hpp"
#include <memory>
#include <vector_store_client.hpp>

namespace nuclaw {

class MemoryManager {
public:
    MemoryManager(std::shared_ptr<VectorStoreClient> vector_store)
        : vector_store_(vector_store) {}
    
    // 添加短期记忆
    void add_short_term(AgentState& state, const std::string& content, 
                       const std::string& category = "event") {
        Memory mem{
            .id = generate_uuid(),
            .content = content,
            .category = category,
            .timestamp = std::chrono::system_clock::now()
        };
        
        state.short_term_memory.push_back(mem);
        
        // 限制容量，重要记忆转入长期记忆
        if (state.short_term_memory.size() > 10) {
            consolidate_memories(state);
        }
    }
    
    // 存储到长期记忆（向量数据库）
    async::Task<void> store_long_term(
        const std::string& agent_id,
        const std::string& user_id,
        const Memory& memory) {
        
        // 生成 embedding
        auto embedding = co_await embedding_service_.encode(memory.content);
        
        // 存储到向量数据库
        co_await vector_store_->upsert({
            .id = memory.id,
            .vector = embedding,
            .metadata = {
                {"agent_id", agent_id},
                {"user_id", user_id},
                {"category", memory.category},
                {"content", memory.content},
                {"timestamp", std::to_string(
                    std::chrono::system_clock::to_time_t(memory.timestamp)
                )}
            }
        });
    }
    
    // 检索相关记忆
    async::Task<std::vector<Memory>> retrieve_memories(
        const std::string& agent_id,
        const std::string& user_id,
        const std::string& query,
        size_t limit = 5) {
        
        // 生成查询向量
        auto query_embedding = co_await embedding_service_.encode(query);
        
        // 向量搜索
        auto results = co_await vector_store_->search({
            .vector = query_embedding,
            .filter = fmt::format("agent_id = '{}' AND user_id = '{}'", 
                                agent_id, user_id),
            .top_k = limit
        });
        
        // 转换为 Memory 对象
        std::vector<Memory> memories;
        for (const auto& r : results) {
            memories.push_back(Memory{
                .id = r.id,
                .content = r.metadata.at("content"),
                .category = r.metadata.at("category"),
                .timestamp = std::chrono::system_clock::from_time_t(
                    std::stol(r.metadata.at("timestamp"))
                )
            });
        }
        
        co_return memories;
    }
    
    // 记忆巩固：从短期记忆提取重要信息存入长期记忆
    async::Task<void> consolidate_memories(AgentState& state) {
        for (const auto& mem : state.short_term_memory) {
            // 判断重要性（可以用 LLM 或规则）
            if (mem.importance > 7 || mem.category == "preference") {
                co_await store_long_term(state.agent_id, "", mem);
            }
        }
        
        // 清空短期记忆
        state.short_term_memory.clear();
    }

private:
    std::shared_ptr<VectorStoreClient> vector_store_;
    EmbeddingService embedding_service_;
};

} // namespace nuclaw
```

### 3. 个性化对话引擎

```cpp
// personalized_chat.hpp
#pragma once
#include "agent_state.hpp"
#include "memory_manager.hpp"
#include <llm_client.hpp>

namespace nuclaw {

class PersonalizedChatEngine {
public:
    PersonalizedChatEngine(
        std::shared_ptr<LLMClient> llm,
        std::shared_ptr<MemoryManager> memory_mgr
    ) : llm_(llm), memory_mgr_(memory_mgr) {}
    
    async::Task<std::string> chat(
        AgentState& state,
        const std::string& user_id,
        const std::string& user_name,
        const std::string& message) {
        
        // 1. 获取或创建关系
        auto& relation = state.get_or_create_relationship(user_id);
        relation.user_name = user_name;
        relation.interaction_count++;
        relation.last_interaction = std::chrono::system_clock::now();
        
        // 2. 检索相关记忆
        auto memories = co_await memory_mgr_->retrieve_memories(
            state.agent_id, user_id, message, 5
        );
        
        // 3. 添加当前消息到短期记忆
        memory_mgr_->add_short_term(state, 
            fmt::format("User {}: {}", user_name, message), 
            "dialogue"
        );
        
        // 4. 构建个性化 prompt
        auto prompt = build_personalized_prompt(
            state, relation, memories, user_name, message
        );
        
        // 5. 调用 LLM
        auto response = co_await llm_->complete(prompt);
        
        // 6. 记录回复到短期记忆
        memory_mgr_->add_short_term(state, 
            fmt::format("Agent: {}", response), 
            "dialogue"
        );
        
        // 7. 更新情感和关系
        update_emotion_and_relation(state, relation, message, response);
        
        co_return response;
    }

private:
    std::string build_personalized_prompt(
        const AgentState& state,
        const Relationship& relation,
        const std::vector<Memory>& memories,
        const std::string& user_name,
        const std::string& message) {
        
        std::string prompt = fmt::format(R"(你是 {}，一个{}的 AI 助手。

你的个性特征：
{}

当前状态：{}

与 {} 的关系：
- 熟悉度：{}/10
- 好感度：{}/10  
- 已交互 {} 次

相关记忆：
{}

对话历史：
{}

{} 说：{}

请根据以上信息，以你的个性做出自然回复。记住：你是活的角色，不是客服机器人。
)", state.personality.name,
   fmt::join(state.personality.traits, "、"),
   state.emotion.describe(),
   user_name,
   relation.familiarity,
   relation.affinity,
   relation.interaction_count,
   format_memories(memories),
   format_short_term(state.short_term_memory),
   user_name,
   message
        );
        
        return prompt;
    }
    
    void update_emotion_and_relation(
        AgentState& state,
        Relationship& relation,
        const std::string& user_msg,
        const std::string& agent_response) {
        
        // 简单规则：用户说"谢谢"增加好感度
        if (user_msg.find("谢谢") != std::string::npos ||
            user_msg.find("感谢") != std::string::npos) {
            relation.affinity = std::min(10.0f, relation.affinity + 0.5f);
            state.emotion.happiness += 1.0f;
        }
        
        // 熟悉度随交互次数增加
        relation.familiarity = std::min(10.0f, 
            relation.familiarity + 0.1f);
        
        // 情感自然衰减
        state.emotion.decay();
    }
    
    std::string format_memories(const std::vector<Memory>& memories) {
        if (memories.empty()) return "（无）";
        
        std::string result;
        for (const auto& m : memories) {
            result += fmt::format("- [{}] {}\n", m.category, m.content);
        }
        return result;
    }
    
    std::string format_short_term(const std::vector<Memory>& stm) {
        std::string result;
        for (const auto& m : stm) {
            result += fmt::format("- {}\n", m.content);
        }
        return result;
    }
    
    std::shared_ptr<LLMClient> llm_;
    std::shared_ptr<MemoryManager> memory_mgr_;
};

} // namespace nuclaw
```

### 4. 使用示例

```cpp
// main.cpp
#include <nuclaw/agent_state.hpp>
#include <nuclaw/memory_manager.hpp>
#include <nuclaw/personalized_chat.hpp>

int main() {
    // 初始化组件
    auto vector_store = std::make_shared<ChromaClient>("http://localhost:8000");
    auto memory_mgr = std::make_shared<nuclaw::MemoryManager>(vector_store);
    auto llm = std::make_shared<OpenAIClient>("api-key");
    
    // 创建 Agent 状态
    nuclaw::AgentState state;
    state.agent_id = "travel-assistant-01";
    state.personality = {
        .name = "小旅",
        .role = "旅行助手",
        .traits = {"热情", "细心", "幽默"},
        .likes = {"旅行", "美食", "摄影"},
        .dislikes = {"赶时间", "拥挤"}
    };
    
    // 创建对话引擎
    nuclaw::PersonalizedChatEngine chat(llm, memory_mgr);
    
    // 对话循环
    std::string user_id = "user_123";
    std::string user_name = "小明";
    
    while (true) {
        std::cout <> user_input;
        
        auto response = chat.chat(state, user_id, user_name, user_input).get();
        std::cout << "小旅: " << response << std::endl;
    }
    
    return 0;
}
```

## 交互示例

```
小明: 你好

小旅: 你好呀小明！👋 我是小旅，你的旅行助手。
有什么我可以帮你的吗？想去哪里玩？

---

小明: 我想去日本

小旅: 日本！✨ 好选择！樱花季快到了呢～
你是第一次去还是之前去过呀？
喜欢什么样的旅行风格？悠闲一点还是安排满满？

---

小明: 我叫小明，我喜欢美食

小旅: 好的小明，我记住啦！📝
（已记录：用户"小明"喜欢美食）

那你去日本可太对了！日本的美食真的绝绝子～
拉面、寿司、烧鸟、和牛... 光想想就流口水了 🤤

---

小明: 我叫什么？

小旅: 你叫小明呀！我才不会忘呢～ 😊
而且我知道你超爱吃美食的！

---

小明: 谢谢

小旅: 不客气！能帮到你我也很开心～
（好感度 +0.5）

---

[第二天]

小明: 还记得我吗？

小旅: 当然记得！小明嘛～ 🎉
昨天你说想去日本吃美食，我还在想推荐你呢！
要聊聊日本的美食之旅吗？

（从长期记忆检索到：用户小明，喜欢美食，想去日本）
```

## 本章总结

通过本章，我们实现了：

1. **三层记忆系统**
   - 短期记忆：当前对话上下文
   - 工作记忆：任务相关信息
   - 长期记忆：向量数据库存储的用户画像和历史

2. **情感计算**
   - Agent 有情绪状态（开心/疲惫/平静）
   - 情感会自然衰减和恢复
   - 用户互动影响 Agent 情绪

3. **关系系统**
   - 记录与每个用户的熟悉度和好感度
   - 交互历史影响对话风格
   - 个性化回复基于关系亲疏

4. **记忆巩固**
   - 自动将重要信息从短期记忆转入长期记忆
   - 向量检索实现语义记忆搜索

---

## 下一章预告

**Step 17: 期中实战 — 旅行小管家**

综合运用前面所有知识，构建一个懂你的智能旅行规划助手！
