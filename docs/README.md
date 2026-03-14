# NuClaw 在线文档

本目录包含 NuClaw 项目的 MkDocs 配置文件，用于生成在线电子书。

## 快速开始

### 1. 安装依赖

```bash
pip install mkdocs-material mkdocs-minify-plugin
```

### 2. 本地预览

```bash
mkdocs serve
# 或
./build-docs.sh
```

然后在浏览器打开 http://127.0.0.1:8000

### 3. 构建静态站点

```bash
mkdocs build
```

输出目录为 `site/`，可以直接部署到任何静态托管服务。

### 4. 部署到 GitHub Pages

```bash
mkdocs gh-deploy
```

这会将 `site/` 目录的内容推送到 `gh-pages` 分支。

## 目录结构

```
NuClaw/
├── mkdocs.yml              # MkDocs 配置文件
├── docs/                   # 文档源文件
│   ├── index.md           # 首页
│   ├── stylesheets/       # 自定义 CSS
│   ├── javascripts/       # 自定义 JS
│   └── step00-14/         # 各章节（符号链接到 tutorials/）
├── tutorials/             # 原始教程文件
└── site/                  # 构建输出（自动生成）
```

## 自定义配置

### 修改主题颜色

编辑 `mkdocs.yml` 中的 `theme.palette` 部分：

```yaml
palette:
  - scheme: default
    primary: indigo    # 改为 blue、red、green 等
    accent: indigo
```

### 添加新页面

1. 在 `docs/` 下创建新的 Markdown 文件
2. 在 `mkdocs.yml` 的 `nav` 部分添加导航项

### 修改导航结构

编辑 `mkdocs.yml` 中的 `nav` 部分，调整章节顺序或分组。

## 特性

- 🔍 **全文搜索** - 即时搜索所有章节
- 🌙 **深色模式** - 支持亮色/深色主题切换
- 📱 **响应式设计** - 完美适配手机、平板、桌面
- 📝 **代码高亮** - 支持 C++ 语法高亮和复制按钮
- 🔗 **锚点导航** - 章节内快速跳转
- 📊 **Mermaid 图表** - 支持流程图、时序图等

## 参考

- [MkDocs 官方文档](https://www.mkdocs.org/)
- [Material for MkDocs](https://squidfunk.github.io/mkdocs-material/)
