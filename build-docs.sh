#!/bin/bash
# NuClaw 文档构建脚本

set -e

echo "🚀 NuClaw 文档构建工具"
echo "======================"

# 检查 mkdocs
if ! command -v mkdocs &. /dev/null; then
    echo "❌ 未找到 mkdocs，请先安装:"
    echo "   pip install mkdocs-material mkdocs-minify-plugin"
    exit 1
fi

# 构建文档
echo "🔨 构建文档..."
mkdocs build

echo ""
echo "✅ 构建完成！输出目录: site/"
echo ""
echo "本地预览: mkdocs serve"
echo "部署到 GitHub Pages: mkdocs gh-deploy"
