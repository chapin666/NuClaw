# NuClaw - AI Agent Framework

> 工程化 C++ AI Agent 框架

## 项目结构

```
nuclaw/
├── CMakeLists.txt          # 主构建配置
├── README.md               # 项目说明
├── configs/
│   └── config.yaml         # 配置文件
├── include/nuclaw/         # 头文件
│   ├── agent.hpp
│   ├── chat_engine.hpp
│   ├── tool.hpp
│   ├── tool_registry.hpp
│   └── ...
├── src/
│   └── main.cpp            # 主程序入口
├── tests/                  # 单元测试
│   ├── CMakeLists.txt
│   └── test_*.cpp
└── docs/                   # 文档
```

## 快速开始

### 编译

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### 运行

```bash
export OPENAI_API_KEY=sk-xxx
./nuclaw ../configs/config.yaml
```

### 运行测试

```bash
make test
```

## 功能特性

- ✅ 工具注册表模式
- ✅ RAG 检索增强
- ✅ 多 Agent 协作
- ✅ 配置管理
- ✅ 监控告警

## 许可证

MIT
