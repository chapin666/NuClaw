# Step 16: Agent 状态与记忆系统 —— 让 Agent 有"温度"

> 目标：实现情感状态机、短期记忆、长期记忆，让 Agent 拥有个性化和持续学习能力
> 
003e 难度：⭐⭐⭐⭐ | 代码量：约 1200 行 | 预计学习时间：4-5 小时

---

## 一、为什么需要状态与记忆？

### 1.1 Step 4 和 Step 15 的问题

**Step 4** 实现了**会话级上下文**（多轮对话），但它有个局限——**只在当前会话有效**：

```
昨天对话：
用户: 你好，我叫小明，喜欢篮球
Bot: 你好小明！很高兴认识你
（会话结束，Step 4 的上下文被清除）

今天新对话（Step 4 无法帮助）：
用户: 我今天做了什么？
Bot: 抱歉，我不知道你在说什么 ❌

用户: 我叫什么名字？
Bot: 抱歉，我不知道 ❌
```

**Step 15** 接入了 IM 平台，但同样的问题——**每次对话都是孤立的**：

```
用户: 你好，我叫小明，喜欢篮球 🏀
Bot: 你好小明！很高兴认识你

（一周后）
用户: 推荐个运动
Bot: 请问您喜欢什么运动？（完全忘记你喜欢篮球）❌
```

**问题本质：Step 4 是短时上下文，不是长期记忆**

### 1.2 长期记忆的价值

**与 Step 4 会话级上下文的区别：**

| 能力 | Step 4: 会话级上下文 | Step 16: 长期记忆 |
|:---|:---|:---|
| **时间跨度** | 当前会话（几分钟-几小时） | 跨会话（几天、几周、永久） |
| **存储** | 内存，关闭即消失 | 数据库，持久保存 |
| **内容** | 最近对话、当前话题 | 用户画像、历史事件、情感状态 |
| **典型场景** | "那上海呢"理解指天气 | "我记得你喜欢湖人队" |

**长期记忆带来的体验升级：**

```
有记忆的 Agent：

用户: 你好，我叫小明，喜欢篮球 🏀
Bot: 你好小明！篮球很棒呢～我记得你之前说过喜欢湖人队？

（一周后）
用户: 推荐个运动
Bot: 既然你喜欢篮球，要不要试试投篮训练 App？
        或者附近有个室内球场，天气不好也能打
        
（用户情绪低落时）
用户: 今天好倒霉
Bot: （检测到情绪低落，调整回复风格）
      抱抱～想聊聊发生了什么吗？
      要不要听个笑话？我记得你喜欢科比，
      他说过... "总有一个人要赢，为什么不是我？"

价值：
• 连续对话体验（记得你是谁）
• 个性化服务（懂你的偏好）
• 情感陪伴（有温度，会关心）
```

---

## 二、系统设计

### 2.1 三层记忆架构

```
┌─────────────────────────────────────────────────────────────────────┐
│                      Agent 记忆系统架构                               │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  ┌──────────────────────────────────────────────────────────────┐   │
│  │                        工作记忆 (Working Memory)              │   │
│  │                   当前对话上下文（最近 10-20 轮）               │   │
│  │                         内存存储                                │   │
│  │  User: 今天天气怎么样？                                         │   │
│  │  Bot: 北京今天晴，25°C...                                       │   │
│  │  User: 那适合穿什么？                                           │   │
│  │  Bot: 建议穿短袖，记得带件薄外套                                 │   │
│  └──────────────────────────────────────────────────────────────┘   │
│                              │                                       │
│                              │ 摘要提取                              │
│                              ▼                                       │
│  ┌──────────────────────────────────────────────────────────────┐   │
│  │                      短期记忆 (Short-term Memory)             │   │
│  │               当前会话摘要 + 近期事件（数小时-数天）            │   │
│  │                      Redis / 内存                              │   │
│  │  • 用户当前情绪：开心                                           │   │
│  │  • 本轮对话主题：天气、穿搭                                     │   │
│  │  • 用户意图：出行准备                                           │   │
│  └──────────────────────────────────────────────────────────────┘   │
│                              │                                       │
│                              │ 写入                                  │
│                              ▼                                       │
│  ┌──────────────────────────────────────────────────────────────┐   │
│  │                      长期记忆 (Long-term Memory)              │   │
│  │              用户画像、历史事件、知识（永久存储）                │   │
│  │                   PostgreSQL / MongoDB                        │   │
│  │  ┌────────────────────────────────────────────────────────┐   │   │
│  │  │                   用户画像                              │   │   │
│  │  │  基本信息: 小明, 25岁, 北京                             │   │   │
│  │  │  兴趣爱好: 篮球、湖人、科技                               │   │   │
│  │  │  性格特点: 外向、幽默                                    │   │   │
│  │  │  沟通偏好: 喜欢玩笑、不喜欢太正式                          │   │   │
│  │  └────────────────────────────────────────────────────────┘   │   │
│  │  ┌────────────────────────────────────────────────────────┐   │   │
│  │  │                   事件记忆                              │   │   │
│  │  │  [2024-03-01] 首次对话，介绍了自己                        │   │   │
│  │  │  [2024-03-05] 帮忙规划了日本旅行                          │   │   │
│  │  │  [2024-03-10] 分享了升职的喜悦                            │   │   │
│  │  └────────────────────────────────────────────────────────┘   │   │
│  └──────────────────────────────────────────────────────────────┘   │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```


**与 Step 4 的关系：**

- **Step 4** 实现了图中的**工作记忆**（会话级上下文，内存存储，关闭即消失）
- **本章** 在工作记忆之上，增加了**短期记忆**（Redis）和**长期记忆**（数据库持久化）
- **工作记忆** ↔ **短期/长期记忆** 之间通过摘要提取和写入实现数据流动

**数据流转：**
```
当前对话 → 工作记忆（Step 4 已实现）
                ↓
         摘要提取（提取关键信息）
                ↓
    短期记忆（Redis，数小时-数天）
                ↓
         重要事件写入
                ↓
    长期记忆（数据库，永久保存）
```

### 2.2 Agent 状态机

```cpp
// Agent 内部状态
struct AgentState {
    // 情感状态（影响回复风格）
    struct Emotion {
        float mood;        // 心情: -1.0(低落) ~ 1.0(开心)
        float energy;      // 能量: 0.0(疲惫) ~ 1.0(活跃)
        float liking;      // 好感: -1.0(讨厌) ~ 1.0(喜欢)
    } emotion;
    
    // 认知状态
    struct Cognition {
        std::string current_topic;      // 当前话题
        std::string user_intent;        // 用户意图
        float engagement;                // 参与度: 0.0 ~ 1.0
    } cognition;
    
    // 记忆引用
    std::vector<MemoryPtr> active_memories;  // 激活的相关记忆
};

// 情感计算
class EmotionEngine {
public:
    // 根据用户输入和上下文计算情感变化
    void update(AgentState& state, const std::string& user_input) {
        // 情感分析：从用户输入检测情绪
        Sentiment sentiment = analyze_sentiment(user_input);
        
        // 更新 Agent 的共情状态
        state.emotion.mood = lerp(state.emotion.mood, sentiment.mood, 0.3f);
        
        // 好感度调整（根据互动质量）
        if (sentiment.is_positive) {
            state.emotion.liking = std::min(1.0f, state.emotion.liking + 0.05f);
        }
    }
    
    // 获取回复风格建议
    ReplyStyle suggest_style(const AgentState& state) {
        if (state.emotion.mood < -0.5f) {
            return ReplyStyle::EMPATHETIC;  // 用户低落，需要安慰
        } else if (state.emotion.energy > 0.7f) {
            return ReplyStyle::ENERGETIC;   // 用户活跃，可以活泼
        } else if (state.emotion.liking > 0.8f) {
            return ReplyStyle::FRIENDLY;    // 关系好，可以亲密
        }
        return ReplyStyle::NEUTRAL;
    }
};
```

---

## 三、记忆系统实现

### 3.1 记忆数据结构

```cpp
// 记忆类型
enum class MemoryType {
    FACT,       // 事实：用户基本信息、偏好
    EVENT,      // 事件：一次对话、一个经历
    PREFERENCE, // 偏好：喜欢/不喜欢
    RELATIONSHIP // 关系：和谁、什么关系
};

// 记忆重要性（影响保留优先级）
enum class Importance {
    CRITICAL = 5,   // 关键：姓名、重要日期
    HIGH = 4,       // 重要：偏好、重要事件
    MEDIUM = 3,     // 普通：日常对话
    LOW = 2,        // 次要：临时信息
    TRIVIAL = 1     // 琐碎：可以遗忘
};

// 记忆条目
struct Memory {
    std::string id;
    MemoryType type;
    Importance importance;
    
    std::string content;           // 记忆内容（自然语言）
    json structured_data;          // 结构化数据
    std::vector<float> embedding;  // 向量表示（用于语义检索）
    
    std::chrono::timestamp created_at;
    std::chrono::timestamp last_accessed;
    int access_count = 0;          // 访问次数（影响保留）
    
    float strength = 1.0f;         // 记忆强度（随时间衰减）
    
    // 关联
    std::vector<std::string> related_memories;
    std::vector<std::string> tags;
};

// 记忆创建示例
Memory create_user_fact(const std::string& user_id,
                        const std::string& fact,
                        Importance importance) {
    return Memory{
        .id = generate_uuid(),
        .type = MemoryType::FACT,
        .importance = importance,
        .content = fact,
        .structured_data = {
            {"user_id", user_id},
            {"fact_type", "basic_info"}
        },
        .created_at = now(),
        .strength = static_cast<float>(importance) / 5.0f
    };
}
```

### 3.2 记忆管理器

```cpp
class MemoryManager {
public:
    MemoryManager(std::shared_ptr<VectorStore> vector_store,
                  std::shared_ptr<Database> db)
        : vector_store_(vector_store), db_(db) {}
    
    // 添加记忆
    void add_memory(const std::string& user_id, Memory memory) {
        // 1. 获取向量嵌入
        memory.embedding = embedding_client_.embed(memory.content);
        
        // 2. 存储到向量数据库（用于语义检索）
        vector_store_>add(memory.id, memory.embedding, memory);
        
        // 3. 持久化到数据库
        db_>save_memory(user_id, memory);
        
        // 4. 更新工作记忆
        working_memory_[user_id].push_back(memory);
    }
    
    // 检索相关记忆
    std::vector<Memory> retrieve_relevant(
        const std::string& user_id,
        const std::string& query,
        size_t top_k = 5
    ) {
        // 1. 向量化查询
        auto query_emb = embedding_client_.embed(query);
        
        // 2. 向量相似度搜索
        auto candidates = vector_store_>search(query_emb, top_k * 2);
        
        // 3. 过滤和排序（考虑记忆强度、重要性、时效性）
        std::vector<Memory> results;
        for (const auto& mem : candidates) {
            // 计算综合得分
            float score = calculate_relevance_score(mem, query);
            if (score > 0.5f) {
                results.push_back(mem);
                
                // 更新访问统计（强化记忆）
                update_access_stats(mem);
            }
        }
        
        // 按得分排序
        std::sort(results.begin(), results.end(),
            [](const Memory& a, const Memory& b) {
                return a.strength > b.strength;
            }
        );
        
        return std::vector<Memory>(results.begin(),
                                      results.begin() + std::min(top_k, results.size()));
    }
    
    // 记忆衰减（定期调用）
    void decay_memories() {
        auto all_memories = db_>get_all_memories();
        
        for (auto& memory : all_memories) {
            // 时间衰减：越久不用的记忆越弱
            auto days_since_access = days_since(memory.last_accessed);
            float time_decay = std::exp(-0.1f * days_since_access);
            
            // 重要性保护：重要记忆衰减更慢
            float importance_factor = static_cast<float>(memory.importance) / 5.0f;
            
            // 更新强度
            memory.strength *= (0.5f + 0.5f * time_decay) * (0.5f + 0.5f * importance_factor);
            
            // 遗忘弱记忆
            if (memory.strength < 0.1f && memory.importance < Importance::HIGH) {
                db_>delete_memory(memory.id);
                vector_store_>remove(memory.id);
            } else {
                db_>update_memory(memory);
            }
        }
    }
    
    // 生成用户画像摘要
    std::string generate_profile_summary(const std::string& user_id) {
        auto facts = db_>get_memories_by_type(user_id, MemoryType::FACT);
        auto preferences = db_>get_memories_by_type(user_id, MemoryType::PREFERENCE);
        
        std::ostringstream summary;
        summary << "用户画像：\n";
        
        for (const auto& fact : facts) {
            if (fact.importance >= Importance::HIGH) {
                summary << "• " << fact.content << "\n";
            }
        }
        
        summary << "\n偏好：\n";
        for (const auto& pref : preferences) {
            summary << "• " << pref.content << "\n";
        }
        
        return summary.str();
    }

private:
    float calculate_relevance_score(const Memory& memory,
                                    const std::string& query) {
        // 语义相似度（向量）
        float semantic_score = vector_similarity(memory.embedding, 
                                                  embedding_client_.embed(query));
        
        // 时效性
        float recency_score = std::exp(-0.01f * hours_since(memory.created_at));
        
        // 访问频率
        float frequency_score = std::min(1.0f, memory.access_count / 10.0f);
        
        // 记忆强度
        float strength_score = memory.strength;
        
        // 加权综合
        return 0.4f * semantic_score +
               0.2f * recency_score +
               0.1f * frequency_score +
               0.3f * strength_score;
    }

    std::shared_ptr<VectorStore> vector_store_;
    std::shared_ptr<Database> db_;
    EmbeddingClient embedding_client_;
    std::map<std::string, std::vector<Memory>> working_memory_;
};
```

---

## 四、个性化回复生成

```cpp
class PersonalizedAgent {
public:
    PersonalizedAgent(LLMClient& llm,
                      MemoryManager& memory,
                      EmotionEngine& emotion)
        : llm_(llm), memory_(memory), emotion_(emotion) {}
    
    void chat(const std::string& user_id,
              const std::string& input,
              std::function<void(const std::string&)> callback) {
        
        // 1. 检索相关记忆
        auto relevant_memories = memory_.retrieve_relevant(user_id, input, 5);
        
        // 2. 获取用户画像
        std::string profile = memory_.generate_profile_summary(user_id);
        
        // 3. 更新情感状态
        AgentState state;
        emotion_.update(state, input);
        auto style = emotion_.suggest_style(state);
        
        // 4. 构建个性化 Prompt
        std::string prompt = build_personalized_prompt(
            input, profile, relevant_memories, style
        );
        
        // 5. 调用 LLM
        llm_.chat({{"user", prompt}}, 
            [this, user_id, input, callback](const std::string& response) {
                // 6. 提取并存储新记忆
                extract_and_store_memories(user_id, input, response);
                
                callback(response);
            }
        );
    }

private:
    std::string build_personalized_prompt(
        const std::string& input,
        const std::string& profile,
        const std::vector<Memory>& memories,
        ReplyStyle style
    ) {
        std::ostringstream prompt;
        
        // 系统指令 + 风格
        prompt << "你是一位";
        switch (style) {
            case ReplyStyle::EMPATHETIC:
                prompt << "温暖体贴、善于倾听的";
                break;
            case ReplyStyle::ENERGETIC:
                prompt << "活泼热情、充满活力的";
                break;
            case ReplyStyle::FRIENDLY:
                prompt << "亲切友好、像老朋友一样的";
                break;
            default:
                prompt << "专业友好的";
        }
        prompt << "AI 助手。\n\n";
        
        // 用户画像
        prompt << "关于用户：\n" << profile << "\n";
        
        // 相关记忆
        if (!memories.empty()) {
            prompt << "你可能记得：\n";
            for (const auto& mem : memories) {
                prompt << "• " << mem.content << "\n";
            }
            prompt << "\n";
        }
        
        // 当前输入
        prompt << "用户说：" << input << "\n";
        prompt << "你的回复：";
        
        return prompt.str();
    }
    
    void extract_and_store_memories(const std::string& user_id,
                                    const std::string& input,
                                    const std::string& response) {
        // 使用 LLM 提取关键信息
        std::string extraction_prompt = R"(
从对话中提取值得记忆的信息。
只提取重要的事实、偏好、事件。

用户：)" + input + R"(
助手：)" + response + R"(

以 JSON 格式输出：
{
  "memories": [
    {"type": "fact", "content": "...", "importance": 4},
    {"type": "preference", "content": "...", "importance": 3}
  ]
}
)";
        
        llm_.chat({{"user", extraction_prompt}},
            [this, user_id](const std::string& result) {
                try {
                    json j = json::parse(result);
                    for (const auto& mem : j["memories"]) {
                        Memory memory{
                            .type = parse_memory_type(mem["type"]),
                            .importance = static_cast<Importance>(mem["importance"].get<int>()),
                            .content = mem["content"],
                            .created_at = now()
                        };
                        memory_.add_memory(user_id, memory);
                    }
                } catch (...) {
                    // 解析失败，忽略
                }
            }
        );
    }

    LLMClient& llm_;
    MemoryManager& memory_;
    EmotionEngine& emotion_;
};
```

---

## 五、本章小结

**核心收获：**

1. **三层记忆架构**：
   - 工作记忆：当前对话上下文
   - 短期记忆：会话摘要、近期事件
   - 长期记忆：用户画像、永久知识

2. **情感状态机**：
   - 心情、能量、好感度三维状态
   - 影响回复风格和内容

3. **记忆管理**：
   - 向量存储 + 语义检索
   - 记忆强度与衰减机制
   - 自动信息提取

---

## 六、引出的问题

### 6.1 实战应用问题

现在 Agent 已经具备完整能力：
- ✅ 工具调用（Step 6-9）
- ✅ RAG 检索（Step 10）
- ✅ 多 Agent 协作（Step 11）
- ✅ 配置管理（Step 12）
- ✅ 监控告警（Step 13）
- ✅ 部署运维（Step 14）
- ✅ IM 平台接入（Step 15）
- ✅ 状态与记忆（Step 16）

**需要一个完整的实战项目**来综合运用这些能力。

---

**后续章节预告（Step 17+）：大型实战项目**

我们将从零构建一个**智能客服 SaaS 平台**，分成多章详细讲解：
- **Step 17**: 需求分析与架构设计
- **Step 18**: 核心功能实现（客服 Agent、知识库）
- **Step 19**: 高级功能（多租户、人机协作、数据分析）
- **Step 20**: 生产部署与运维

这是一个基于前面所有章节代码的、可直接商用的完整项目。
