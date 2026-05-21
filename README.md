# Smart AI Robot Car

<!-- [![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT) -->

智能AI机器人小车项目，基于ROS2框架开发，支持语音交互、目标追踪、人脸识别等功能。

## 项目简介

Smart AI Robot Car 是一个面向智能硬件的AI机器人解决方案，提供了完整的机器人软件栈，包括：

- **语音交互**: 支持KWS语音唤醒、ASR语音识别
- **视觉识别**: 智能避障、人体追踪、物体识别
- **运动控制**: 基于ROS2的机器人底盘控制
- **模块化架构**: 插件化的模块设计，支持灵活扩展

## 功能特性

- 🎙️ 语音唤醒 (KWS)
- 🗣️ 语音识别 (ASR)
- 🤖 大语言模型集成 (LLM)
- 👀 人脸/人体追踪
- 🔊 音频播放与处理
- 📡 WebSocket 实时通信
- 🔧 完善的日志系统
- 📦 模块化服务架构

## 技术栈

- **操作系统**: Linux (ARM)
- **机器人框架**: ROS2 Humble
- **编程语言**: C, Python
- **AI/ML**: MNN, MediaPipe
- **通信协议**: WebSocket, MQTT

## 目录结构

```
ai_car_project/
├── ai_audio/          # 音频模块
├── ai_common/         # 公共基础模块
├── ai_kws/            # 语音唤醒模块
├── ai_large_model/    # 大语言模型模块
├── ai_local_asr/      # 本地语音识别
├── ai_qth/            # 通信模块
├── ai_robot_face/     # 机器人表情
├── ai_ros2/           # ROS2相关
├── ai_server/         # 服务配置
├── ai_tools/          # 工具脚本
├── ai_tracker_body/   # 目标追踪
└── ai_car_output/     # 构建输出
```

## 快速开始

### 环境要求

- ARM Linux 系统 (已烧录支持ROS2的镜像)
- ROS2 Humble
- Python 3.10+
- GCC/CMake 构建工具

## 模块说明

### ai_common
公共基础模块，提供服务定位器、消息队列、共享内存等基础功能。

### ai_audio
音频模块，负责音频采集、播放、语音前处理等。

### ai_kws
关键词唤醒模块，支持语音唤醒功能。

### ai_large_model
大语言模型集成模块，支持Coze API等云端LLM服务。

### ai_tracker_body
目标追踪模块，支持人脸、人体、人手等多种目标的追踪。

## 许可证

本项目基于 [MIT 许可证](LICENSE) 开源。

## 贡献指南

欢迎提交 Issue 和 Pull Request！

## 联系方式

如有问题，请提交 Issue 或联系项目维护者。

## 开发板底包

小车开发板底包下载地址：
https://developer.quectel.com/doc/files/Expr_Sense_Bot_firmware.zip