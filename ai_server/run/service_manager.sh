#!/bin/bash

# 服务管理脚本 - 自动化systemctl操作
# 使用方法: ./service_manager.sh [enable|start|status|restart|disable|stop]

# 定义服务列表
SERVICES=("wifi" "find_device" "button_scan" "remote")

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 打印带颜色的消息
print_status() {
    local color=$1
    local message=$2
    echo -e "${color}${message}${NC}"
}

# 检查是否为root用户
check_root() {
    if [ "$EUID" -ne 0 ]; then
        print_status $RED "❌ 此脚本需要root权限运行"
        print_status $YELLOW "请使用: sudo $0 $@"
        exit 1
    fi
}

# 启用服务开机自启
enable_services() {
    print_status $BLUE "🔧 启用服务开机自启..."
    for service in "${SERVICES[@]}"; do
        print_status $YELLOW "正在启用 $service.service..."
        if systemctl enable "$service.service" >/dev/null 2>&1; then
            print_status $GREEN "✅ $service.service 启用成功"
        else
            print_status $RED "❌ $service.service 启用失败"
        fi
    done
    print_status $GREEN "🎉 所有服务开机自启配置完成"
}

# 启动服务
start_services() {
    print_status $BLUE "🚀 启动服务..."
    for service in "${SERVICES[@]}"; do
        print_status $YELLOW "正在启动 $service.service..."
        if systemctl start "$service.service" >/dev/null 2>&1; then
            print_status $GREEN "✅ $service.service 启动成功"
        else
            print_status $RED "❌ $service.service 启动失败"
        fi
    done
    print_status $GREEN "🎉 所有服务启动完成"
}

# 查看服务状态
show_status() {
    print_status $BLUE "📊 查看服务状态..."
    echo ""
    for service in "${SERVICES[@]}"; do
        print_status $YELLOW "=== $service.service 状态 ==="
        systemctl status "$service.service" --no-pager -l
        echo ""
    done
}

# 重启服务
restart_services() {
    print_status $BLUE "🔄 重启服务..."
    for service in "${SERVICES[@]}"; do
        print_status $YELLOW "正在重启 $service.service..."
        if systemctl restart "$service.service" >/dev/null 2>&1; then
            print_status $GREEN "✅ $service.service 重启成功"
        else
            print_status $RED "❌ $service.service 重启失败"
        fi
    done
    print_status $GREEN "🎉 所有服务重启完成"
}

# 禁用服务开机自启
disable_services() {
    print_status $BLUE "🔧 禁用服务开机自启..."
    for service in "${SERVICES[@]}"; do
        print_status $YELLOW "正在禁用 $service.service..."
        if systemctl disable "$service.service" >/dev/null 2>&1; then
            print_status $GREEN "✅ $service.service 禁用成功"
        else
            print_status $RED "❌ $service.service 禁用失败"
        fi
    done
    print_status $GREEN "🎉 所有服务开机自启已禁用"
}

# 停止服务
stop_services() {
    print_status $BLUE "🛑 停止服务..."
    for service in "${SERVICES[@]}"; do
        print_status $YELLOW "正在停止 $service.service..."
        if systemctl stop "$service.service" >/dev/null 2>&1; then
            print_status $GREEN "✅ $service.service 停止成功"
        else
            print_status $RED "❌ $service.service 停止失败"
        fi
    done
    print_status $GREEN "🎉 所有服务已停止"
}

# 一键设置（启用并启动所有服务）
setup_all() {
    print_status $BLUE "🚀 一键设置：启用并启动所有服务..."
    enable_services
    echo ""
    start_services
    echo ""
    print_status $GREEN "🎉 一键设置完成！"
}

# 显示帮助信息
show_help() {
    echo "服务管理脚本 - 自动化systemctl操作"
    echo ""
    echo "使用方法: $0 [命令]"
    echo ""
    echo "可用命令:"
    echo "  enable    - 启用所有服务开机自启"
    echo "  start     - 启动所有服务"
    echo "  status    - 查看所有服务状态"
    echo "  restart   - 重启所有服务"
    echo "  disable   - 禁用所有服务开机自启"
    echo "  stop      - 停止所有服务"
    echo "  setup     - 一键设置（启用并启动所有服务）"
    echo "  help      - 显示此帮助信息"
    echo ""
    echo "示例:"
    echo "  sudo $0 setup     # 一键设置所有服务"
    echo "  sudo $0 status    # 查看服务状态"
    echo "  sudo $0 restart   # 重启所有服务"
    echo ""
    echo "管理的服务: ${SERVICES[*]}"
}

# 主函数
main() {
    case "${1:-help}" in
        "enable")
            check_root
            enable_services
            ;;
        "start")
            check_root
            start_services
            ;;
        "status")
            show_status
            ;;
        "restart")
            check_root
            restart_services
            ;;
        "disable")
            check_root
            disable_services
            ;;
        "stop")
            check_root
            stop_services
            ;;
        "setup")
            check_root
            setup_all
            ;;
        "help"|"-h"|"--help")
            show_help
            ;;
        *)
            print_status $RED "❌ 未知命令: $1"
            echo ""
            show_help
            exit 1
            ;;
    esac
}

# 运行主函数
main "$@"
