# Step 16: Agent 状态与记忆系统

🤖 让 Agent 拥有情感和记忆

## 快速开始

```bash
# 编译
g++ -std=c++17 -I include src/main.cpp -o stateful_agent

# 运行
./stateful_agent
```

## 功能演示

- ✅ 短期记忆：记住最近对话
- ✅ 用户关系：跟踪熟悉度和好感度  
- ✅ 情感状态：根据交互变化
- ✅ 多用户隔离：区分不同用户

## 文件结构

```
include/nuclaw/
└── agent_state.hpp    # Agent 状态定义
src/
└── main.cpp           # 演示程序
```
