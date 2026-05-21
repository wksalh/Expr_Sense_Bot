#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# 多功能目标跟踪系统 - 人体跟随优化版（车体跟随完整版）
#
# ============================================================================
# 修改说明 (2026-04-28)
# 1. 图像输入：移除 ROS2 图像订阅，改为直接从 /dev/video4 (设备索引4) 获取相机数据。
# 2. AI 检测器：替换 api_tracker_body 模块为 tracker_binding，使用新的 init/set_param API。
# 3. 模型路径与授权：硬编码授权码及模型文件路径，取消对 config.json/model.json 的依赖。
# 4. 主循环：改用 ROS2 定时器（30Hz）驱动图像处理，不再依赖 camera_topic。
# 5. 其余控制逻辑（云台PID、车体控制、平滑跟踪、日志记录）完全保留不变。
# ============================================================================

import os
import sys
import cv2
import math
import rclpy
import threading
import numpy as np
import time
import queue
import json
from collections import deque
from dataclasses import dataclass
from typing import Optional, Tuple, List, Dict, Any
from enum import IntEnum
import traceback
import tempfile
from datetime import datetime

# ============================================================================
# 环境设置和模块导入
# ============================================================================

# 设置环境
os.environ['PYTHONIOENCODING'] = 'utf-8'
sys.stdout.reconfigure(encoding='utf-8') if hasattr(sys.stdout, 'reconfigure') else None

# MODIFIED: 修改 tracker 模块路径，添加 build/ 目录，以便导入 tracker_binding
tracker_full_path = "/mnt/car_test/object_tracker"
sys.path.insert(0, tracker_full_path)
sys.path.insert(0, os.path.join(tracker_full_path, 'build'))
os.environ['LD_LIBRARY_PATH'] = f"{tracker_full_path}/lib:{os.environ.get('LD_LIBRARY_PATH', '')}"

# ROS2相关模块
from rclpy.node import Node
from cv_bridge import CvBridge
from sensor_msgs.msg import Image
from geometry_msgs.msg import Twist
from std_srvs.srv import Trigger, SetBool
from interfaces.srv import SetPoint, SetFloat64
from large_models_msgs.srv import SetString
from ros_robot_controller_msgs.msg import SetPWMServoState, PWMServoState, RGBState, RGBStates

# 导入现有模块
import sdk.pid as pid
import sdk.common as common
import sdk.yaml_handle as yaml_handle
from app.common import Heart, ColorPicker

# MODIFIED: 导入新的 tracker_binding 模块 (替代 api_tracker_body)
try:
    import tracker_binding
    TRACKER_AVAILABLE = True
    print("tracker_binding module imported successfully")
except ImportError as e:
    TRACKER_AVAILABLE = False
    print(f"Failed to import tracker_binding module: {e}")

from rclpy.callback_groups import ReentrantCallbackGroup

# MODIFIED: 定义全局授权码和许可证路径（请替换为真实值）
AUTH_CODE = "D7F81710-BC7C-4B9C-A213-9B9BA14166A7"
LICENSE_FILE = os.path.join(tracker_full_path, "license.lic")

# ============================================================================
# 1. 核心数据结构
# ============================================================================

class TargetState(IntEnum):
    """目标跟踪状态"""
    LOST = 0          # 完全丢失
    DETECTED = 1      # 当前帧检测到
    TRACKING = 2      # 稳定跟踪中
    PREDICTING = 3    # 预测状态

@dataclass
class DetectionResult:
    """检测结果标准化"""
    x: int              # 中心X坐标
    y: int              # 中心Y坐标
    width: int          # 宽度
    height: int         # 高度
    class_name: str     # 类别名称
    
    @property
    def area(self) -> int:
        return self.width * self.height

# ============================================================================
# 2. 车体跟随专用日志记录器
# ============================================================================

class ChassisFollowingLogger:
    """车体跟随专用日志记录器 - 保存到/mnt目录"""
    
    def __init__(self, log_dir="/mnt/body_following_logs", max_logs=500):
        self.log_dir = log_dir
        self.max_logs = max_logs
        
        # 创建日志目录
        if not os.path.exists(log_dir):
            os.makedirs(log_dir)
        
        # 日志文件管理
        self.log_files = deque(maxlen=max_logs)
        self.frame_count = 0
        self.log_interval = 30  # 每30帧记录一次
        
        # 创建CSV日志文件
        self.csv_file = os.path.join(log_dir, "chassis_following_log.csv")
        self._init_csv_file()
    
    def _init_csv_file(self):
        """初始化CSV日志文件"""
        if not os.path.exists(self.csv_file):
            with open(self.csv_file, 'w', encoding='utf-8') as f:
                f.write("timestamp,frame_count,target_width,image_width,width_ratio,target_x,center_x,error_x,linear_x,angular_z,state\n")
    
    def log_chassis_data(self, timestamp, frame_count, target_width, image_width, 
                         target_x, center_x, error_x, linear_x, angular_z, state):
        """记录车体跟随数据到CSV"""
        try:
            width_ratio = target_width / image_width if image_width > 0 else 0
            
            with open(self.csv_file, 'a', encoding='utf-8') as f:
                f.write(f"{timestamp},{frame_count},{target_width},{image_width},{width_ratio:.3f},"
                       f"{target_x},{center_x},{error_x},{linear_x:.3f},{angular_z:.3f},{state}\n")
        except Exception as e:
            print(f"Failed to write chassis log: {e}")
    
    def save_chassis_debug_image(self, image, detection_result, linear_x, angular_z, 
                               width_ratio, state_name, timestamp=None):
        """保存车体跟随调试图像"""
        if timestamp is None:
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S_%f")
        
        self.frame_count += 1
        
        # 每log_interval帧保存一次
        if self.frame_count % self.log_interval != 0:
            return None
        
        try:
            # 创建副本
            log_image = image.copy()
            h, w = log_image.shape[:2]
            
            # 绘制检测结果
            if detection_result:
                # 绘制检测框
                x1 = detection_result.x - detection_result.width // 2
                y1 = detection_result.y - detection_result.height // 2
                x2 = x1 + detection_result.width
                y2 = y1 + detection_result.height
                
                color = (0, 255, 0) if state_name == "TRACKING" else (255, 0, 0)
                cv2.rectangle(log_image, (x1, y1), (x2, y2), color, 2)
                cv2.circle(log_image, (detection_result.x, detection_result.y), 5, (0, 0, 255), -1)
            
            # 绘制中心线
            cv2.line(log_image, (w//2, 0), (w//2, h), (255, 0, 0), 1)  # 垂直中线
            cv2.line(log_image, (0, h//2), (w, h//2), (255, 0, 0), 1)  # 水平中线
            cv2.circle(log_image, (w//2, h//2), 5, (255, 0, 0), -1)
            
            # 绘制控制信息
            info_y = 30
            cv2.putText(log_image, f"Chassis Following Mode", (10, info_y), 
                       cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 255, 255), 2)
            info_y += 30
            
            cv2.putText(log_image, f"State: {state_name}", (10, info_y), 
                       cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 2)
            info_y += 25
            
            cv2.putText(log_image, f"Target Width: {detection_result.width if detection_result else 0}", 
                       (10, info_y), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 2)
            info_y += 25
            
            cv2.putText(log_image, f"Width Ratio: {width_ratio:.2f}", (10, info_y), 
                       cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 2)
            info_y += 25
            
            cv2.putText(log_image, f"Control: ({linear_x:.2f}, {angular_z:.2f})", (10, info_y), 
                       cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 2)
            
            # 绘制距离指示条
            bar_width = 200
            bar_height = 20
            bar_x = 10
            bar_y = h - 40
            
            # 背景条
            cv2.rectangle(log_image, (bar_x, bar_y), (bar_x + bar_width, bar_y + bar_height), 
                         (100, 100, 100), -1)
            
            # 当前比例位置
            current_pos = int(width_ratio * bar_width)
            current_pos = min(max(current_pos, 0), bar_width)
            
            # 目标比例位置（40%）
            target_pos = int(0.40 * bar_width)
            
            # 绘制当前比例
            cv2.rectangle(log_image, (bar_x, bar_y), (bar_x + current_pos, bar_y + bar_height), 
                         (0, 255, 0) if width_ratio < 0.40 else (0, 0, 255), -1)
            
            # 绘制目标线
            cv2.line(log_image, (bar_x + target_pos, bar_y - 5), 
                    (bar_x + target_pos, bar_y + bar_height + 5), (255, 255, 0), 2)
            
            # 绘制标签
            cv2.putText(log_image, f"{width_ratio:.1%}", (bar_x + bar_width + 10, bar_y + 15), 
                       cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 255), 1)
            cv2.putText(log_image, "40%", (bar_x + target_pos - 10, bar_y - 10), 
                       cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 0), 1)
            
            # 保存图像
            filename = f"{self.log_dir}/chassis_{timestamp}.jpg"
            cv2.imwrite(filename, log_image)
            
            # 添加到日志文件列表
            self.log_files.append(filename)
            
            # 清理旧日志文件
            if len(self.log_files) > self.max_logs:
                old_file = self.log_files.popleft()
                if os.path.exists(old_file):
                    os.remove(old_file)
            
            return filename
            
        except Exception as e:
            print(f"Failed to save chassis debug image: {e}")
            return None

# ============================================================================
# 3. 智能平滑目标跟踪器（人体跟踪优化）
# ============================================================================

class SmartSmoothTargetTracker:
    """智能平滑目标跟踪器 - 人体跟踪优化版"""
    
    def __init__(self, config: Dict[str, Any]):
        self.config = config
        
        # 滑动窗口配置
        self.window_size = config.get('window_size', 10)
        self.max_predict_frames = config.get('max_predict_frames', 4)
        self.lost_threshold = config.get('lost_threshold', 5)
        
        # 历史数据队列
        self.position_history = deque(maxlen=self.window_size)
        self.size_history = deque(maxlen=self.window_size)
        self.timestamp_history = deque(maxlen=self.window_size)
        
        # 跟踪状态
        self.current_state = TargetState.LOST
        self.consecutive_misses = 0
        self.consecutive_hits = 0
        self.last_detection_time = 0
        
        # 丢失计数器
        self.real_lost_count = 0
        self.max_real_lost_frames = 10
        
        # 速度估计
        self.velocity_x = 0
        self.velocity_y = 0
        self.last_position = None
        self.last_time = None
        
        # 平滑输出
        self.smoothed_x = 0
        self.smoothed_y = 0
        self.smoothed_width = 0
        
        # 人体跟踪特有参数
        self.human_velocity_history = deque(maxlen=10)
    
    def update(self, detection: Optional[DetectionResult]) -> Dict[str, Any]:
        """更新跟踪器"""
        current_time = time.time()
        
        # 记录检测状态
        if detection is not None:
            # 打印检测到的目标信息
            print(f"[Tracker] 检测到目标: 位置({detection.x}, {detection.y}), 大小{detection.width}x{detection.height}, 类别{detection.class_name}")
            
            # 如果是LOST状态检测到目标，特别记录
            if self.current_state == TargetState.LOST:
                print(f"[Tracker] 重要：从LOST状态检测到目标！开始跟踪")
            
            self._handle_detection(detection, current_time)
        else:
            # 打印未检测到目标的信息
            if self.consecutive_misses % 10 == 0:  # 每10次未检测记录一次
                print(f"[Tracker] 未检测到目标，连续未检测次数: {self.consecutive_misses}, 当前状态: {self.current_state.name}")
            
            self._handle_miss(current_time)
        
        # 计算平滑输出
        self._compute_smoothed_output(current_time)
        
        # 获取状态并记录重要状态变化
        new_status = self.get_status()
        
        # 记录状态变化
        if self.current_state != new_status['state']:
            old_state_name = self.current_state.name
            new_state_name = new_status['state_name']
            print(f"[Tracker] 状态变化: {old_state_name} -> {new_state_name}")
            
            # 如果从LOST变为其他状态，特别记录
            if self.current_state == TargetState.LOST:
                print(f"[Tracker] 重要：从LOST状态恢复！新状态: {new_state_name}")
        
        return new_status
    
    def _handle_detection(self, detection: DetectionResult, timestamp: float):
        """处理新的检测"""
        self.consecutive_misses = 0
        self.consecutive_hits += 1
        self.real_lost_count = 0
        
        # 添加到历史
        self.position_history.append((detection.x, detection.y))
        self.size_history.append(detection.width)
        self.timestamp_history.append(timestamp)
        
        # 计算速度
        if self.last_position and self.last_time:
            dt = timestamp - self.last_time
            if dt > 0:
                self.velocity_x = (detection.x - self.last_position[0]) / dt
                self.velocity_y = (detection.y - self.last_position[1]) / dt
                self.human_velocity_history.append(math.sqrt(self.velocity_x**2 + self.velocity_y**2))
        
        # 更新状态
        if self.current_state == TargetState.LOST:
            self.current_state = TargetState.DETECTED
        elif self.consecutive_hits >= 3:
            self.current_state = TargetState.TRACKING
        
        self.last_position = (detection.x, detection.y)
        self.last_time = timestamp
        self.last_detection_time = timestamp
    
    def _handle_miss(self, timestamp: float):
        """处理检测丢失"""
        self.consecutive_misses += 1
        self.consecutive_hits = 0
        
        # 短期预测
        if self.consecutive_misses <= self.max_predict_frames and len(self.position_history) > 0:
            self.current_state = TargetState.PREDICTING
            
            # 基于速度预测
            if self.velocity_x != 0 or self.velocity_y != 0:
                dt = timestamp - self.last_time
                last_x, last_y = self.position_history[-1]
                pred_x = last_x + self.velocity_x * dt
                pred_y = last_y + self.velocity_y * dt
                self.position_history.append((int(pred_x), int(pred_y)))
        else:
            # 长期丢失
            self.real_lost_count += 1
            if self.real_lost_count >= self.max_real_lost_frames:
                self.reset()
            else:
                self.current_state = TargetState.LOST
    
    def _compute_smoothed_output(self, current_time: float):
        """计算平滑输出"""
        if self.current_state == TargetState.LOST:
            self.smoothed_x = 0
            self.smoothed_y = 0
            self.smoothed_width = 0
            return
        
        if not self.position_history:
            return
        
        # 指数加权移动平均
        positions = list(self.position_history)
        sizes = list(self.size_history)
        
        n = len(positions)
        
        if n == 1:
            self.smoothed_x, self.smoothed_y = positions[0]
            self.smoothed_width = sizes[0]
        else:
            # 使用更快的衰减
            decay = 0.6
            weights = np.zeros(n)
            for i in range(n):
                weights[i] = (1 - decay) * (decay ** (n - 1 - i))
            
            weights = weights / weights.sum()
            
            xs = np.array([p[0] for p in positions])
            ys = np.array([p[1] for p in positions])
            ws = np.array(sizes)
            
            self.smoothed_x = int(np.dot(xs, weights))
            self.smoothed_y = int(np.dot(ys, weights))
            self.smoothed_width = int(np.dot(ws, weights))
    
    def get_status(self) -> Dict[str, Any]:
        """获取状态"""
        # 计算跟踪稳定性分数（基于连续命中次数）
        stability_score = min(1.0, self.consecutive_hits / 10.0) if self.consecutive_hits > 0 else 0.0
        
        return {
            'x': int(self.smoothed_x),
            'y': int(self.smoothed_y),
            'width': int(self.smoothed_width),
            'state': self.current_state,
            'state_name': self.current_state.name,
            'miss_count': self.consecutive_misses,
            'hit_count': self.consecutive_hits,
            'real_lost_count': self.real_lost_count,
            'stability_score': stability_score,
            'velocity_x': self.velocity_x,
            'velocity_y': self.velocity_y,
            'history_size': len(self.position_history)
        }
    
    def reset(self):
        """重置跟踪器"""
        self.position_history.clear()
        self.size_history.clear()
        self.timestamp_history.clear()
        self.human_velocity_history.clear()
        
        self.current_state = TargetState.LOST
        self.consecutive_misses = 0
        self.consecutive_hits = 0
        self.real_lost_count = 0
        self.velocity_x = 0
        self.velocity_y = 0
        self.last_position = None
        self.last_time = None

# ============================================================================
# 4. 增强版ObjectTracker（人体跟随车体控制完整版）
# ============================================================================

class EnhancedObjectTracker:
    """增强版ObjectTracker - 人体跟随车体控制完整版"""
    
    def __init__(self, color, node, set_color, set_status=False):
        # 保持原始初始化
        self.node = node
        self.y_stop = 120
        self.pro_size = (320, 240)
        self.last_color_circle = None
        self.lost_target_count = 0

        # 新增：详细状态跟踪
        self.last_state = TargetState.LOST
        self.state_change_count = 0
        self.state_change_log_interval = 10  # 每10次状态变化记录一次
        
        # 新增：检测计数器
        self.detection_count = 0
        self.last_detection_time = 0

        self.set_status = set_status
        self.set_color = set_color
        if color is not None:
            self.target_lab, self.target_rgb = color
        self.lab_data = yaml_handle.get_yaml_data(yaml_handle.lab_file_path)

        # 颜色RGB值定义
        self.range_rgb = {
            'red': (255, 0, 0),
            'blue': (0, 0, 255),
            'green': (0, 255, 0),
            'black': (0, 0, 0),
            'white': (255, 255, 255),
        }

        self.threshold = 0.3

        # 云台PID参数 - 人体跟踪优化（云台向上）
        self.servo_x_pid = pid.PID(P=0.25, I=0.015, D=0.04)  # 水平方向
        self.servo_y_pid = pid.PID(P=0.35, I=0.018, D=0.03)  # 垂直方向
        
        # 底盘PID参数（保留但不用于人体跟随）
        self.chassis_yaw_pid = pid.PID(P=0.018, I=0.004, D=0.0015)   # 转向控制
        self.chassis_dist_pid = pid.PID(P=0.012, I=0.003, D=0.001)   # 距离控制

        # 舵机参数
        self.servo_x = 1500
        
        # 关键修改：人体跟踪时云台应该更向上
        if set_status and set_color == 'body':
            self.servo_y = 1000
            self.node.get_logger().info("人体跟踪模式：云台初始位置向上调整")
        elif set_status and set_color == 'face':
            self.servo_y = 1100
        elif set_status and set_color == 'hand':
            self.servo_y = 1150
        else:
            self.servo_y = 1200
        
        self.servo_min_x = 400
        self.servo_max_x = 2600
        self.servo_min_y = 700
        self.servo_max_y = 1900
        
        self.body_tracking_min_y = 900
        self.body_tracking_max_y = 1300

        self.pan_tilt_x_threshold = 10
        self.pan_tilt_y_threshold = 10
        
        self.last_servo_x = 1500
        self.last_servo_y = self.servo_y
        self.servo_smooth_alpha = 0.7
        
        self.human_tracking_mode = (set_status and set_color == 'body')
        self.last_target_size = 0
        self.target_size_history = deque(maxlen=10)
        
        self.body_tracking_upward_bias = 0.2
        self.last_body_y_position = 0
        self.body_y_history = deque(maxlen=10)
        
        self.pid_error_reset_counter = 0
        self.max_pid_error_frames = 5
        
        self.pid_output_max = 40
        self.pid_output_min = -40
        
        self.pid_output_history_x = deque(maxlen=10)
        self.pid_output_history_y = deque(maxlen=10)
        
        self.target_position_history = deque(maxlen=20)

        self.smooth_tracker = SmartSmoothTargetTracker({
            'window_size': 10,
            'max_predict_frames': 4,
            'lost_threshold': 5
        })
        
        self.image_logger = ChassisFollowingLogger("/mnt/body_following_logs", 500)
        
        self.tracker_full_obj = None
        self.detection_mode = set_color if set_status else "color"
        self.det_thr = 0.4
        
        self.target_lost_frames = 0
        self.max_lost_frames_before_stop = 5
        
        self.calibration_state = {
            'active': False,
            'start_time': 0,
            'duration': 0.5,
            'cooldown': 1.0,
            'last_calibration': 0,
            'threshold': 80,
            'angular_speed': 0.25
        }
        
        # 初始化tracker_full
        if set_status and set_color in ['face', 'body', 'hand']:
            self._init_tracker_full(set_color)
            if set_color == 'body':
                self.node.get_logger().info("人体跟踪模式已启用，云台将保持向上")
                self.node.get_logger().info("车体跟随：固定速度前进/停止，带智能校准")
                self.node.get_logger().info(f"校准阈值：{self.calibration_state['threshold']}像素")
                self.node.get_logger().info(f"最大丢失帧数：{self.max_lost_frames_before_stop}")
    
    # MODIFIED: 重写 _init_tracker_full，使用 tracker_binding 代替原 api_tracker_body
    def _init_tracker_full(self, mode: str):
        """使用 tracker_binding 初始化 AI 检测器"""
        if not TRACKER_AVAILABLE:
            print(f"tracker_binding module not available, cannot initialize {mode} detector")
            return False
        
        try:
            # 模型路径映射 (body -> person)
            actual_mode = 'person' if mode == 'body' else mode
            det_model = os.path.join(tracker_full_path, f"models/{actual_mode}/sim.engine")
            track_model = os.path.join(tracker_full_path, "models/reid/sim.engine")
            
            if not os.path.exists(det_model):
                print(f"Detection model not found: {det_model}")
                return False
            if not os.path.exists(track_model):
                print(f"Tracking model not found: {track_model}")
                return False
            
            # 创建 tracker_binding.Tracker 实例
            self.tracker_full_obj = tracker_binding.Tracker()
            
            # 设置参数
            self.tracker_full_obj.set_param(
                conf_thresh=self.det_thr,
                iou_thresh=0.3,
                score_thresh=0.2,
                nn_budget=10,
                max_cosine_distance=0.4,
                use_reid=True
            )
            
            # 初始化
            device_id = 0
            if not self.tracker_full_obj.init(det_model, track_model, LICENSE_FILE, device_id, AUTH_CODE):
                print(f"tracker_full init failed for mode {mode}")
                return False
            
            self.detection_mode = mode
            print(f"tracker_full {mode} detector initialized successfully (using {actual_mode} model)")
            return True
            
        except Exception as e:
            print(f"Failed to init tracker_full: {e}")
            traceback.print_exc()
            return False
    
    def detect_with_tracker_full(self, image, force_log=False):
        """使用tracker_full进行检测"""
        if not self.tracker_full_obj or image is None:
            return None
        
        try:
            # 预处理图像
            processed_image = cv2.resize(image, (640, 480))
            rgb_image = cv2.cvtColor(processed_image, cv2.COLOR_BGR2RGB)
            
            # 执行检测
            detections = self.tracker_full_obj.process(rgb_image)
            
            if not detections:
                return None
            
            # 选择面积最大的目标
            selected_target = max(detections, key=lambda d: d.width * d.height)
            
            # 坐标转换
            img_h, img_w = image.shape[:2]
            scale_x = img_w / 640
            scale_y = img_h / 480
            
            x = int(selected_target.x * scale_x)
            y = int(selected_target.y * scale_y)
            width = int(selected_target.width * scale_x)
            height = int(selected_target.height * scale_y)
            
            # 边界检查
            x = max(0, min(x, img_w-1))
            y = max(0, min(y, img_h-1))
            width = max(10, min(width, img_w-x))
            height = max(10, min(height, img_h-y))
            
            # 计算中心点
            center_x = x + width // 2
            center_y = y + height // 2
            
            # 关键修改：人体跟踪时调整目标点向上
            if self.human_tracking_mode:
                self.body_y_history.append(center_y)
                self.last_body_y_position = center_y
                
                upward_adjustment = int(height * 0.15)
                center_y = max(0, center_y - upward_adjustment)
                
                if self.node.debug and self.node.frame_count % 30 == 0:
                    self.node.get_logger().info(f"人体跟踪：目标点向上调整 {upward_adjustment} 像素")
            
            # 创建检测结果
            detection = DetectionResult(
                x=center_x,
                y=center_y,
                width=width,
                height=height,
                class_name=self.detection_mode
            )
            
            # 更新平滑跟踪器
            tracker_status = self.smooth_tracker.update(detection)
            
            # 记录目标大小历史
            target_area = width * height
            self.target_size_history.append(target_area)
            self.last_target_size = target_area
            
            # 保存图像日志
            if self.human_tracking_mode and (force_log or self.node.frame_count % 30 == 0):
                control_info = {
                    'servo_x': self.servo_x,
                    'servo_y': self.servo_y,
                    'linear_x': self.node.last_linear_x,
                    'angular_z': self.node.last_angular_z,
                    'pid_x': 0,
                    'pid_y': 0
                }
                
                filename = self.image_logger.save_chassis_debug_image(
                    image,
                    detection,
                    self.node.last_linear_x,
                    self.node.last_angular_z,
                    width / img_w if img_w > 0 else 0,
                    tracker_status.get('state_name', 'UNKNOWN'),
                    mode_name=self.detection_mode
                )
                
                if self.node.debug and self.node.frame_count % 60 == 0:
                    self.node.get_logger().info(f"保存人体检测日志: {filename}")
            
            return detection
            
        except Exception as e:
            print(f"object_tracker detection error: {e}")
            traceback.print_exc()
            return None
    
    def update_pid(self, x, y, img_w, img_h, use_smooth=True):
        """更新云台PID - 人体跟踪云台向上优化"""
        # 获取跟踪器状态
        tracker_status = self.smooth_tracker.get_status()
        
        # 检查是否需要重置PID
        if tracker_status['state'] == TargetState.LOST:
            self.pid_error_reset_counter += 1
            if self.pid_error_reset_counter >= self.max_pid_error_frames:
                self.servo_x_pid.clear()
                self.servo_y_pid.clear()
                self.pid_error_reset_counter = 0
                self.pid_output_history_x.clear()
                self.pid_output_history_y.clear()
                self.node.get_logger().info("目标丢失，PID已重置")
                return self.servo_x, self.servo_y, 0, 0
        else:
            self.pid_error_reset_counter = 0
        
        # 只有在TRACKING或DETECTED状态时才进行PID控制
        if tracker_status['state'] not in [TargetState.TRACKING, TargetState.DETECTED]:
            return self.servo_x, self.servo_y, 0, 0
        
        if use_smooth:
            smooth_x = tracker_status['x']
            smooth_y = tracker_status['y']
        else:
            smooth_x = x
            smooth_y = y
        
        # 阈值处理
        if abs(smooth_x - img_w / 2) < self.pan_tilt_x_threshold:
            smooth_x = img_w / 2
        if abs(smooth_y - img_h / 2) < self.pan_tilt_y_threshold:
            smooth_y = img_h / 2

        # 动态PID调整 - 人体跟踪优化
        error_x = abs(smooth_x - img_w / 2)
        error_y = abs(smooth_y - img_h / 2)
        
        # 人体跟踪：使用不同的PID参数
        if self.human_tracking_mode:
            if error_x > 100:
                self.servo_x_pid.Kp = 0.28
                self.servo_x_pid.Ki = 0.016
                self.servo_x_pid.Kd = 0.045
            elif error_x > 60:
                self.servo_x_pid.Kp = 0.25
                self.servo_x_pid.Ki = 0.015
                self.servo_x_pid.Kd = 0.04
            elif error_x > 30:
                self.servo_x_pid.Kp = 0.22
                self.servo_x_pid.Ki = 0.014
                self.servo_x_pid.Kd = 0.035
            else:
                self.servo_x_pid.Kp = 0.20
                self.servo_x_pid.Ki = 0.013
                self.servo_x_pid.Kd = 0.03
                
            if error_y > 100:
                self.servo_y_pid.Kp = 0.40
                self.servo_y_pid.Ki = 0.016
                self.servo_y_pid.Kd = 0.035
            elif error_y > 60:
                self.servo_y_pid.Kp = 0.35
                self.servo_y_pid.Ki = 0.015
                self.servo_y_pid.Kd = 0.03
            elif error_y > 30:
                self.servo_y_pid.Kp = 0.30
                self.servo_y_pid.Ki = 0.014
                self.servo_y_pid.Kd = 0.025
            else:
                self.servo_y_pid.Kp = 0.25
                self.servo_y_pid.Ki = 0.013
                self.servo_y_pid.Kd = 0.02

        # X轴PID控制
        self.servo_x_pid.SetPoint = img_w / 2
        self.servo_x_pid.update(smooth_x)
        servo_x_output = int(self.servo_x_pid.output)
        servo_x_output = np.clip(servo_x_output, self.pid_output_min, self.pid_output_max)
        
        self.pid_output_history_x.append(servo_x_output)
        
        self.servo_x += servo_x_output
        self.servo_x = np.clip(self.servo_x, self.servo_min_x, self.servo_max_x)

        # Y轴PID控制 - 人体跟随优化
        self.servo_y_pid.SetPoint = img_h / 2
        self.servo_y_pid.update(smooth_y)
        servo_y_output = int(self.servo_y_pid.output)
        
        error_y_dir = smooth_y - img_h / 2
        
        if self.human_tracking_mode:
            if error_y_dir < 0:  # 目标在上方
                servo_y_output = -abs(servo_y_output)
                if abs(error_y_dir) > 50:
                    servo_y_output *= 1.5
            else:  # 目标在下方
                servo_y_output = abs(servo_y_output) * 0.3
                if self.servo_y > 1300:
                    servo_y_output = max(0, servo_y_output * 0.1)
        else:
            if error_y_dir < 0:
                servo_y_output = -abs(servo_y_output)
                if abs(error_y_dir) > 50:
                    servo_y_output *= 1.3
            else:
                servo_y_output = abs(servo_y_output)
        
        servo_y_output = np.clip(servo_y_output, self.pid_output_min, self.pid_output_max)
        self.pid_output_history_y.append(servo_y_output)
        
        self.servo_y += servo_y_output
        if self.human_tracking_mode:
            self.servo_y = np.clip(self.servo_y, self.body_tracking_min_y, self.body_tracking_max_y)
        else:
            self.servo_y = np.clip(self.servo_y, self.servo_min_y, self.servo_max_y)
        
        self.servo_x = self.servo_x * self.servo_smooth_alpha + self.last_servo_x * (1 - self.servo_smooth_alpha)
        self.servo_y = self.servo_y * self.servo_smooth_alpha + self.last_servo_y * (1 - self.servo_smooth_alpha)
        
        self.last_servo_x = self.servo_x
        self.last_servo_y = self.servo_y

        return self.servo_x, self.servo_y, servo_x_output, servo_y_output
        
    def control_chassis_simple_follow(self, tracker_status, img_w, img_h):
        """
        简化的车体跟随控制逻辑
        只前进和停止，固定速度，带智能左右校准
        """
        current_time = time.time()
        
        current_state = tracker_status['state']
        current_state_name = tracker_status['state_name']
        
        if current_state != self.last_state:
            self.state_change_count += 1
            self.node.get_logger().info(f"状态变化: {self.last_state.name} -> {current_state_name} (变化次数: {self.state_change_count})")
            self.last_state = current_state
        
        if self.node.frame_count % 30 == 0:
            self.node.get_logger().info(f"跟踪状态: {current_state_name}, 丢失帧数: {self.target_lost_frames}, 目标宽度: {tracker_status['width']}")
        
        if current_state == TargetState.LOST:
            self.target_lost_frames += 1
            if self.target_lost_frames >= self.max_lost_frames_before_stop:
                linear_x = 0.0
                angular_z = 0.0
                self._log_chassis_data(current_time, self.node.frame_count, 0, img_w, 0, img_w // 2, 0, linear_x, angular_z, 'LOST')
                if self.target_lost_frames > 50:
                    self.reset()
                    self.node.get_logger().info("长时间目标丢失，重置跟踪器")
                    return 0.0, 0.0
                return linear_x, angular_z
            else:
                return 0.0, 0.0
        
        if self.target_lost_frames > 0:
            self.node.get_logger().info(f"目标重新出现！之前连续丢失 {self.target_lost_frames} 帧")
            self.target_lost_frames = 0
        
        self.detection_count += 1
        self.last_detection_time = current_time
        
        if current_state == TargetState.PREDICTING:
            return 0.0, 0.0
        
        if current_state not in [TargetState.TRACKING, TargetState.DETECTED]:
            self.node.get_logger().info(f"非控制状态: {current_state_name}")
            return 0.0, 0.0
        
        target_width = tracker_status['width']
        target_x = tracker_status['x']
        width_ratio = target_width / img_w if img_w > 0 else 0
        center_x = img_w // 2
        error_x = target_x - center_x
        
        if width_ratio < 0.40:
            linear_x = 0.4
            if self.detection_count % 20 == 0:
                self.node.get_logger().info(f"前进跟随: 宽度比例 {width_ratio:.2%}, 误差 {error_x} 像素")
        else:
            linear_x = 0.0
        
        angular_z = 0.0
        if linear_x > 0:
            angular_z = self._calculate_calibration(error_x, current_time, linear_x)
        
        self._log_chassis_data(current_time, self.node.frame_count, target_width, img_w, target_x, center_x, error_x, linear_x, angular_z, current_state_name)
        
        if self.human_tracking_mode and self.node.frame_count % 30 == 0:
            detection = DetectionResult(
                x=target_x,
                y=tracker_status['y'],
                width=target_width,
                height=target_width,
                class_name='body'
            )
            self.image_logger.save_chassis_debug_image(
                self.node.current_image if hasattr(self.node, 'current_image') and self.node.current_image is not None else np.zeros((480, 640, 3), dtype=np.uint8),
                detection,
                linear_x,
                angular_z,
                width_ratio,
                current_state_name
            )
        
        return linear_x, angular_z

    def _calculate_calibration(self, error_x, current_time, linear_x):
        """
        智能计算校准角速度
        返回：角速度值
        """
        calibration = self.calibration_state
        threshold = calibration['threshold']
        
        time_since_last_cal = current_time - calibration['last_calibration']
        if time_since_last_cal < calibration['cooldown']:
            return 0.0
        
        if abs(error_x) > threshold:
            if not calibration['active']:
                calibration['active'] = True
                calibration['start_time'] = current_time
                calibration['last_calibration'] = current_time
                
                if error_x > 0:
                    angular_speed = -calibration['angular_speed']
                    self.node.get_logger().info(f"开始左转校准：目标偏右 {error_x} 像素")
                else:
                    angular_speed = calibration['angular_speed']
                    self.node.get_logger().info(f"开始右转校准：目标偏左 {abs(error_x)} 像素")
                return angular_speed
            
            if current_time - calibration['start_time'] >= calibration['duration']:
                calibration['active'] = False
                self.node.get_logger().info("校准完成")
                return 0.0
            else:
                if error_x > 0:
                    return -calibration['angular_speed']
                else:
                    return calibration['angular_speed']
        
        if calibration['active']:
            calibration['active'] = False
            self.node.get_logger().info("校准提前完成：偏移已回到阈值内")
        return 0.0
    
    def _log_chassis_data(self, timestamp, frame_count, target_width, image_width, 
                         target_x, center_x, error_x, linear_x, angular_z, state):
        """记录车体跟随数据"""
        current_time_str = datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]
        self.image_logger.log_chassis_data(
            current_time_str, frame_count, target_width, image_width,
            target_x, center_x, error_x, linear_x, angular_z, state
        )
    
    def reset(self):
        """重置跟踪器"""
        self.smooth_tracker.reset()
        self.servo_x_pid.clear()
        self.servo_y_pid.clear()
        self.chassis_yaw_pid.clear()
        self.chassis_dist_pid.clear()
        
        self.servo_x = 1500
        if self.human_tracking_mode:
            self.servo_y = 1000
        else:
            self.servo_y = 1100
        
        self.last_servo_x = self.servo_x
        self.last_servo_y = self.servo_y
        
        self.target_size_history.clear()
        self.pid_output_history_x.clear()
        self.pid_output_history_y.clear()
        self.target_position_history.clear()
        self.body_y_history.clear()
        
        self.target_lost_frames = 0
        self.calibration_state['active'] = False
        self.calibration_state['last_calibration'] = 0

# ============================================================================
# 5. 主节点（人体跟随车体控制完整版）
# ============================================================================

class AITrackingNode(Node):
    def __init__(self, name):
        super().__init__(name, allow_undeclared_parameters=True,
                        automatically_declare_parameters_from_overrides=True)
        
        self.name = name
        self.get_logger().info(f"Initializing {name} node - Human Following (Chassis Only Complete)")
        
        # 基础初始化
        self.set_callback = False
        self.color_picker = None
        self.is_running = False
        self.set_model = False
        self.__target_color = None
        self.heart = None
        
        # 控制使能标志
        self.pan_tilt_enabled = False
        self.chassis_following_enabled = False
        self.exit_funcation = False
        
        # 图像相关
        self.bridge = CvBridge()
        self.image_queue = queue.Queue(2)
        self.current_image = None
        self.image_height = 0
        self.image_width = 0
        self.last_image_time = 0
        
        # 性能监控
        self.frame_count = 0
        self.current_fps = 0.0
        self.last_fps_time = time.time()
        self.processing_times = deque(maxlen=100)
        
        # 阈值参数
        self.threshold = 0.3
        self.color_sensitivity = 1.0
        
        # 云台参数
        self.servo_state = [1500, 1500]
        self.servo_pub = self.create_publisher(SetPWMServoState, 
                                              'ros_robot_controller/pwm_servo/set_state', 10)
        
        # 底盘参数
        self.cmd_vel_pub = self.create_publisher(Twist, 'cmd_vel', 10)
        self.last_linear_x = 0.0
        self.last_angular_z = 0.0
        
        # RGB灯
        self.rgb_pub = self.create_publisher(RGBStates, 'ros_robot_controller/set_rgb', 10)
        
        # 结果发布
        self.result_publisher = self.create_publisher(Image, '~/image_result', 1)
        
        # 状态锁
        self.lock = threading.RLock()
        
        # 回调组
        self.callback_group = ReentrantCallbackGroup()
        
        # 心跳服务
        self.heart = Heart(self, self.name + '/heartbeat', 5,
                          lambda _: self.exit_srv_callback(Trigger.Request(), 
                                                          Trigger.Response()))
        
        # 小车参数
        self.car_x_threshold = 12
        self.car_y_threshold = 12
        self.target_radius = 100
        self.target_x = 0.5
        self.target_y = 0.5
        
        # 底盘PID
        self.pid_yaw = pid.PID(0.025, 0.005, 0.0005)
        self.pid_dist = pid.PID(0.015, 0.005, 0.0001)
        self.x_stop = 320
        self.y_stop = 400
        
        # 当前检测模式
        self.current_detection_mode = None
        
        # 目标丢失计数器
        self.target_lost_counter = 0
        self.max_lost_frames_before_stop = 10
        
        # 当前云台保持位置
        self.current_servo_x = 1500
        self.current_servo_y = 1100  # 人体跟踪稍向上
        
        # 调试模式
        self.debug = self.get_parameter('debug').value if self.has_parameter('debug') else True
        
        # 跟踪器
        self.tracker = None
        
        # 控制线程
        self.control_thread_running = True
        self.control_thread = threading.Thread(target=self._control_loop, daemon=True)
        self.control_thread.start()
        
        self.last_control_log_time = 0
        self.last_target_time = time.time()
        
        # 新增：人体跟随专用参数
        self.human_following_optimized = False
        self.last_human_detection = None
        self.human_detection_count = 0
        
        # MODIFIED: 摄像头初始化，替代 ROS2 图像订阅
        self.cap = cv2.VideoCapture(4)  # /dev/video4
        if not self.cap.isOpened():
            self.get_logger().error("Failed to open camera /dev/video4")
        else:
            self.get_logger().info("Camera /dev/video4 opened successfully")
        
        # MODIFIED: 图像处理定时器，暂未启动
        self.process_timer = None
        
        # ROS服务初始化
        self._init_services()
        
        self.get_logger().info(f"{name} node initialization complete!")
        self.get_logger().info("车体跟随模式：固定速度前进/停止，带智能校准")
        self.get_logger().info("目标丢失处理：连续5帧丢失后停止")
        self.get_logger().info("校准策略：偏移超过80像素时脉冲式校准")
        self.get_logger().info("车体跟随日志保存到：/mnt/body_following_logs/")
        self.get_logger().info("Waiting for /enter service call to start tracking...")
    
    def _init_services(self):
        """初始化ROS服务（保持原接口）"""
        self.enter_srv = self.create_service(Trigger, '~/enter', self.enter_srv_callback)
        self.exit_srv = self.create_service(Trigger, '~/exit', self.exit_srv_callback)
        self.set_running_srv = self.create_service(SetBool, '~/set_running', 
                                                  self.set_running_srv_callback)
        self.set_target_color_srv = self.create_service(SetPoint, '~/set_target_color',
                                                       self.set_target_color_srv_callback)
        self.get_target_color_srv = self.create_service(Trigger, '~/get_target_color',
                                                       self.get_target_color_callback)
        self.set_threshold_srv = self.create_service(SetFloat64, '~/set_threshold',
                                                    self.set_threshold_srv_callback)
        self.set_chassis_following_srv = self.create_service(SetBool, '~/set_chassis_following',
                                                            self.set_chassis_following_callback)
        self.set_pan_tilt_srv = self.create_service(SetBool, '~/set_pan_tilt',
                                                   self.set_pan_tilt_callback)
        self.set_detector_srv = self.create_service(SetString, '~/set_detector',
                                                   self.set_detector_callback)
        self.set_large_model_target_color_srv = self.create_service(SetString,
                                                                   '~/set_large_model_target_color',
                                                                   self.set_large_model_target_color_srv_callback)
    
    def smooth_value(self, current_value, previous_value, alpha):
        """速度平滑处理"""
        return alpha * current_value + (1 - alpha) * previous_value
    
    def _control_loop(self):
        """控制循环线程"""
        control_interval = 0.1
        
        while self.control_thread_running and rclpy.ok():
            try:
                start_time = time.time()
                
                # 只在运行状态下处理控制
                if self.is_running and self.current_image is not None:
                    self._process_control()
                
                # 精确控制频率
                elapsed = time.time() - start_time
                sleep_time = max(0, control_interval - elapsed)
                if sleep_time > 0:
                    time.sleep(sleep_time)
                    
            except Exception as e:
                self.get_logger().error(f"Control loop error: {e}")
                time.sleep(0.1)
    
    def _process_control(self):
        """智能控制逻辑 - 人体跟随车体控制优化"""
        with self.lock:
            if self.tracker is None:
                return
            
            if hasattr(self.tracker, 'smooth_tracker'):
                tracker_status = self.tracker.smooth_tracker.get_status()
            else:
                return
            
            current_state = tracker_status['state']
            human_mode = self.current_detection_mode == 'body'
            
            if self.pan_tilt_enabled and not self.chassis_following_enabled:
                self._process_pan_tilt_only(tracker_status, current_state, human_mode)
            elif self.chassis_following_enabled and not self.pan_tilt_enabled:
                self._process_chassis_only_simple(tracker_status, current_state, human_mode)
            elif self.pan_tilt_enabled and self.chassis_following_enabled:
                self._process_combined(tracker_status, current_state, human_mode)
            elif not self.pan_tilt_enabled and not self.chassis_following_enabled:
                self._stop_all_control()
    
    def _process_pan_tilt_only(self, tracker_status, current_state, human_mode):
        """处理单独云台跟踪"""
        if current_state in [TargetState.TRACKING, TargetState.DETECTED]:
            self.target_lost_counter = 0
            if hasattr(self.tracker, 'update_pid'):
                servo_x, servo_y, pid_x, pid_y = self.tracker.update_pid(
                    tracker_status['x'], tracker_status['y'],
                    self.image_width, self.image_height, use_smooth=True
                )
                self.current_servo_x = servo_x
                self.current_servo_y = servo_y
                self._publish_servo(servo_x, servo_y)
                self.last_target_time = time.time()
                
                if human_mode and self.debug and (time.time() - self.last_control_log_time > 0.5):
                    log_msg = f"人体云台跟踪 | 状态: {tracker_status['state_name']} | "
                    log_msg += f"目标位置: ({tracker_status['x']}, {tracker_status['y']}) | "
                    log_msg += f"云台位置: ({servo_x}, {servo_y}) | "
                    log_msg += f"目标大小: {tracker_status['width']}x{tracker_status.get('height', 0)} | "
                    log_msg += f"PID输出: ({pid_x:.1f}, {pid_y:.1f}) | "
                    log_msg += f"稳定性: {tracker_status.get('stability_score', 0):.2f}"
                    self.get_logger().info(log_msg)
                    self.last_control_log_time = time.time()
        else:
            self.target_lost_counter += 1
            if self.target_lost_counter >= self.max_lost_frames_before_stop:
                self._publish_servo(self.current_servo_x, self.current_servo_y)
                self.tracker.servo_x_pid.clear()
                self.tracker.servo_y_pid.clear()
    
    def _process_chassis_only_simple(self, tracker_status, current_state, human_mode):
        """处理单独车体跟踪"""
        state_name = tracker_status.get('state_name', 'UNKNOWN')
        if self.frame_count % 20 == 0:
            self.get_logger().info(f"控制处理: 状态={state_name}, 目标位置=({tracker_status.get('x', 0)}, {tracker_status.get('y', 0)}), 大小={tracker_status.get('width', 0)}")
        
        if human_mode and hasattr(self.tracker, 'control_chassis_simple_follow'):
            try:
                linear_x, angular_z = self.tracker.control_chassis_simple_follow(
                    tracker_status, self.image_width, self.image_height
                )
                self._publish_servo(self.current_servo_x, self.current_servo_y)
                linear_x = self.smooth_value(linear_x, self.last_linear_x, 0.9)
                angular_z = self.smooth_value(angular_z, self.last_angular_z, 0.9)
                self.last_linear_x = linear_x
                self.last_angular_z = angular_z
                self._send_twist(linear_x, 0.0, angular_z)
                
                if current_state == TargetState.LOST:
                    if self.debug and time.time() - self.last_control_log_time > 0.5:
                        lost_frames = self.tracker.target_lost_frames if hasattr(self.tracker, 'target_lost_frames') else 0
                        self.get_logger().info(f"目标丢失中 | 停止运动 | 连续丢失帧数: {lost_frames}")
                        self.last_control_log_time = time.time()
                elif current_state == TargetState.DETECTED and state_name != 'LOST':
                    if self.debug and time.time() - self.last_control_log_time > 0.5:
                        self.get_logger().info(f"目标重新检测到 | 状态: {state_name} | 开始处理控制")
                        self.last_control_log_time = time.time()
                elif self.debug and time.time() - self.last_control_log_time > 1.0:
                    width_ratio = tracker_status['width'] / self.image_width if self.image_width > 0 else 0
                    log_msg = f"人体车体跟随 | 状态: {state_name} | "
                    log_msg += f"宽度比例: {width_ratio:.1%} | "
                    log_msg += f"速度: ({linear_x:.2f}, {angular_z:.2f})"
                    self.get_logger().info(log_msg)
                    self.last_control_log_time = time.time()
            except Exception as e:
                self.get_logger().error(f"控制处理错误: {e}")
                traceback.print_exc()
                self._send_twist(0.0, 0.0, 0.0)
        self.last_target_time = time.time()
    
    def _process_combined(self, tracker_status, current_state, human_mode):
        """处理云台+车体联合跟踪"""
        if current_state in [TargetState.TRACKING, TargetState.DETECTED]:
            self.target_lost_counter = 0
            if hasattr(self.tracker, 'update_pid'):
                servo_x, servo_y, pid_x_output, pid_y_output = self.tracker.update_pid(
                    tracker_status['x'], tracker_status['y'],
                    self.image_width, self.image_height, use_smooth=True
                )
                self.current_servo_x = servo_x
                self.current_servo_y = servo_y
                self._publish_servo(servo_x, servo_y)
                
                if human_mode:
                    angular_z = common.set_range(pid_x_output * 0.015, -0.5, 0.5)
                    target_width = tracker_status['width']
                    if target_width > 0:
                        size_ratio = target_width / self.image_width
                        if size_ratio > 0.22:
                            linear_x = -0.15
                        elif size_ratio < 0.1:
                            linear_x = 0.15
                        else:
                            linear_x = 0.0
                    else:
                        linear_x = 0.0
                else:
                    angular_z = common.set_range(pid_x_output * 0.03, -1.0, 1.0)
                    linear_x = common.set_range(pid_y_output * 0.03, -0.5, 0.5)
                
                linear_x = self.smooth_value(linear_x, self.last_linear_x, 0.5)
                angular_z = self.smooth_value(angular_z, self.last_angular_z, 0.5)
                self.last_linear_x = linear_x
                self.last_angular_z = angular_z
                
                self._send_twist(linear_x, 0.0, angular_z)
                self.last_target_time = time.time()
        else:
            self.target_lost_counter += 1
            if self.target_lost_counter >= self.max_lost_frames_before_stop:
                self._send_twist(0.0, 0.0, 0.0)
                self._publish_servo(self.current_servo_x, self.current_servo_y)
                if self.tracker:
                    self.tracker.servo_x_pid.clear()
                    self.tracker.servo_y_pid.clear()
                self.pid_yaw.clear()
                self.pid_dist.clear()
    
    def _stop_all_control(self):
        """停止所有控制"""
        self._send_twist(0, 0, 0)
        self._publish_servo(1500, 1500)
        self.pid_yaw.clear()
        self.pid_dist.clear()
        if self.tracker:
            self.tracker.servo_x_pid.clear()
            self.tracker.servo_y_pid.clear()
        self.target_lost_counter = 0
    
    def control_chassis(self, x, y, servo_x, target_radius):
        """控制底盘运动（单独车体追踪模式）"""
        yaw_error = x - self.x_stop
        dist_error = y - self.y_stop

        self.pid_yaw.update(yaw_error)
        self.pid_dist.update(dist_error)

        angular_z = common.set_range(self.pid_yaw.output * 3.0, -2.0, 2.0)
        linear_x = common.set_range(self.pid_dist.output * 3.0, -0.8, 0.8)

        if abs(linear_x) < 0.03:
            linear_x = 0.0
        if abs(angular_z) < 0.05:
            angular_z = 0.0

        linear_x = self.smooth_value(linear_x, self.last_linear_x, 0.6)
        angular_z = self.smooth_value(angular_z, self.last_angular_z, 0.6)

        self.last_linear_x = linear_x
        self.last_angular_z = angular_z

        self._send_twist(linear_x, 0.0, angular_z)
    
    # MODIFIED: 新增摄像头读取与处理函数，替代原 image_callback
    def _camera_read_and_process(self):
        """定时器回调：从摄像头读取一帧并处理"""
        if not self.is_running or self.cap is None or not self.cap.isOpened():
            return
        ret, frame = self.cap.read()
        if not ret:
            self.get_logger().warn("Failed to read frame from camera", throttle_duration_sec=2.0)
            return
        # frame 为 BGR 格式，直接传递
        self._process_frame(frame)

    def _process_frame(self, bgr_image):
        """处理一帧图像（原 image_callback 逻辑）"""
        start_time = time.time()
        
        # 计算FPS
        current_time = time.time()
        if current_time - self.last_fps_time >= 1.0:
            self.current_fps = self.frame_count / (current_time - self.last_fps_time)
            self.frame_count = 0
            self.last_fps_time = current_time
        self.frame_count += 1
        
        try:
            with self.lock:
                self.current_image = bgr_image.copy()
                self.image_height, self.image_width = bgr_image.shape[:2]
                
                # 颜色选择逻辑
                if self.color_picker is not None:
                    rgb_image = cv2.cvtColor(bgr_image, cv2.COLOR_BGR2RGB)
                    target_color, result_image = self.color_picker(rgb_image, rgb_image.copy())
                    if target_color is not None:
                        self.color_picker = None
                        self.tracker = EnhancedObjectTracker(target_color, self, None, False)
                        self._publish_rgb(target_color[1][0], target_color[1][1], target_color[1][2])
                        self.is_running = True
                        self.set_model = False
                        self.current_detection_mode = 'color'
                        self.get_logger().info(f"Color target set")
                
                # 检测与跟踪
                result_image = bgr_image.copy()
                target_pos = None
                target_radius = 0
                
                if self.tracker is not None and self.is_running:
                    try:
                        if hasattr(self.tracker, 'detect_with_tracker_full') and self.tracker.tracker_full_obj:
                            detection = self.tracker.detect_with_tracker_full(bgr_image, force_log=(self.frame_count % 40 == 0))
                            if detection:
                                x1 = detection.x - detection.width // 2
                                y1 = detection.y - detection.height // 2
                                x2 = x1 + detection.width
                                y2 = y1 + detection.height
                                
                                color_map = {
                                    'face': (0, 255, 0),
                                    'body': (255, 0, 0),
                                    'hand': (0, 255, 255),
                                    'color': (255, 0, 255),
                                }
                                color = color_map.get(detection.class_name, (0, 255, 0))
                                
                                cv2.rectangle(result_image, (x1, y1), (x2, y2), color, 2)
                                cv2.circle(result_image, (detection.x, detection.y), 5, (0, 0, 255), -1)
                                label = detection.class_name
                                cv2.putText(result_image, label, (x1, max(20, y1-10)),
                                           cv2.FONT_HERSHEY_SIMPLEX, 0.6, color, 2)
                                if detection.class_name == 'body':
                                    self.last_human_detection = detection
                                    self.human_detection_count += 1
                        else:
                            result_image, target_pos, target_radius = self.tracker(
                                cv2.cvtColor(bgr_image, cv2.COLOR_BGR2RGB), result_image, self.threshold
                            )
                    except Exception as e:
                        self.get_logger().error(f"Detection error: {e}")
                        traceback.print_exc()
                
                # 绘制调试信息
                self._draw_debug_overlay(result_image)
                
                # 发布结果图像（转为RGB消息）
                ros_img_msg = self.bridge.cv2_to_imgmsg(cv2.cvtColor(result_image, cv2.COLOR_BGR2RGB), "rgb8")
                self.result_publisher.publish(ros_img_msg)
                
                if self.debug and not self.image_queue.full():
                    try:
                        self.image_queue.put_nowait(result_image)
                    except:
                        pass
                
                if self.current_fps > 0:
                    fps_text = f"FPS: {self.current_fps:.1f} | Mode: {self.current_detection_mode}"
                    if self.current_detection_mode == 'body' and self.last_human_detection:
                        fps_text += f" | Size: {self.last_human_detection.width}x{self.last_human_detection.height}"
                    cv2.putText(result_image, fps_text, (10, 30),
                              cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)
        
        except Exception as e:
            self.get_logger().error(f"Image processing error: {e}")
            traceback.print_exc()
        
        processing_time = time.time() - start_time
        self.processing_times.append(processing_time)
    
    def _draw_debug_overlay(self, image):
        """绘制调试覆盖层"""
        h, w = image.shape[:2]
        cv2.line(image, (w//2, 0), (w//2, h), (255, 0, 0), 1)
        cv2.line(image, (0, h//2), (w, h//2), (255, 0, 0), 1)
        cv2.circle(image, (w//2, h//2), 5, (255, 0, 0), -1)
        
        mode_text = "Mode: "
        if self.pan_tilt_enabled and self.chassis_following_enabled:
            mode_text += "COMBINED"
        elif self.pan_tilt_enabled:
            mode_text += "PAN-TILT"
        elif self.chassis_following_enabled:
            mode_text += "CHASSIS"
        else:
            mode_text += "IDLE"
        cv2.putText(image, mode_text, (w - 200, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 2)
        
        if self.tracker and hasattr(self.tracker, 'smooth_tracker'):
            tracker_status = self.tracker.smooth_tracker.get_status()
            state_text = f"State: {tracker_status.get('state_name', 'UNKNOWN')}"
            cv2.putText(image, state_text, (w - 200, 60), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 2)
            if tracker_status['state'] != TargetState.LOST:
                stability_text = f"Stability: {tracker_status.get('stability_score', 0):.2f}"
                cv2.putText(image, stability_text, (w - 200, 90), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 2)

    # ========================================================================
    # ROS服务回调函数
    # ========================================================================
    
    def enter_srv_callback(self, request, response):
        """进入跟踪模式"""
        self.get_logger().info("ENTER | Starting tracking mode...")
        with self.lock:
            self.is_running = False
            self.threshold = 0.5
            self.color_picker = None
            self.pan_tilt_enabled = False
            self.chassis_following_enabled = False
            self._send_twist(0, 0, 0)
            if self.current_detection_mode == 'body':
                self._publish_servo(1500, 1000)
            elif self.current_detection_mode == 'face':
                self._publish_servo(1500, 1100)
            elif self.current_detection_mode == 'hand':
                self._publish_servo(1500, 1150)
            else:
                self._publish_servo(1500, 1200)
            self.last_linear_x = 0.0
            self.last_angular_z = 0.0
            self._publish_rgb(0, 0, 0)
            self.target_lost_counter = 0
            self.pid_yaw.clear()
            self.pid_dist.clear()
            
            # MODIFIED: 启动图像处理定时器，替代原来的图像订阅
            if self.process_timer is None:
                self.process_timer = self.create_timer(0.033, self._camera_read_and_process)  # ~30Hz
                self.get_logger().info("TIMER | Camera processing timer started")
        
        response.success = True
        response.message = "Entered tracking mode"
        return response
    
    def exit_srv_callback(self, request, response):
        """退出跟踪模式"""
        self.get_logger().info("EXIT | Stopping tracking mode...")
        with self.lock:
            self.is_running = False
            self.color_picker = None
            self.pan_tilt_enabled = False
            self.chassis_following_enabled = False
            self._send_twist(0, 0, 0)
            self._publish_servo(1500, 1500)
            self._publish_rgb(0, 0, 0)
            self.target_lost_counter = 0
            self.control_thread_running = False
            
            # MODIFIED: 停止并销毁定时器
            if self.process_timer is not None:
                self.destroy_timer(self.process_timer)
                self.process_timer = None
                self.get_logger().info("TIMER | Camera processing timer stopped")
        
        response.success = True
        response.message = "Exited tracking mode"
        return response
    
    def set_target_color_srv_callback(self, request, response):
        """设置目标颜色"""
        x, y = request.data.x, request.data.y
        self.get_logger().info(f"SET_COLOR | Setting target at coordinates: ({x:.3f}, {y:.3f})")
        with self.lock:
            if x == -1 and y == -1:
                self.color_picker = None
                self.tracker = None
                self.is_running = False
                self.pan_tilt_enabled = False
                self.chassis_following_enabled = False
                self._send_twist(0, 0, 0)
                self._publish_rgb(0, 0, 0)
                self._publish_servo(1500, 1500)
                self.target_lost_counter = 0
                response.message = "Color target cleared"
            else:
                self.tracker = None
                self.color_picker = ColorPicker(request.data, 10)
                response.message = f"Color picker set at ({x:.3f}, {y:.3f})"
        response.success = True
        return response
    
    def set_large_model_target_color_srv_callback(self, request, response):
        """大模型设置目标颜色"""
        color_name = request.data.lower()
        self.get_logger().info(f"LM_SET | Large model setting color: {color_name}")
        with self.lock:
            valid_colors = ['red', 'blue', 'green', 'black', 'white']
            if color_name not in valid_colors:
                response.success = False
                response.message = f"Invalid color: {color_name}. Valid: {valid_colors}"
                return response
            self.tracker = EnhancedObjectTracker(None, self, color_name, True)
            self.__target_color = color_name
            self.set_model = True
            self.is_running = True
            self.current_detection_mode = 'color'
            color_rgb = {
                'red': (255, 0, 0),
                'blue': (0, 0, 255),
                'green': (0, 255, 0),
                'black': (0, 0, 0),
                'white': (255, 255, 255),
            }
            rgb = color_rgb.get(color_name, (0, 0, 0))
            self._publish_rgb(rgb[0], rgb[1], rgb[2])
            self.chassis_following_enabled = True
            self.pan_tilt_enabled = False
            self.exit_funcation = True
            self.last_target_time = time.time()
            self.target_lost_counter = 0
            response.success = True
            response.message = f"Target color set to {color_name}, chassis tracking enabled"
        return response
    
    def set_detector_callback(self, request, response):
        """设置检测器类型"""
        detector_type = request.data.lower()
        valid_types = ['face', 'body', 'hand']
        if detector_type not in valid_types:
            response.success = False
            response.message = f"Invalid detector type. Valid: {valid_types}"
            return response
        self.get_logger().info(f"SET_DET | Switching to {detector_type} detector")
        with self.lock:
            self.tracker = EnhancedObjectTracker(None, self, detector_type, True)
            if hasattr(self.tracker, '_init_tracker_full'):
                success = self.tracker._init_tracker_full(detector_type)
            else:
                success = False
            if success:
                self.is_running = True
                self.current_detection_mode = detector_type
                self.target_lost_counter = 0
                self.human_following_optimized = (detector_type == 'body')
                color_map = {
                    'face': (0, 255, 0),
                    'body': (255, 0, 0),
                    'hand': (0, 255, 255),
                }
                rgb = color_map.get(detector_type, (255, 255, 255))
                self._publish_rgb(rgb[0], rgb[1], rgb[2])
                if detector_type == 'body':
                    self.chassis_following_enabled = True
                    self.pan_tilt_enabled = False
                    self.get_logger().info("人体跟随模式：启用底盘跟随，云台保持不动")
                else:
                    self.pan_tilt_enabled = True
                    self.chassis_following_enabled = False
                if hasattr(self.tracker, 'servo_x') and hasattr(self.tracker, 'servo_y'):
                    self.current_servo_x = self.tracker.servo_x
                    self.current_servo_y = self.tracker.servo_y
                    self._publish_servo(self.tracker.servo_x, self.tracker.servo_y)
                response.success = True
                response.message = f"Switched to {detector_type} detector"
            else:
                response.success = False
                response.message = f"Failed to initialize {detector_type} detector"
        return response
    
    def set_pan_tilt_callback(self, request, response):
        """设置云台追踪使能"""
        enabled = request.data
        self.get_logger().info(f"SET_PT | Pan-tilt tracking: {'ON' if enabled else 'OFF'}")
        with self.lock:
            self.pan_tilt_enabled = enabled
            if not self.pan_tilt_enabled:
                self._publish_servo(1500, 1500)
                if self.tracker is not None:
                    self.tracker.servo_x_pid.clear()
                    self.tracker.servo_y_pid.clear()
            response.success = True
            response.message = f"Pan-tilt tracking {'enabled' if enabled else 'disabled'}"
        return response
    
    def set_chassis_following_callback(self, request, response):
        """设置车体追踪使能"""
        enabled = request.data
        self.get_logger().info(f"SET_CH | Chassis following: {'ON' if enabled else 'OFF'}")
        with self.lock:
            self.chassis_following_enabled = enabled
            if not self.chassis_following_enabled:
                self._send_twist(0, 0, 0)
                self.pid_yaw.clear()
                self.pid_dist.clear()
            response.success = True
            response.message = f"Chassis following {'enabled' if enabled else 'disabled'}"
        return response
    
    def set_running_srv_callback(self, request, response):
        """设置运行状态"""
        enabled = request.data
        self.get_logger().info(f"SET_RUN | Running state: {'ON' if enabled else 'OFF'}")
        with self.lock:
            self.is_running = enabled
            if not self.is_running:
                self.chassis_following_enabled = False
                self.pan_tilt_enabled = False
                self._send_twist(0, 0, 0)
                self._publish_servo(1500, 1500)
        response.success = True
        response.message = f"Running state set to {enabled}"
        return response
    
    def get_target_color_callback(self, request, response):
        """获取目标颜色"""
        with self.lock:
            if self.tracker is not None and hasattr(self.tracker, 'target_rgb') and self.tracker.target_rgb is not None:
                rgb = self.tracker.target_rgb
                response.success = True
                response.message = "{},{},{}".format(int(rgb[0]), int(rgb[1]), int(rgb[2]))
            else:
                response.success = False
                response.message = "No target color set"
        return response
    
    def set_threshold_srv_callback(self, request, response):
        """设置阈值"""
        threshold = request.data
        self.get_logger().info(f"SET_TH | Setting threshold to: {threshold}")
        with self.lock:
            self.threshold = threshold
        response.success = True
        response.message = f"Threshold set to {threshold}"
        return response
    
    # ========================================================================
    # 辅助函数
    # ========================================================================
    
    def _publish_servo(self, servo_x, servo_y):
        """发布舵机控制指令"""
        try:
            msg = SetPWMServoState()
            state_x = PWMServoState()
            state_x.id = [2]
            state_x.position = [int(servo_x)]
            state_x.offset = [0]
            state_y = PWMServoState()
            state_y.id = [1]
            state_y.position = [int(servo_y)]
            state_y.offset = [0]
            msg.state = [state_x, state_y]
            msg.duration = 0.02
            self.servo_pub.publish(msg)
        except Exception as e:
            self.get_logger().error(f"SERVO_ERR | {e}")
    
    def _send_twist(self, linear_x, linear_y, angular_z):
        """发布底盘速度指令"""
        try:
            twist = Twist()
            twist.linear.x = float(linear_x)
            twist.linear.y = float(linear_y)
            twist.angular.z = float(angular_z)
            self.cmd_vel_pub.publish(twist)
        except Exception as e:
            self.get_logger().error(f"TWIST_ERR | {e}")
    
    def _publish_rgb(self, r, g, b):
        """发布RGB灯控制指令"""
        try:
            msg = RGBStates()
            state1 = RGBState(index=1, red=int(r), green=int(g), blue=int(b))
            state2 = RGBState(index=2, red=int(r), green=int(g), blue=int(b))
            msg.states = [state1, state2]
            self.rgb_pub.publish(msg)
        except Exception as e:
            self.get_logger().error(f"RGB_ERR | {e}")
    
    def destroy_node(self):
        """销毁节点"""
        self.get_logger().info("DESTROY | Shutting down node...")
        self.control_thread_running = False
        if hasattr(self, 'control_thread') and self.control_thread.is_alive():
            self.control_thread.join(timeout=1.0)
        
        # MODIFIED: 清除定时器并释放摄像头
        if self.process_timer is not None:
            self.destroy_timer(self.process_timer)
            self.process_timer = None
        if hasattr(self, 'cap') and self.cap is not None and self.cap.isOpened():
            self.cap.release()
            self.get_logger().info("Camera released")
        
        if self.heart is not None:
            self.heart.destroy()
        super().destroy_node()

# ============================================================================
# 主函数
# ============================================================================

def main():
    rclpy.init()
    node = None
    try:
        node = AITrackingNode('ai_tracking')
        node.get_logger().info("START | AI tracking node started (Camera Direct, tracker_binding)")
        rclpy.spin(node)
    except KeyboardInterrupt:
        if node: node.get_logger().info("STOP | Keyboard interrupt")
    except Exception as e:
        if node: node.get_logger().error(f"ERROR | {e}"), node.get_logger().error(traceback.format_exc())
    finally:
        if node: node.destroy_node()
        rclpy.shutdown()
        print("AI tracking node shutdown complete")

if __name__ == "__main__":
    main()