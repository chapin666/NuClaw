# NuClaw 在线文档部署指南

## 一键部署

```bash
cd NuClaw
./deploy.sh
```

然后根据提示选择平台即可。

---

## 方案对比

| 平台 | 优点 | 缺点 | 适合场景 |
|:---|:---|:---|:---|
| **GitHub Pages** | 完全免费，无流量限制，与代码仓库集成 | 国内访问稍慢 | 技术文档首选 |
| **Cloudflare Pages** | 全球 CDN，国内访问快 | 需要 Cloudflare 账号 | 有国内读者的文档 |
| **Netlify** | 拖拽部署，预览链接 | 有流量限制 | 快速预览 |

---

## GitHub Pages 部署（推荐）

### 快速部署

```bash
# 方式1：交互式
./deploy.sh
# 然后选择 1

# 方式2：直接命令
./deploy.sh github
```

### 首次部署

如果是第一次部署到 GitHub Pages，需要先确保：

1. 代码已推送到 GitHub
2. 仓库是公开的（免费）

```bash
# 进入 NuClaw 目录
cd NuClaw

# 如果没有推送到 GitHub，先推送
git remote add origin https://github.com/你的用户名/NuClaw.git
git push -u origin main

# 然后部署
./deploy.sh github
```

### 部署后访问

部署成功后，访问地址格式：

```
https://你的用户名.github.io/NuClaw
```

---

## Cloudflare Pages 部署

```bash
./deploy.sh cloudflare
```

首次使用需要登录：
```bash
npx wrangler login
```

---

## Netlify 部署

```bash
./deploy.sh netlify
```

或者手动拖拽 `site/` 文件夹到 https://app.netlify.com/drop

---

## 本地预览

部署前可以先本地预览：

```bash
cd NuClaw
mkdocs serve
# 访问 http://127.0.0.1:8000
```

---

## 自动部署（GitHub Actions）

如果你想每次推送代码时自动部署，可以添加 GitHub Actions：

创建 `.github/workflows/deploy.yml`：

```yaml
name: Deploy Docs

on:
  push:
    branches: [ main ]

permissions:
  contents: write

jobs:
  deploy:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      
      - name: Setup Python
        uses: actions/setup-python@v5
        with:
          python-version: '3.x'
      
      - name: Install dependencies
        run: |
          pip install mkdocs-material mkdocs-minify-plugin
      
      - name: Deploy
        run: mkdocs gh-deploy --force
```

这样每次 `git push` 都会自动更新文档。
