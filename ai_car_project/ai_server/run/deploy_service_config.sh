#!/bin/bash
# QuecPi 智能车服务配置部署脚本
# 专门用于复制 service_config 路径下的代码到相关 /etc 目录并启动相关服务
# 用法: ./deploy_service_config.sh [OPTION]

set -e  # 如果任何命令返回非零状态则立即退出

# 获取脚本所在目录，并自动检测 service_config 目录
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# 脚本在 ai_car_service/run/ 目录下，service_config 目录是 ai_car_service
SERVICE_CONFIG_DIR="$(dirname "$SCRIPT_DIR")"

# 如果环境变量已设置，优先使用环境变量
if [ -n "$QUECPI_SERVICE_CONFIG_DIR" ]; then
    SERVICE_CONFIG_DIR="$QUECPI_SERVICE_CONFIG_DIR"
fi

# 颜色定义
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; BLUE='\033[0;34m'; NC='\033[0m'

# 日志函数
log_info() { echo -e "${BLUE}[INFO]${NC} $1"; }
log_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
log_warning() { echo -e "${YELLOW}[WARNING]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

# Check if running as root
check_root() {
    if [ "$EUID" -ne 0 ]; then
        log_error "This script must be run as root (use sudo)"
        exit 1
    fi
    log_success "Running with root privileges"
}

# Get base directory (parent of service_config)
get_base_dir() {
    # Try to find base directory from script location or environment
    if [ -n "$QUECPI_BASE_DIR" ]; then
        echo "$QUECPI_BASE_DIR"
    else
        # Try to find from script location
        local script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
        local parent_dir="$(dirname "$script_dir")"
        local grandparent_dir="$(dirname "$parent_dir")"
        
        # If script is in service_config/run/, then parent_dir is service_config
        # and grandparent_dir is the base directory
        if [ "$(basename "$parent_dir")" = "service_config" ] && [ -d "$parent_dir" ]; then
            echo "$grandparent_dir"
        # Check if parent directory contains service_config
        elif [ -d "$parent_dir/service_config" ]; then
            echo "$parent_dir"
        # Try common installation paths
        elif [ -d "/mnt/car_test/quecpi_smartcar/service_config" ]; then
            echo "/mnt/car_test/quecpi_smartcar"
        elif [ -d "/mnt/quecpi_smartcar/service_config" ]; then
            echo "/mnt/quecpi_smartcar"
        else
            # Default fallback
            echo "/mnt/quecpi_smartcar"
        fi
    fi
}

# Check service_config directory exists
check_service_config() {
    local base_dir=$(get_base_dir)
    local service_config_dir="$base_dir/service_config"
    
    if [ ! -d "$service_config_dir" ]; then
        log_error "Service config directory not found: $service_config_dir"
        log_error "Please ensure the service_config directory exists at the expected location"
        log_error "You can set QUECPI_BASE_DIR environment variable to specify the base directory"
        exit 1
    fi
    
    log_success "Service config directory found: $service_config_dir"
    
    # Detect and log other directory paths
    local parent_dir="$(dirname "$base_dir")"
    log_info "Base directory: $base_dir"
    log_info "Parent directory: $parent_dir"
    
    # Check for ROS2 workspace
    if [ -d "$parent_dir/ros2_ws" ]; then
        log_info "ROS2 workspace found at: $parent_dir/ros2_ws"
    elif [ -d "$base_dir/ros2_ws" ]; then
        log_info "ROS2 workspace found at: $base_dir/ros2_ws"
    elif [ -d "/mnt/ros2_ws" ]; then
        log_info "ROS2 workspace found at: /mnt/ros2_ws"
    fi
    
    # Check for AI_Talker
    if [ -d "$parent_dir/AI_Talker" ]; then
        log_info "AI_Talker found at: $parent_dir/AI_Talker"
    elif [ -d "$base_dir/AI_Talker" ]; then
        log_info "AI_Talker found at: $base_dir/AI_Talker"
    fi
    
    # Check for emotion
    if [ -d "$parent_dir/emotion" ]; then
        log_info "Emotion directory found at: $parent_dir/emotion"
    elif [ -d "$base_dir/emotion" ]; then
        log_info "Emotion directory found at: $base_dir/emotion"
    fi
    
    export QUECPI_BASE_DIR="$base_dir"
    export QUECPI_SERVICE_CONFIG_DIR="$service_config_dir"
}

# Deploy udev rules
deploy_udev_rules() {
    local service_config_dir="${QUECPI_SERVICE_CONFIG_DIR:-/mnt/quecpi_smartcar/service_config}"
    local rules_dir="$service_config_dir/rules.d"
    
    if [ ! -d "$rules_dir" ]; then
        log_warning "udev rules directory not found: $rules_dir"
        return 0
    fi
    
    log_info "Deploying udev rules to /etc/udev/rules.d/..."
    
    # 创建备份目录
    local backup_dir="/etc/udev/rules.d/backup_$(date +%Y%m%d_%H%M%S)"
    mkdir -p "$backup_dir" 2>/dev/null || true
    
    # 部署每个规则文件
    for rule_file in "$rules_dir"/*.rules; do
        if [ -f "$rule_file" ]; then
            local filename=$(basename "$rule_file")
            local target_file="/etc/udev/rules.d/$filename"
            
            # 如果文件已存在则备份
            if [ -f "$target_file" ]; then
                cp "$target_file" "$backup_dir/" 2>/dev/null || true
                log_info "Backed up existing rule: $filename"
            fi
            
            # 复制新的规则文件
            if cp "$rule_file" "$target_file"; then
                log_success "Deployed udev rule: $filename"
            else
                log_error "Failed to deploy udev rule: $filename"
            fi
        fi
    done
    
    log_success "udev rules deployment completed"
}

# 部署 systemd 服务文件
deploy_systemd_services() {
    local service_config_dir="${QUECPI_SERVICE_CONFIG_DIR:-$SERVICE_CONFIG_DIR}"
    local service_dir="$service_config_dir/service"
    
    if [ ! -d "$service_dir" ]; then
        log_warning "systemd service directory not found: $service_dir"
        return 0
    fi
    
    log_info "Deploying systemd service files to /etc/systemd/system/..."
    
    # Get base directory and ROS2 workspace directory for placeholder replacement
    local base_dir="${QUECPI_BASE_DIR:-$(get_base_dir)}"
    local parent_dir="$(dirname "$base_dir")"
    
    # Determine ROS2 workspace directory
    local ros2_ws_dir=""
    if [ -d "$parent_dir/ros2_ws" ]; then
        ros2_ws_dir="$parent_dir/ros2_ws"
    elif [ -d "$base_dir/ros2_ws" ]; then
        ros2_ws_dir="$base_dir/ros2_ws"
    elif [ -d "/mnt/ros2_ws" ]; then
        ros2_ws_dir="/mnt/ros2_ws"
    else
        ros2_ws_dir="/mnt/ros2_ws"  # Default fallback
    fi
    
    # Create backup directory
    local backup_dir="/etc/systemd/system/backup_$(date +%Y%m%d_%H%M%S)"
    mkdir -p "$backup_dir" 2>/dev/null || true
    
    # Deploy each service file
    for service_file in "$service_dir"/*.service; do
        if [ -f "$service_file" ]; then
            local filename=$(basename "$service_file")
            local target_file="/etc/systemd/system/$filename"
            
            # Backup existing file if it exists
            if [ -f "$target_file" ]; then
                cp "$target_file" "$backup_dir/" 2>/dev/null || true
                log_info "Backed up existing service: $filename"
            fi
            
            # Replace placeholders and copy service file
            if sed -e "s|@QUECPI_SERVICE_CONFIG_DIR@|$service_config_dir|g" \
                   -e "s|@QUECPI_BASE_DIR@|$base_dir|g" \
                   -e "s|@QUECPI_ROS2_WS_DIR@|$ros2_ws_dir|g" \
                   "$service_file" > "$target_file"; then
                log_success "Deployed systemd service: $filename"
            else
                log_error "Failed to deploy systemd service: $filename"
            fi
        fi
    done
    
    log_success "systemd service files deployment completed"
}

# 部署运行脚本
deploy_run_scripts() {
    local service_config_dir="${QUECPI_SERVICE_CONFIG_DIR:-$SERVICE_CONFIG_DIR}"
    local run_dir="$service_config_dir/run"
    local target_run_dir="/usr/local/bin/quecpi"
    
    if [ ! -d "$run_dir" ]; then
        log_warning "Run scripts directory not found: $run_dir"
        return 0
    fi
    
    log_info "Deploying run scripts to $target_run_dir..."
    
    # Create target directory
    mkdir -p "$target_run_dir" 2>/dev/null || true
    
    # Create backup directory
    local backup_dir="$target_run_dir/backup_$(date +%Y%m%d_%H%M%S)"
    mkdir -p "$backup_dir" 2>/dev/null || true
    
    # Deploy each script file
    for script_file in "$run_dir"/*; do
        if [ -f "$script_file" ]; then
            local filename=$(basename "$script_file")
            local target_file="$target_run_dir/$filename"
            
            # Backup existing file if it exists
            if [ -f "$target_file" ]; then
                cp "$target_file" "$backup_dir/" 2>/dev/null || true
                log_info "Backed up existing script: $filename"
            fi
            
            # Copy new script file
            if cp "$script_file" "$target_file"; then
                # Make executable
                chmod +x "$target_file" 2>/dev/null || true
                log_success "Deployed run script: $filename"
            else
                log_error "Failed to deploy run script: $filename"
            fi
        fi
    done
    
    log_success "Run scripts deployment completed"
}

# Configure udev rules
setup_udev_rules() {
    log_info "Configuring udev rules for device access..."
    
    # Reload udev rules
    if udevadm control --reload-rules; then
        log_success "udev rules reloaded successfully"
    else
        log_error "Failed to reload udev rules"
        return 1
    fi
    
    # Trigger udev events
    if udevadm trigger; then
        log_success "udev events triggered successfully"
    else
        log_warning "Failed to trigger udev events"
    fi
    
    log_success "udev rules configuration completed"
}

# Install Python dependencies
install_dependencies() {
    log_info "Installing required Python dependencies..."
    
    # Check if dependencies are already installed
    if python3 -c "import gpiod, netifaces, serial" 2>/dev/null; then
        log_success "All Python dependencies are already available"
        return 0
    fi
    
    # Try pip3 first
    if command -v pip3 >/dev/null 2>&1; then
        log_info "Using pip3 to install dependencies..."
        if pip3 install --user --timeout 60 --retries 3 gpiod netifaces pyserial; then
            log_success "Python dependencies installed via pip3"
            return 0
        else
            log_warning "pip3 installation failed, trying system package manager..."
        fi
    fi
    
    # Try system package managers
    if command -v apt >/dev/null 2>&1; then
        log_info "Using apt to install dependencies..."
        apt update && apt install -y python3-gpiod python3-netifaces python3-serial
    elif command -v yum >/dev/null 2>&1; then
        log_info "Using yum to install dependencies..."
        yum install -y python3-gpiod python3-netifaces python3-pyserial
    elif command -v dnf >/dev/null 2>&1; then
        log_info "Using dnf to install dependencies..."
        dnf install -y python3-gpiod python3-netifaces python3-pyserial
    else
        log_error "No package manager found (pip3, apt, yum, dnf)"
        log_error "Please install dependencies manually:"
        log_error "  pip3 install gpiod netifaces pyserial"
        return 1
    fi
    
    log_success "Python dependencies installation completed"
}

# Configure and start systemd services
setup_systemd_services() {
    log_info "Configuring systemd services..."
    
    # Reload systemd daemon to recognize new service files
    systemctl daemon-reload
    
    # List of services to configure
    local services=("button_scan.service" "find_device.service" "remote.service" "wifi.service")
    
    # Enable and start services
    for service in "${services[@]}"; do
        if [ -f "/etc/systemd/system/$service" ]; then
            log_info "Configuring service: $service"
            
            # Stop service if running
            if systemctl is-active --quiet "$service"; then
                log_info "Stopping running service: $service"
                systemctl stop "$service" 2>/dev/null || true
            fi
            
            # Enable service for automatic startup
            if systemctl enable "$service" 2>/dev/null; then
                log_success "Enabled service: $service"
                
                # Start service with retry mechanism
                local max_retries=3
                local retry_count=0
                local service_started=false
                
                while [ $retry_count -lt $max_retries ] && [ "$service_started" = false ]; do
                    if systemctl start "$service" 2>/dev/null; then
                        log_success "Started service: $service"
                        service_started=true
                    else
                        retry_count=$((retry_count + 1))
                        log_warning "Failed to start service: $service (attempt $retry_count/$max_retries)"
                        
                        # Show service status for debugging
                        systemctl status "$service" --no-pager -l || true
                        
                        if [ $retry_count -lt $max_retries ]; then
                            log_info "Retrying service start..."
                            sleep 2
                        fi
                    fi
                done
                
                if [ "$service_started" = false ]; then
                    log_error "Failed to start service $service after $max_retries attempts"
                    log_error "Please check service logs: journalctl -u $service -f"
                fi
            else
                log_warning "Failed to enable service: $service"
            fi
        else
            log_warning "Service file not found: $service"
        fi
    done
    
    log_success "Systemd services configuration completed"
}

# Show service status
show_service_status() {
    log_info "Checking service status..."
    
    local services=("button_scan.service" "find_device.service" "remote.service" "wifi.service")
    
    echo
    echo "=========================================="
    echo "           SERVICE STATUS"
    echo "=========================================="
    
    for service in "${services[@]}"; do
        if systemctl list-unit-files | grep -q "^$service"; then
            local status=$(systemctl is-active "$service" 2>/dev/null || echo "inactive")
            local enabled=$(systemctl is-enabled "$service" 2>/dev/null || echo "disabled")
            
            if [ "$status" = "active" ]; then
                echo -e "✓ $service: ${GREEN}ACTIVE${NC} (${enabled})"
            else
                echo -e "✗ $service: ${RED}INACTIVE${NC} (${enabled})"
            fi
        else
            echo -e "? $service: ${YELLOW}NOT FOUND${NC}"
        fi
    done
    
    echo
    echo "Service Management Commands:"
    echo "  Check status: systemctl status <service_name>"
    echo "  Start service: systemctl start <service_name>"
    echo "  Stop service: systemctl stop <service_name>"
    echo "  Restart service: systemctl restart <service_name>"
    echo "  View logs: journalctl -u <service_name> -f"
    echo
}

# Main function
main() {
    echo "=========================================="
    echo "    QuecPi Service Configuration Deployment"
    echo "=========================================="
    echo "此脚本将复制 service_config 路径下的代码到相关 /etc 目录并启动相关服务"
    echo
    echo "部署步骤:"
    echo "1. 检查权限和目录"
    echo "2. 复制 udev 规则文件"
    echo "3. 复制 systemd 服务文件"
    echo "4. 复制运行脚本"
    echo "5. 安装 Python 依赖"
    echo "6. 配置 udev 规则"
    echo "7. 配置并启动 systemd 服务"
    echo "8. 显示服务状态"
    echo
    echo "开始部署过程..."
    echo
    
    # Execute deployment steps in sequence
    check_root                    # Step 1: Check root privileges
    check_service_config          # Step 2: Check service config directory
    deploy_udev_rules            # Step 3: Deploy udev rules
    deploy_systemd_services      # Step 4: Deploy systemd services
    deploy_run_scripts           # Step 5: Deploy run scripts
    install_dependencies         # Step 6: Install Python dependencies
    setup_udev_rules             # Step 7: Configure udev rules
    setup_systemd_services       # Step 8: Configure and start services
    show_service_status          # Step 9: Show service status
    
    log_success "Service configuration deployment completed successfully!"
}

# Display usage instructions
show_usage() {
    echo "Usage: $0 [OPTION]"
    echo
    echo "Options:"
    echo "  --help, -h                      Show this help message"
    echo "  --status                        Show service status only"
    echo "  --restart                       Restart all services"
    echo "  --stop                          Stop all services"
    echo "  --start                         Start all services"
    echo "  (no arguments)                  Run full deployment"
    echo
    echo "Examples:"
    echo "  $0                              # Run full deployment"
    echo "  $0 --status                     # Show service status"
    echo "  $0 --restart                    # Restart all services"
    echo
}

# Restart all services
restart_services() {
    log_info "Restarting all QuecPi services..."
    
    local services=("button_scan.service" "find_device.service" "remote.service" "wifi.service")
    
    for service in "${services[@]}"; do
        if systemctl list-unit-files | grep -q "^$service"; then
            log_info "Restarting service: $service"
            systemctl restart "$service" 2>/dev/null && log_success "Restarted: $service" || log_error "Failed to restart: $service"
        fi
    done
    
    log_success "Service restart completed"
}

# Stop all services
stop_services() {
    log_info "Stopping all QuecPi services..."
    
    local services=("button_scan.service" "find_device.service" "remote.service" "wifi.service")
    
    for service in "${services[@]}"; do
        if systemctl is-active --quiet "$service"; then
            log_info "Stopping service: $service"
            systemctl stop "$service" 2>/dev/null && log_success "Stopped: $service" || log_error "Failed to stop: $service"
        fi
    done
    
    log_success "Service stop completed"
}

# Start all services
start_services() {
    log_info "Starting all QuecPi services..."
    
    local services=("button_scan.service" "find_device.service" "remote.service" "wifi.service")
    
    for service in "${services[@]}"; do
        if systemctl list-unit-files | grep -q "^$service"; then
            log_info "Starting service: $service"
            systemctl start "$service" 2>/dev/null && log_success "Started: $service" || log_error "Failed to start: $service"
        fi
    done
    
    log_success "Service start completed"
}

# Set up error handling
trap 'log_error "Script execution failed with error code: $?"' ERR

# Check command line arguments
case "${1:-}" in
    --help|-h)
        show_usage
        ;;
    --status)
        check_root
        show_service_status
        ;;
    --restart)
        check_root
        restart_services
        show_service_status
        ;;
    --stop)
        check_root
        stop_services
        ;;
    --start)
        check_root
        start_services
        show_service_status
        ;;
    "")
        # No arguments, run full deployment
        main "$@"
        ;;
    *)
        log_error "Unknown option: $1"
        show_usage
        exit 1
        ;;
esac
