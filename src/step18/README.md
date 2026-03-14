# Step 18: 虚拟咖啡厅「星语轩」

☕ 多 NPC 虚拟世界演示

## 快速开始

```bash
# 编译
g++ -std=c++17 -I include src/main.cpp -o starcafe

# 运行
./starcafe
```

## 交互命令

- `look` - 观察环境
- `talk <npc>` - 与 NPC 对话
- `wait` - 等待一小时
- `quit` - 离开

## 文件结构

```
include/starcafe/
└── simple_npc.hpp    # NPC 定义
src/
└── main.cpp          # 主程序
```
