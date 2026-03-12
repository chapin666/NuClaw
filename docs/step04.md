# Step 4: LLM 集成

> 目标：连接 OpenAI API，实现真正的 AI 对话。
> 
> 代码量：约 250 行

## 新增功能

- HTTPS 客户端 (OpenSSL)
- OpenAI API 调用
- 命令系统

## 命令

| 命令 | 功能 |
|------|------|
| `/echo <text>` | 直接返回 |
| `/clear` | 清空历史 |
| `/llm_on/off` | 切换模式 |

## 环境

```bash
export OPENAI_API_KEY=sk-xxx
./server
```

## 编译

```bash
g++ -std=c++17 main.cpp -o server \
    -lboost_system -lssl -lcrypto -lpthread
```

## 下一步

→ [Step 5: 多租户](step05.md)
