# Step 3: 规则 AI —— 基于关键词的 Agent

> 目标：实现基于规则的意图识别，让 Agent 能"听懂"用户的话
> 
003e 难度：⭐⭐⭐ | 代码量：约 350 行 | 预计学习时间：2-3 小时

---

## 一、为什么需要意图识别？

### 1.1 Step 2 的局限

Step 2 实现了 HTTP 路由，但 Agent 的回复还是固定的：

```cpp
router.add_route("GET", "/chat", [](const Request& req) {
    std::string msg = req.get_param("message");
    // 不管什么输入，都返回 Echo
    return "Echo: " + msg;
});
```

**实际场景：** 用户可能用不同方式表达同一意图：

```
意图：查询天气
用户输入：                              Agent 应该：
- "北京天气怎么样？"                    → 调用天气 API 查北京
- "今天会下雨吗？"                       → 调用天气 API 查本地
- "明天需要带伞吗？"                     → 调用天气 API 查明天预报
- "What's the weather in Shanghai?"      → 调用天气 API 查上海
```

**问题：** 如何用代码理解这些不同表述背后的**同一意图**？

### 1.2 规则引擎的价值

在 LLM 出现之前，对话系统依赖**规则引擎**处理用户输入：

```
用户输入
    │
    ▼
┌─────────────────┐
│   意图识别      │  ← 规则匹配、关键词提取
│  (Intent Classifier) │
└────────┬────────┘
         │
    ┌────┴────┐
    │         │
    ▼         ▼
 查询天气   查询时间
    │         │
    ▼         ▼
┌─────────┐ ┌─────────┐
│天气 API │ │系统调用 │
└─────────┘ └─────────┘
```

**规则 AI 的优势：**
- 响应速度快（无需调用 LLM）
- 结果确定（不会随机）
- 成本低（无 API 费用）
- 适合处理明确的、固定的任务

---

## 二、意图识别核心概念

### 2.1 意图（Intent）与实体（Entity）

**意图（Intent）**：用户想要做什么

| 用户输入 | 意图 |
|:---|:---|
| "北京天气怎么样？" | `query_weather` |
| "现在几点了？" | `query_time` |
| "讲个笑话" | `tell_joke` |
| "帮我订个闹钟" | `set_alarm` |

**实体（Entity）**：意图中的关键参数

```
用户输入："明天北京会下雨吗？"

意图：query_weather
实体：
  - location: "北京"
  - date: "明天"
  - concern: "是否会下雨"
```

**实体类型：**

| 类型 | 说明 | 示例 |
|:---|:---|:---|
| 地点（location）| 城市、国家、地标 | 北京、纽约、故宫 |
| 时间（datetime）| 绝对或相对时间 | 明天、下午3点、下周一 |
| 人物（person）| 人名 | 张三、马云 |
| 数字（number）| 数量、价格 | 100元、5个 |

### 2.2 匹配策略对比

| 策略 | 实现方式 | 优点 | 缺点 |
|:---|:---|:---|:---|
| **精确匹配** | `if (input == "天气")` | 简单 | 无法处理变体 |
| **关键词匹配** | `if (input.contains("天气"))` | 灵活 | 容易误判 |
| **正则匹配** | `regex_match(input, pattern)` | 强大 | 维护复杂 |
| **相似度匹配** | 编辑距离、向量相似 | 容错好 | 计算量大 |

**本章重点：关键词匹配 + 正则提取**（平衡灵活性和复杂度）

---

## 三、规则引擎设计

### 3.1 意图定义结构

```cpp
// 实体定义
struct Entity {
    std::string name;        // 实体名：location, date
    std::string type;        // 类型：string, regex, keyword
    std::string pattern;     // 匹配模式
    bool required;           // 是否必须
};

// 意图定义
struct Intent {
    std::string name;                    // 意图名：query_weather
    std::vector<std::string> keywords;   // 触发关键词
    std::vector<Entity> entities;       // 需要提取的实体
    std::function<std::string(const std::map<std::string, std::string>&)> handler;
};

// 识别结果
struct IntentResult {
    std::string intent;                          // 识别出的意图
    float confidence;                            // 置信度（0-1）
    std::map<std::string, std::string> entities;  // 提取的实体
};
```

### 3.2 规则引擎核心类

```cpp
class RuleEngine {
public:
    // 注册意图
    void register_intent(const Intent& intent);
    
    // 识别意图
    IntentResult recognize(const std::string& input);

private:
    // 计算匹配分数
    float calculate_score(const std::string& input, const Intent& intent);
    
    // 提取实体
    std::map<std::string, std::string> extract_entities(
        const std::string& input, 
        const std::vector<Entity>& entities
    );
    
    std::vector<Intent> intents_;
};
```

---

## 四、核心代码实现

### 4.1 关键词匹配

```cpp
float RuleEngine::calculate_score(const std::string& input, const Intent& intent) {
    float score = 0.0f;
    std::string lower_input = to_lower(input);
    
    // 1. 关键词匹配得分
    for (const auto& keyword : intent.keywords) {
        if (lower_input.find(to_lower(keyword)) != std::string::npos) {
            // 关键词命中，增加分数
            score += 1.0f;
            
            // 完整匹配权重更高
            if (lower_input == to_lower(keyword)) {
                score += 0.5f;
            }
        }
    }
    
    // 2. 关键词覆盖率
    if (!intent.keywords.empty()) {
        float coverage = score / intent.keywords.size();
        score = coverage;
    }
    
    return score;
}

IntentResult RuleEngine::recognize(const std::string& input) {
    IntentResult best_result;
    best_result.confidence = 0.0f;
    
    // 遍历所有意图，找到最佳匹配
    for (const auto& intent : intents_) {
        float score = calculate_score(input, intent);
        
        if (score > best_result.confidence) {
            best_result.intent = intent.name;
            best_result.confidence = score;
            best_result.entities = extract_entities(input, intent.entities);
        }
    }
    
    return best_result;
}
```

**匹配逻辑说明：**

1. **大小写不敏感**：统一转小写后匹配
2. **多关键词支持**：一个意图可以有多个触发词
3. **完整匹配加权**：如果输入完全等于关键词，置信度更高
4. **覆盖率计算**：命中的关键词占总关键词的比例

### 4.2 实体提取

```cpp
std::map<std::string, std::string> RuleEngine::extract_entities(
    const std::string& input,
    const std::vector<Entity>& entities) {
    
    std::map<std::string, std::string> result;
    
    for (const auto& entity : entities) {
        if (entity.type == "regex") {
            // 使用正则表达式提取
            std::regex re(entity.pattern);
            std::smatch match;
            
            if (std::regex_search(input, match, re)) {
                result[entity.name] = match[1].str();  // 取第一个捕获组
            }
        } else if (entity.type == "keyword_list") {
            // 从预定义列表中匹配
            std::istringstream stream(entity.pattern);
            std::string keyword;
            
            while (stream >> keyword) {
                if (input.find(keyword) != std::string::npos) {
                    result[entity.name] = keyword;
                    break;
                }
            }
        } else if (entity.type == "datetime") {
            // 时间解析（简化版）
            result[entity.name] = parse_datetime(input);
        }
    }
    
    return result;
}
```

**实体提取示例：**

```cpp
// 定义实体：地点
Entity location_entity{
    .name = "location",
    .type = "keyword_list",
    .pattern = "北京 上海 广州 深圳 杭州 南京",  // 预定义城市列表
    .required = true
};

// 定义实体：日期
Entity date_entity{
    .name = "date",
    .type = "regex",
    .pattern = "(今天|明天|后天|下周[一二三四五六日]|\d{4}-\d{2}-\d{2})",
    .required = false  // 非必须，默认今天
};

// 提取示例
// 输入："明天北京会下雨吗？"
// 结果：{ "date": "明天", "location": "北京" }
```

### 4.3 意图注册与处理

```cpp
void setup_intents(RuleEngine& engine) {
    // 意图 1：查询天气
    Intent weather_intent{
        .name = "query_weather",
        .keywords = {"天气", "温度", "下雨", "雪", "几度", "forecast", "weather"},
        .entities = {
            {
                .name = "location",
                .type = "keyword_list", 
                .pattern = "北京 上海 广州 深圳 杭州",
                .required = true
            },
            {
                .name = "date",
                .type = "regex",
                .pattern = "(今天|明天|后天)",
                .required = false
            }
        },
        .handler = [](const std::map<std::string, std::string>& entities) {
            std::string location = entities.count("location") ? 
                                   entities.at("location") : "本地";
            std::string date = entities.count("date") ? 
                              entities.at("date") : "今天";
            
            // 实际应该调用天气 API
            return date + location + "的天气是晴天，25°C";
        }
    };
    
    // 意图 2：查询时间
    Intent time_intent{
        .name = "query_time",
        .keywords = {"时间", "几点", "现在", "time", "clock"},
        .entities = {},  // 不需要提取实体
        .handler = [](const std::map<std::string, std::string>&) {
            auto now = std::chrono::system_clock::now();
            auto time = std::chrono::system_clock::to_time_t(now);
            std::stringstream ss;
            ss << std::put_time(std::localtime(&time), "%H:%M:%S");
            return "现在是 " + ss.str();
        }
    };
    
    // 意图 3：问候
    Intent greeting_intent{
        .name = "greeting",
        .keywords = {"你好", "您好", "Hello", "Hi", "在吗"},
        .entities = {},
        .handler = [](const std::map<std::string, std::string>&) {
            return "你好！有什么可以帮您的吗？";
        }
    };
    
    // 注册到引擎
    engine.register_intent(weather_intent);
    engine.register_intent(time_intent);
    engine.register_intent(greeting_intent);
}
```

---

## 五、集成到 HTTP 服务器

### 5.1 Agent Session 类

```cpp
class AgentSession : public std::enable_shared_from_this<AgentSession> {
public:
    AgentSession(tcp::socket socket, RuleEngine& engine)
        : socket_(std::move(socket)),
          engine_(engine) {}
    
    void start() {
        do_read();
    }

private:
    void do_read() {
        auto self = shared_from_this();
        
        socket_.async_read_some(
            asio::buffer(buffer_),
            [this, self](error_code ec, size_t length) {
                if (ec) return;
                
                // 解析 HTTP 请求
                HttpParser parser;
                Request req;
                
                if (parser.parse(buffer_.data(), length, req)) {
                    handle_chat(req);
                }
            }
        );
    }
    
    void handle_chat(const Request& req) {
        // 获取用户消息
        std::string message = req.get_param("message");
        
        // 意图识别
        IntentResult result = engine_.recognize(message);
        
        std::string reply;
        
        if (result.confidence > 0.5f) {
            // 找到匹配的意图，执行 handler
            auto intent = engine_.find_intent(result.intent);
            if (intent && intent->handler) {
                reply = intent->handler(result.entities);
            }
        } else {
            // 没有匹配到意图
            reply = "抱歉，我不太理解您的意思。您可以问：\n"
                   "- 北京天气怎么样\n"
                   "- 现在几点了\n"
                   "- 你好";
        }
        
        // 构造 HTTP 响应
        std::string response = 
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "\r\n"
            "{\"reply\":\"" + escape_json(reply) + "\","
            "\"intent\":\"" + result.intent + "\","
            "\"confidence\":" + std::to_string(result.confidence) + "}";
        
        do_write(response);
    }
    
    void do_write(const std::string& response) {
        auto self = shared_from_this();
        asio::async_write(
            socket_,
            asio::buffer(response),
            [this, self](error_code ec, size_t) {
                if (!ec) {
                    // Keep-Alive：继续等待下一个请求
                    do_read();
                }
            }
        );
    }

    tcp::socket socket_;
    RuleEngine& engine_;
    std::array<char, 8192> buffer_;
};
```

---

## 六、规则引擎的局限

### 6.1 规则系统的边界

规则 AI 能处理明确的任务，但有明显局限：

```
能处理：                              不能处理：
─────────────────────────────────────────────────────────────
"北京天气"        → query_weather      "我想出去玩，合适吗？"
"几点了"          → query_time         （需要理解"出去玩"和天气关联）
"讲个笑话"        → tell_joke          
                                      "昨天说的那个事怎么样了？"
                                      （没有上下文记忆）
                                      
"订明天去上海的票"  → 提取实体          "帮我安排个周末短途旅行"
  - 时间：明天                          （任务太复杂，需要多步规划）
  - 目的地：上海
```

**核心局限：**
1. **语义理解浅**：只能匹配关键词，不能理解深层语义
2. **无上下文**：每个请求独立，无法记住之前的对话
3. **扩展性差**：新增意图需要改代码、重新编译
4. **容错性弱**：用户输入稍有变化就可能识别失败

### 6.2 走向 LLM

规则 AI 适合作为：
- **快速响应层**：先匹配规则，命中则直接返回（快）
- **意图预分类**：缩小 LLM 的处理范围
- **兜底保障**：LLM 不可用时降级使用

但真正的智能需要 **LLM（大语言模型）**：

```
用户："我想去一个不太热、有海、消费不高的地方待几天"

规则 AI：无法理解（没有关键词命中）

LLM：
- 理解语义：用户想找避暑、海滨、性价比高的旅游目的地
- 推理：推荐青岛、大连、厦门等城市
- 多轮对话：询问具体时间、预算进一步细化推荐
```

---

## 七、本章小结

**核心收获：**

1. **意图识别**：理解 Intent（做什么）和 Entity（参数）的概念

2. **规则引擎**：
   - 关键词匹配算法
   - 正则表达式实体提取
   - 置信度计算

3. **规则 AI 架构**：
   ```
   用户输入 → 意图识别 → 实体提取 → Handler 执行 → 生成回复
   ```

4. **局限认知**：
   - 规则适合明确、固定的任务
   - 复杂语义理解需要 LLM

---

## 八、引出的问题

### 8.1 上下文问题

```
用户：你好，我叫小明
Agent：你好小明！

用户：我叫什么名字？
Agent：抱歉，我不知道  ← 因为没有保存对话历史
```

**问题：** 如何维护对话状态，让 Agent 能记住之前说的话？

### 8.2 实时性问题

目前的 HTTP 请求-响应模式：
```
用户发送 → 服务器处理 → 返回结果
     ↑___________________________↓
              （等待整个流程）
```

**问题：** 如果处理需要 10 秒（如复杂计算），用户只能干等。

**更好的体验：** 边处理边推送进度。

### 8.3 推送问题

HTTP 是请求-响应模式，服务器无法主动推送：

```
需要的能力：
Agent: "正在查询天气..."  （主动推送进度）
Agent: "找到了，北京今天..."  （主动推送结果）

HTTP 限制：
服务器只能被动响应请求，不能主动发送
```

---

**下一章预告（Step 4）：**

我们将实现 **HTTP Session 管理**：
- 解决 HTTP 无状态问题，支持多轮对话
- Session ID 机制：服务端维护对话上下文
- 上下文关联：理解"那上海呢"这类指代问题
- 为接入 LLM 的复杂对话做准备

规则 AI 解决了"理解用户"的基础问题，接下来要让对话能记住上下文、支持连续交互。
