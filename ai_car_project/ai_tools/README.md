# Quec AI Car Service Bundle

本仓库用于辅助部署与管理 Quectel 智能车（AI Robot）运行环境，聚合了批量下发脚本、systemd 服务单元与 udev 规则。

## 特性

- 自动路径检测：脚本会自动检测基础目录和相关资源目录
- 相对路径支持：支持将服务部署到设备的任意路径
- 环境变量配置：支持通过 `QUECPI_BASE_DIR` 环境变量自定义基础路径

## 前提条件

**重要**：在使用之前，必须确保设备已烧录支持 ROS2 构建的镜像(SG565DWFPARL1A01_QDP_LP6.6_QL1.3_R2H_V01)。设备需要预装以下组件：

- ROS2 运行环境
- colcon 构建工具
- 相关的系统依赖库

如果设备未烧录支持 ROS2 的镜像，请先完成镜像烧录后再进行部署。

## 目录结构

### 设备端目录结构

```
/mnt/car_test/                    # 或任意父目录
├── quecpi_smartcar/              # BASE_DIR（基础目录）
│   └── service_config/
│       ├── run/                  # 运行脚本
│       ├── service/              # systemd 服务文件
│       └── rules.d/              # udev 规则
├── AI_Talker/                    # 与 quecpi_smartcar 同级
├── emotion/                      # 与 quecpi_smartcar 同级
├── ros2_ws/                     # 与 quecpi_smartcar 同级
└── software/                     # 与 quecpi_smartcar 同级
```

脚本会自动检测目录结构，优先使用与 `quecpi_smartcar` 同级的目录。

### 部署到设备

使用 `package.sh` 打包后，将 `Ai_Car_Output` 目录内容通过 adb push 到设备：

```bash
# 1. 打包构建产物
./package.sh 或 ./package.sh pack      #只打包不编译
./package.sh build                     #编译最新版本并打包
./package.sh clean                     #清理输出目录

# 2. 推送整个目录到设备
adb push Ai_Car_Output /mnt/car_test/

# 或分步推送各个目录
adb push Ai_Car_Output/AI_Talker /mnt/car_test/
adb push Ai_Car_Output/emotion /mnt/car_test/
adb push Ai_Car_Output/quecpi_smartcar /mnt/car_test/
adb push Ai_Car_Output/ros2_ws /mnt/car_test/
adb push Ai_Car_Output/software /mnt/car_test/
```

推送完成后，设备上的目录结构将与上述设备端目录结构一致。

### 编译 ROS2 工作空间

在设备上部署完成后，需要编译 ROS2 工作空间：

```bash
# 1. 进入设备（通过 adb shell 或 SSH）
adb shell

# 2. 进入 ROS2 工作空间目录
cd /mnt/car_test/ros2_ws

# 3. 编译 ROS2 工作空间
colcon build

# 4. 如果编译成功，source 工作空间
source install/setup.bash
```

**注意**：
- 确保设备上已安装 ROS2 和 colcon 构建工具
- 编译过程可能需要一些时间，请耐心等待
- 如果编译失败，请检查错误日志并解决依赖问题
- 编译完成后，`start_car.sh` 脚本会自动 source 工作空间

### 修复 Python 依赖环境

在部署完成后，如果遇到 Python 依赖问题，可以在网络连接良好的前提下使用 `fix_python_dependencies.sh` 脚本自动修复：

```bash
# 1. 进入设备（通过 adb shell 或 SSH）
adb shell

# 2. 进入服务配置目录
cd /mnt/car_test/quecpi_smartcar/service_config/run

# 3. 运行修复脚本
chmod +x fix_python_dependencies.sh
./fix_python_dependencies.sh
```

#### fix_python_dependencies.sh 功能说明

该脚本会自动完成以下修复工作：

1. **安装 Python 依赖包**：自动安装 `pandas`、`smbus2`、`mediapipe`、`transforms3d`、`pyzbar` 等必需的 Python 包
2. **修复 OpenGL 兼容性**：提供 OpenGL 兼容性问题的修复指导
3. **安装兼容的 OpenCV 版本**：卸载冲突的 `opencv-contrib-python`，安装兼容版本 `opencv-python-headless==4.5.5.64`
4. **降级 NumPy 版本**：安装 NumPy < 2.0 版本以确保兼容性
5. **修复 cv2 类型问题**：修复 `cv2/typing/__init__.py` 中的 `DictValue` 类型定义问题
6. **验证安装结果**：自动验证所有依赖包的安装情况

#### 使用选项

```bash
# 运行完整的修复流程（默认）
./fix_python_dependencies.sh

# 仅验证已安装的依赖
./fix_python_dependencies.sh --verify

# 查看帮助信息
./fix_python_dependencies.sh --help
```

#### 注意事项

- 脚本需要 `pip3` 工具，确保设备已安装 Python3 和 pip3
- 某些修复步骤（如 OpenGL 兼容性修复）可能需要系统级权限，脚本会提供相应的手动修复指导
- 如果遇到权限问题，可能需要先执行 `mount -o remount,rw /usr` 来挂载文件系统为可写模式
- 建议在首次部署后运行此脚本，确保 Python 环境配置正确

## 快速使用

### start_car.sh 和 start_car.service

`start_car.sh` 是整车启动脚本，`start_car.service` 是 systemd 服务单元文件，两者配合实现一键拉起整车服务。

#### 方式一：通过 systemd 服务启动（推荐）

1. **部署服务**：
   ```bash
   chmod +x run/deploy_service_config.sh
   ./run/deploy_service_config.sh
   ```
   脚本会自动部署 `start_car.service` 到 `/etc/systemd/system/` 并启动服务。

2. **服务管理**：
   ```bash
   # 查看服务状态
   systemctl status start_car.service
   
   # 查看日志
   journalctl -u start_car.service -f
   
   # 启动服务
   systemctl start start_car.service
   
   # 停止服务
   systemctl stop start_car.service
   
   # 重启服务
   systemctl restart start_car.service
   
   # 开机自启
   systemctl enable start_car.service
   ```

#### 方式二：直接运行脚本

```bash
# 直接运行 start_car.sh
./run/start_car.sh

# 或使用完整路径
/mnt/car_test/quecpi_smartcar/service_config/run/start_car.sh
```

#### start_car.sh 功能说明

`start_car.sh` 会自动完成以下工作：
1. WiFi 连接检测（等待最多 60 秒）
2. 硬件初始化（摄像头检测、音频配置、GPIO 设置）
3. ROS2 工作空间启动
4. AI 语音服务启动（ai_car_demo、websocket_audio_demo）
5. 服务监控和自动重启

所有日志保存在 `service_config/run/` 目录下。

## 路径配置

### 环境变量

- `QUECPI_BASE_DIR`：基础目录路径（默认自动检测）
- `QUECPI_SERVICE_CONFIG_DIR`：服务配置目录路径
- `QUECPI_ROS2_WS_DIR`：ROS2 工作空间路径（自动检测）
- `QUECPI_AI_TALKER_DIR`：AI_Talker 目录路径（自动检测）
- `QUECPI_EMOTION_DIR`：Emotion 目录路径（自动检测）

### 路径检测优先级

1. 环境变量 `QUECPI_BASE_DIR`
2. 从脚本位置自动推断
3. 检查 `/mnt/car_test/quecpi_smartcar` 和 `/mnt/quecpi_smartcar`
4. 默认回退到 `/mnt/quecpi_smartcar`

## 日志位置

所有运行日志位于服务配置目录的 `run/` 子目录下：
- `start_car.log`：主服务启动日志
- `ros2.log`：ROS2 启动日志
- `ai_car_demo.log`：IoT 服务日志
- `websocket_audio_demo.log`：WebSocket 音频服务日志

## 主要脚本说明

- `run/start_car.sh`：整车启动脚本，完成 WiFi 检测、硬件初始化、ROS2 bringup、AI语音服务等
- `run/start_node.sh`：只启动 ROS2 节点的轻量入口
- `run/fix_python_dependencies.sh`：Python 依赖修复脚本，自动安装和修复 Python 环境依赖问题（包括 OpenCV、NumPy、MediaPipe 等）
- `run/remote.py`：9026 端口 TCP 服务，支持远程 WiFi 配网
- `run/button_scan.py`：按键扫描服务
- `run/find_device.py`：设备检测服务
