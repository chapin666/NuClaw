# Step 5: LLM 接入 - 从规则到语义理解

> 目标：理解 LLM 的工作原理，接入真实 AI 能力
> 
> 难度：⭐⭐⭐ (中等)
> 
> 代码量：约 450 行
> 
> 预计学习时间：3-4 小时

---

## 📚 前置知识

### 什么是 LLM？

**LLM（Large Language Model，大语言模型）** 是一种基于深度学习的自然语言处理模型。

**通俗理解：**
- 它读过互联网上的大量文本（书籍、网页、论文等）
- 它学会了语言的模式和知识
- 它能理解你的问题，并生成合理的回答

**代表模型：**
- **GPT-4** / GPT-3.5（OpenAI）
- **Claude**（Anthropic）
- **文心一言**（百度）
- **通义千问**（阿里）

### 为什么需要 LLM？

回顾 Step 4 的规则系统：

```
用户：今天适合出门吗？
AI：我不理解。          ← 失败！

用户：外面天气怎么样？
AI：我不理解。          ← 失败！

用户：今天外面如何？
AI：我不理解。          ← 失败！
```

**都涉及天气，但因为不包含"天气"关键词，全部失败！**

**LLM 的优势：**

```
用户：今天适合出门吗？
LLM 思考：
1. "出门"通常和天气有关
2. "适合"是询问建议
3. 用户想了解天气情况

回复：这取决于天气。如果晴天且温度适宜，就很适合出门！
```

**LLM 理解语义，不只是匹配关键词。**

---

## 第一步：规则系统 vs LLM 深度对比

### 规则系统的局限性

**代码示例：**

```cpp
// 规则系统（Step 4）
string process(string input) {
    if (regex_match(input, "天气")) return get_weather();
    if (regex_match(input, "时间")) return get_time();
    if (regex_match(input, "你好")) return "你好！";
    return "我不理解";
}
```

**问题分析：**

| 用户输入 | 能否回答 | 原因 |
|:---|:---:|:---|
| 今天天气如何？ | ✅ | 匹配"天气" |
| 外面天气怎么样？ | ❌ | 不匹配 |
| 今天适合出门吗？ | ❌ | 不匹配 |
| 现在什么时辰？ | ❌ | 匹配"时间"但用词不同 |
| 告诉我时间 | ❌ | 不完全匹配 |

**维护噩梦：**

如果要支持 100 种问法，需要写 100 个正则表达式！

```cpp
// 天气的 100 种问法（崩溃！）
if (regex_match("天气")) ...
if (regex_match("气温")) ...
if (regex_match("温度")) ...
if (regex_match("气候")) ...
if (regex_match("下不下雨")) ...
if (regex_match("热不热")) ...
// ... 还有 94 个
```

### LLM 的解决方式

**核心能力：语义理解**

```
用户：今天适合出门吗？

LLM 处理流程：
1. 分词：[今天, 适合, 出门, 吗, ？]

2. 语义分析：
   - "今天" → 时间词
   - "出门" → 活动，通常关联天气
   - "适合" → 询问建议/条件
   - "吗" → 疑问句

3. 知识关联：
   - 出门 → 受天气影响
   - 适合 → 需要评估条件
   - 今天 → 当前时间

4. 推理：
   - 用户想知道今天的天气情况
   - 以便决定是否出门

5. 生成回复：
   "这取决于天气。建议查看天气预报，如果是晴天且温度适宜，就很适合出门！"
```

### 对比总结

| 维度 | 规则系统 | LLM |
|:---|:---|:---|
| **理解方式** | 关键词匹配 | 语义理解 |
| **泛化能力** | 差（没见过就不会） | 强（能推理） |
| **维护成本** | 高（规则越多越难维护） | 低（模型自动学习） |
| **确定性** | 高（同样输入同样输出） | 中（有一定随机性） |
| **资源消耗** | 低（简单计算） | 高（神经网络推理） |
| **可解释性** | 高（知道为什么这样回复） | 低（黑盒） |
| **适用场景** | 简单确定的任务 | 复杂开放的任务 |

---

## 第二步：LLM 工作原理（简化版）

### 训练过程（了解即可）

**Step 1: 预训练**
```
输入：互联网上的海量文本
      （书籍、网页、论文、代码...）
      ↓
模型学习：词语之间的关系
         语法规则
         世界知识
      ↓
输出：基础语言模型
```

**Step 2: 微调（Fine-tuning）**
```
输入：对话数据（问题-回答对）
      ↓
模型学习：如何对话
         如何遵循指令
      ↓
输出：Chat 模型（如 GPT-3.5-turbo）
```

### 推理过程（实际使用的部分）

```
用户输入：北京天气如何？
         ↓
分词：[北京, 天气, 如何, ？]
         ↓
向量化：将每个词转为向量（一串数字）
         ↓
神经网络计算：
   ┌─────────────────────────┐
   │  Transformer 网络        │
   │  (多层注意力机制)        │
   │                         │
   │  输入向量 → 计算 → 输出向量 │
   └─────────────────────────┘
         ↓
生成回复：北京今天晴天，气温 25°C。
```

**关键概念：Token**

LLM 处理的不是"字符"，而是 **Token（词元）**：

```
文本："Hello, world!"
Token: ["Hello", ",", " world", "!"]
      ↓
ID:   [15496, 11, 995, 0]
```

**中文 Token 数量：**
- 英文：约 1 个 token/单词
- 中文：约 1.5-2 个 token/字

**上下文窗口：**
- GPT-3.5：4K tokens（约 3000 汉字）
- GPT-4：8K/32K tokens

---

## 第三步：接入 LLM API

### OpenAI API 介绍

**官方文档：** https://platform.openai.com/docs

**API 端点：**
```
POST https://api.openai.com/v1/chat/completions
```

**请求格式：**
```json
{
    "model": "gpt-3.5-turbo",
    "messages": [
        {"role": "system", "content": "你是 AI 助手"},
        {"role": "user", "content": "北京天气如何？"}
    ],
    "temperature": 0.7,
    "max_tokens": 500
}
```

**参数说明：**

| 参数 | 说明 | 示例 |
|:---|:---|:---|
| `model` | 模型名称 | `gpt-3.5-turbo`, `gpt-4` |
| `messages` | 对话历史 | 数组，包含 system/user/assistant |
| `temperature` | 创造性（0-2）| 0=确定，2=随机 |
| `max_tokens` | 最大回复长度 | 500 |

**角色说明：**
- `system`：系统提示，设定 AI 的身份和行为
- `user`：用户输入
- `assistant`：AI 回复（用于多轮对话）

### 代码实现

```cpp
// LLMClient 类 - 封装 OpenAI API 调用
class LLMClient {
public:
    LLMClient(const std::string& api_key) : api_key_(api_key) {}
    
    // 发送消息给 LLM，获取回复
    std::string complete(const std::vector<Message>& messages) {
        // 1. 构建请求 JSON
        json::object request;
        request["model"] = "gpt-3.5-turbo";
        request["temperature"] = 0.7;
        request["max_tokens"] = 500;
        
        // 2. 添加消息
        json::array msgs;
        for (const auto& [role, content] : messages) {
            json::object msg;
            msg["role"] = role;
            msg["content"] = content;
            msgs.push_back(msg);
        }
        request["messages"] = msgs;
        
        // 3. 发送 HTTP POST 请求（使用 Boost.Beast）
        std::string request_body = json::serialize(request);
        std::string response = http_post(
            "https://api.openai.com/v1/chat/completions",
            request_body,
            api_key_  // 用于 Authorization: Bearer
        );
        
        // 4. 解析响应
        return parse_response(response);
    }

private:
    std::string api_key_;
    
    std::string http_post(const std::string& url, 
                          const std::string& body,
                          const std::string& api_key) {
        // 使用 Boost.Beast 发送 HTTPS POST
        // ...（省略具体实现）
    }
    
    std::string parse_response(const std::string& response) {
        // 解析 OpenAI 的 JSON 响应
        json::value val = json::parse(response);
        return val.as_object()["choices"]
               .as_array()[0]
               .as_object()["message"]
               .as_object()["content"]
               .as_string();
    }
};
```

### Prompt 工程基础

**什么是 Prompt？**

Prompt 是给 LLM 的输入，决定了 LLM 的输出。

**好的 Prompt：**

```
角色设定：
"你是专业的天气助手，擅长回答天气相关问题。"

任务说明：
"请根据用户的问题，提供准确的天气信息和建议。"

约束条件：
"如果不确定，请建议用户查看官方天气应用。"
```

**代码中的 Prompt：**

```cpp
std::vector<Message> build_prompt(const std::string& user_input) {
    return {
        // System prompt - 设定 AI 的身份
        {"system", "你是 NuClaw AI 助手，一个友善的智能助手。"
                   "你能理解自然语言，回答各种问题。"
                   "如果不确定，请诚实告知。"},
        
        // User input - 用户问题
        {"user", user_input}
    };
}
```

---

## 第四步：对话历史管理

### 为什么需要历史？

**多轮对话示例：**

```
用户：我叫张三。
AI：你好张三！有什么可以帮你的？

用户：我的名字是什么？
AI：你的名字是张三。
      ↑
      需要记住上一轮的内容！
```

**没有历史会怎样？**

```
用户：我叫张三。
AI：你好张三！

用户：我的名字是什么？
AI：我不知道你的名字。
      ↑
      完全忘记了！
```

### 实现对话历史

```cpp
struct ChatContext {
    // 保存完整的对话历史
    std::vector<std::pair<std::string, std::string>> history;
    
    // 添加消息
    void add_message(const std::string& role, const std::string& content) {
        history.push_back({role, content});
    }
    
    // 转换为 LLM 需要的格式
    std::vector<Message> to_messages() const {
        std::vector<Message> msgs;
        // 先加 system prompt
        msgs.push_back({"system", "你是 NuClaw AI 助手..."});
        // 再加历史对话
        for (const auto& [role, content] : history) {
            msgs.push_back({role, content});
        }
        return msgs;
    }
};

class ChatEngine {
    std::string process(const std::string& user_input, ChatContext& ctx) {
        // 1. 添加用户消息到历史
        ctx.add_message("user", user_input);
        
        // 2. 调用 LLM（传入完整历史）
        std::string reply = llm_.complete(ctx.to_messages());
        
        // 3. 添加 AI 回复到历史
        ctx.add_message("assistant", reply);
        
        // 4. 限制历史长度（防止超出 token 限制）
        if (ctx.history.size() > 20) {
            // 删除最老的对话
            ctx.history.erase(ctx.history.begin(), 
                              ctx.history.begin() + 2);
        }
        
        return reply;
    }
};
```

### 历史长度限制

**为什么要限制？**

LLM 有上下文窗口限制（如 GPT-3.5 是 4K tokens）。

**策略：**
1. **滑动窗口**：只保留最近 N 轮对话
2. **摘要压缩**：把老对话压缩成摘要
3. **关键信息提取**：只保留重要事实

```cpp
// 简单策略：只保留最近 10 轮（20 条消息）
if (ctx.history.size() > 20) {
    ctx.history.erase(ctx.history.begin(), ctx.history.begin() + 2);
}
```

---

## 第五步：运行测试

### 设置 API Key

```bash
# Linux/Mac
export OPENAI_API_KEY="sk-your-api-key-here"

# Windows
set OPENAI_API_KEY=sk-your-api-key-here
```

**获取 API Key：**
1. 访问 https://platform.openai.com/
2. 注册/登录账号
3. 进入 API Keys 页面
4. 创建新的 API Key

### 编译运行

```bash
cd src/step05
mkdir build && cd build
cmake .. && make
export OPENAI_API_KEY="sk-xxx"  # 设置你的 key
./nuclaw_step05
```

### WebSocket 测试

```bash
wscat -c ws://localhost:8080/ws

> 你好
< 你好！我是基于 LLM 的 AI 助手。有什么可以帮你的吗？

> 今天适合出门吗？
< 这取决于天气。建议查看天气预报，如果是晴天且温度适宜，
  就很适合出门！
  
> 北京天气如何？
< 抱歉，我目前没有实时天气数据接入。
  建议查看天气 App 或网站获取最新信息。
```

### 发现问题！

**LLM 理解了语义，但无法获取实时数据！**

```
用户：北京现在多少度？
AI：抱歉，我没有实时天气数据。

原因：LLM 的知识是静态的（训练时的数据），
      不知道当前的实时信息。
```

**下一章解决方案：工具调用**
- LLM 理解需要天气 → 调用天气 API → 返回结果

---

## 本节总结

### 核心概念

1. **LLM**：大语言模型，理解语义而非匹配关键词
2. **API 调用**：通过 HTTP 与 LLM 服务通信
3. **Prompt 工程**：设计好的输入，得到好的输出
4. **对话历史**：维护上下文，实现多轮对话

### 代码演进

```
Step 4: WebSocket + 规则 AI (400行)
   ↓ 替换规则为 LLM
Step 5: WebSocket + LLM (450行)
   - LLMClient 类
   - HTTP API 调用
   - Prompt 构建
   - 对话历史管理
```

### Agent Loop 升级

```
Step 4: 输入 → 规则匹配 → 输出
          （死板，只能匹配关键词）

Step 5: 输入 → LLM 理解 → 输出
          （灵活，理解语义）
```

### 仍存在的问题

**LLM 无实时数据：**
- 不知道当前时间
- 不知道实时天气
- 无法查数据库

**下一章：工具调用（Function Calling）**
- 让 LLM 能调用外部工具
- 获取实时数据
- 执行实际操作

---

## 📝 课后练习

### 练习 1：改进 Prompt
设计更好的 system prompt：
- 让 AI 更友好
- 让 AI 承认不知道时不瞎编
- 让 AI 用特定的语气说话

### 练习 2：历史管理优化
实现更智能的历史管理：
- 把太久远的对话压缩成摘要
- 保留重要的用户事实（如用户名）

### 练习 3：多模型支持
修改代码支持多个 LLM：
- OpenAI GPT-3.5
- 文心一言
- 根据配置切换

### 思考题
1. 为什么 LLM 会"幻觉"（编造不存在的事实）？
2. 为什么 LLM 需要 temperature 参数？
3. 如何降低 LLM API 的调用成本？

---

## 📖 扩展阅读

### LLM 应用架构模式

**模式 1: Direct API**
```
用户 → 你的应用 → LLM API
```
- 简单直接
- 适合快速原型

**模式 2: RAG（检索增强生成）**
```
用户 → 检索相关知识 → LLM（结合知识生成）
```
- 减少幻觉
- 适合知识问答

**模式 3: Agent + Tools**
```
用户 → LLM 决策 → 调用工具 → 执行 → 回复
```
- 能执行实际任务
- 就是我们下一章要做的！

### Prompt 技巧速查

| 技巧 | 示例 |
|:---|:---|
| **角色设定** | "你是一位经验丰富的医生..." |
| **输出格式** | "请用 JSON 格式输出..." |
| **Few-shot** | "示例1：输入→输出\n示例2：输入→输出\n现在轮到你了..." |
| **Chain of Thought** | "让我们一步一步思考..." |
| **约束条件** | "如果不确定，请说不知道" |

---

**恭喜！** 你的 Agent 现在有了"大脑"。下一章我们将给它装上"手脚"——工具调用能力。
