# NuClaw - 渐进式 AI Agent 教程

## Step 16-19: 项目实战（渐进式演进版）

### 演进路径

```
Step 15: Agent 基类
    ↓ 继承 + 扩展
Step 16: StatefulAgent (添加状态记忆)
    ↓ 继承 + 旅行知识
Step 17: TravelAgent (旅行助手)
    ↓ 多实例 + 协作
Step 18: NPCAgent (虚拟咖啡厅)
    ↓ 多租户 + 隔离
Step 19: SaaS Platform (多租户平台)
```

### 编译运行

```bash
cd src/step16 && g++ -std=c++17 -I include src/main.cpp -o step16_demo && ./step16_demo
cd src/step17 && g++ -std=c++17 -I include src/main.cpp -o step17_demo && ./step17_demo
cd src/step18 && g++ -std=c++17 -I include src/main.cpp -o step18_demo && ./step18_demo
cd src/step19 && g++ -std=c++17 -I include src/main.cpp -o step19_demo -lpthread && ./step19_demo
```

### 文件结构

每个步骤包含：
- `stepXX_progressive.hpp` - 渐进式演进的核心代码
- `src/main.cpp` - 演示程序
- 依赖前一章的 `stepYY_progressive.hpp`
