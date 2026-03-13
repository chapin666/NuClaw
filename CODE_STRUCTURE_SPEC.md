# NuClaw 代码结构规范 V2

## 原则

1. **Step 0-5: 单文件** (简单概念，专注学习)
2. **Step 6-9: 模块化** (工具系统复杂，必须拆分)
3. **Step 10+: 模块化 + 目录组织** (企业级功能)

## 各 Step 结构定义

### Step 0-5: 单文件模式
```
src/step0X/
├── CMakeLists.txt
└── main.cpp          (唯一源文件，< 600行)
```

### Step 6-9: 模块化模式
```
src/step0X/
├── CMakeLists.txt
├── main.cpp          (入口，< 100行)
├── chat_engine.hpp/cpp    (AI核心)
├── llm_client.hpp/cpp     (LLM调用)
├── tool.hpp               (工具基类)
├── tool_registry.hpp/cpp  (工具注册表)
├── calculator.hpp         (具体工具)
├── weather_tool.hpp       (具体工具)
└── server.hpp/cpp         (WebSocket服务器)
```

### Step 10+: 分层目录模式
```
src/stepXX/
├── CMakeLists.txt
├── main.cpp
├── core/               (核心业务)
│   ├── chat_engine.hpp/cpp
│   └── task_scheduler.hpp/cpp
├── tools/              (工具实现)
│   ├── tool.hpp
│   ├── registry.hpp/cpp
│   └── *.hpp           (具体工具)
├── llm/                (LLM相关)
│   └── llm_client.hpp/cpp
└── network/            (网络层)
    └── server.hpp/cpp
```

## 具体规范

### Step 6 应该模块化
原因：
- 引入 Tool 概念，需要基类定义
- 有多个工具实现（天气、时间、计算）
- 有 ToolExecutor 执行器
- 单文件会超过 600 行，难读

目标结构：
```
src/step06/
├── CMakeLists.txt
├── main.cpp
├── chat_engine.hpp/cpp       (Agent Loop 核心)
├── llm_client.hpp/cpp        (LLM 调用)
├── tool_executor.hpp/cpp     (工具执行)
├── tool.hpp                  (工具基类)
├── weather_tool.hpp          (天气工具)
├── time_tool.hpp             (时间工具)
└── calc_tool.hpp             (计算工具)
```

### Step 8 保持模块化
当前已经模块化，但需要优化结构：
```
src/step08/
├── CMakeLists.txt
├── main.cpp
├── core/
│   ├── chat_engine.hpp/cpp
│   └── sandbox.hpp           (安全沙箱)
├── llm/
│   └── llm_client.hpp/cpp
├── tools/
│   ├── tool.hpp
│   ├── registry.hpp/cpp
│   ├── http_tool.hpp/cpp     (HTTP工具+SSRF保护)
│   ├── file_tool.hpp/cpp     (文件工具+沙箱)
│   └── code_tool.hpp/cpp     (代码执行+黑名单)
└── network/
    └── server.hpp/cpp
```

## 执行计划

1. **重构 Step 6**: 从单文件拆分为模块化
2. **优化 Step 8**: 按目录组织文件
3. **统一 CMakeLists.txt**: 每章的构建文件格式一致
4. **更新教程文档**: 代码结构变化后同步更新
