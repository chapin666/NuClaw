#!/bin/bash
# NuClaw 文档一键部署脚本
# 支持: GitHub Pages / Cloudflare Pages / Netlify

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CD="\033[0m"      # 重置
GR="\033[0;32m"   # 绿色
YE="\033[1;33m"   # 黄色
RE="\033[0;31m"   # 红色
BL="\033[0;34m"   # 蓝色

echo -e "${BL}"
echo "╔══════════════════════════════════════════════╗"
echo "║     NuClaw 文档部署工具                      ║"
echo "║     支持 GitHub / Cloudflare / Netlify       ║"
echo "╚══════════════════════════════════════════════╝"
echo -e "${CD}"

# 检查依赖
check_deps() {
    if ! command -v git &> /dev/null; then
        echo -e "${RE}❌ 需要安装 git${CD}"
        exit 1
    fi
    if ! command -v mkdocs &> /dev/null; then
        echo -e "${YE}⚠️  未检测到 mkdocs，尝试安装...${CD}"
        pip install mkdocs-material mkdocs-minify-plugin 2>/dev/null || {
            echo -e "${RE}❌ 安装失败，请手动运行: pip install mkdocs-material${CD}"
            exit 1
        }
    fi
}

# 构建文档
build_docs() {
    echo -e "${YE}🔨 构建文档...${CD}"
    cd "$SCRIPT_DIR"
    mkdocs build --clean
    echo -e "${GR}✅ 构建完成${CD}"
}

# 部署到 GitHub Pages
deploy_github() {
    echo -e "${BL}📦 部署到 GitHub Pages${CD}"
    
    cd "$SCRIPT_DIR"
    
    # 检查是否有 origin
    if ! git remote get-url origin &>/dev/null; then
        echo -e "${YE}⚠️  未配置远程仓库${CD}"
        echo "请输入 GitHub 仓库地址 (如: https://github.com/username/nuclaw.git):"
        read -r repo_url
        git remote add origin "$repo_url"
    fi
    
    # 保存当前分支
    current_branch=$(git branch --show-current 2>/dev/null || echo "main")
    
    echo -e "${YE}🚀 推送到 gh-pages 分支...${CD}"
    
    # 使用 mkdocs 的 gh-deploy
    mkdocs gh-deploy --force
    
    # 获取仓库信息
    repo_url=$(git remote get-url origin)
    # 转换为 GitHub Pages URL
    if [[ "$repo_url" == *"github.com"* ]]; then
        # 提取用户名和仓库名
        if [[ "$repo_url" == git@github.com:* ]]; then
            # SSH 格式
            path=${repo_url#git@github.com:}
        else
            # HTTPS 格式
            path=${repo_url#https://github.com/}
        fi
        path=${path%.git}
        username=$(echo "$path" | cut -d'/' -f1)
        repo=$(echo "$path" | cut -d'/' -f2)
        
        echo ""
        echo -e "${GR}✅ 部署成功！${CD}"
        echo ""
        echo -e "${BL}🌐 访问地址:${CD}"
        echo "   https://${username}.github.io/${repo}"
        echo ""
        echo -e "${YE}⏱️  可能需要 1-2 分钟后才能访问${CD}"
    fi
}

# 部署到 Cloudflare Pages
deploy_cloudflare() {
    echo -e "${BL}📦 部署到 Cloudflare Pages${CD}"
    
    cd "$SCRIPT_DIR"
    
    # 检查是否安装了 wrangler
    if ! command -v npx &> /dev/null; then
        echo -e "${RE}❌ 需要安装 Node.js 和 npm${CD}"
        exit 1
    fi
    
    # 安装 wrangler
    echo -e "${YE}📦 安装 Cloudflare Wrangler...${CD}"
    npm install -g wrangler 2>/dev/null || npx wrangler --version &>/dev/null || {
        echo -e "${YE}将使用 npx 临时运行 wrangler${CD}"
    }
    
    echo -e "${YE}🚀 部署到 Cloudflare Pages...${CD}"
    
    # 部署
    if command -v wrangler &> /dev/null; then
        wrangler pages deploy site --project-name=nuclaw-docs
    else
        npx wrangler pages deploy site --project-name=nuclaw-docs
    fi
    
    echo -e "${GR}✅ 部署完成！${CD}"
}

# 部署到 Netlify
deploy_netlify() {
    echo -e "${BL}📦 部署到 Netlify${CD}"
    
    cd "$SCRIPT_DIR"
    
    # 检查 netlify-cli
    if ! command -v netlify &> /dev/null; then
        echo -e "${YE}📦 安装 Netlify CLI...${CD}"
        npm install -g netlify-cli
    fi
    
    echo -e "${YE}🚀 部署到 Netlify...${CD}"
    netlify deploy --prod --dir=site
}

# 生成静态文件供手动上传
manual_deploy() {
    echo -e "${BL}📦 生成静态文件${CD}"
    
    cd "$SCRIPT_DIR"
    
    # 打包 site 目录
    tar -czf nuclaw-docs.tar.gz -C site .
    
    echo -e "${GR}✅ 已生成 nuclaw-docs.tar.gz${CD}"
    echo ""
    echo -e "${YE}你可以:${CD}"
    echo "  1. 上传到任意静态托管服务"
    echo "  2. 用 Python 临时预览: cd site && python3 -m http.server 8000"
    echo ""
    echo -e "${BL}文件位置: ${SCRIPT_DIR}/nuclaw-docs.tar.gz${CD}"
}

# 主菜单
main_menu() {
    echo ""
    echo "选择部署方式:"
    echo ""
    echo "  1) GitHub Pages (推荐 - 完全免费，无限制)"
    echo "  2) Cloudflare Pages (全球 CDN，国内快)"
    echo "  3) Netlify (拖拽部署)"
    echo "  4) 仅生成静态文件 (手动上传)"
    echo ""
    echo -n "请输入选项 [1-4]: "
    read -r choice
    
    case $choice in
        1)
            check_deps
            build_docs
            deploy_github
            ;;
        2)
            check_deps
            build_docs
            deploy_cloudflare
            ;;
        3)
            check_deps
            build_docs
            deploy_netlify
            ;;
        4)
            check_deps
            build_docs
            manual_deploy
            ;;
        *)
            echo -e "${RE}❌ 无效选项${CD}"
            exit 1
            ;;
    esac
}

# 如果有参数直接执行
if [ "$1" = "github" ]; then
    check_deps
    build_docs
    deploy_github
elif [ "$1" = "cloudflare" ]; then
    check_deps
    build_docs
    deploy_cloudflare
elif [ "$1" = "netlify" ]; then
    check_deps
    build_docs
    deploy_netlify
elif [ "$1" = "manual" ]; then
    check_deps
    build_docs
    manual_deploy
else
    main_menu
fi
