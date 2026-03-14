# Step 16: Agent 状态与记忆系统

基于 Step 15 的 IM 平台接入代码，添加 Agent 状态管理和记忆系统。

## 新增文件

1. `include/nuclaw/agent_state.hpp` - Agent 状态定义（情感、记忆、关系）
2. `include/nuclaw/memory_manager.hpp` - 记忆管理器（短期/长期记忆）
3. `include/nuclaw/personalized_chat.hpp` - 个性化对话引擎

## 核心功能

- 情感状态计算（开心、精力、信任、兴趣）
- 三层记忆系统（短期、工作、长期）
- 用户关系管理（熟悉度、好感度）
- 个性化对话（根据关系和记忆调整回复）
