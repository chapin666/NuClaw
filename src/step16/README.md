# Step 15: IM 平台接入

基于 Step 14 的生产就绪代码，添加 IM 平台适配器。

## 新增文件

1. `include/nuclaw/im_adapter.hpp` - IM 平台适配器接口
2. `include/nuclaw/feishu_adapter.hpp` - 飞书适配器
3. `include/nuclaw/dingtalk_adapter.hpp` - 钉钉适配器
4. `src/im_server.cpp` - IM 消息服务器

## 核心功能

- Webhook 接收消息
- 事件订阅处理
- 多平台消息格式统一
- 群聊 @提及处理
