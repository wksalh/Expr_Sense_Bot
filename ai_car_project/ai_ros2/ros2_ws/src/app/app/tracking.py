#!/usr/bin/env python3
# encoding: utf-8
# 颜色跟踪 + 云台追踪 + 车体追踪 + RGB灯控制（优化版：支持独立云台追踪，大模型触发单车体追踪，云台+车体联合追踪时云台PID控制小车运动，5秒未检测到目标自动退出）

import os
import cv2
import math
import queue
import rclpy
import threading
import numpy as np
import sdk.pid as pid
import sdk.common as common
import sdk.yaml_handle as yaml_handle
from rclpy.node import Node
from app.common import Heart
from cv_bridge import CvBridge
from sensor_msgs.msg import Image
from app.common import ColorPicker
from geometry_msgs.msg import Twist
from std_srvs.srv import SetBool, Trigger
from interfaces.srv import SetPoint, SetFloat64
from large_models_msgs.srv import SetString
from ros_robot_controller_msgs.msg import MotorsState, SetPWMServoState, PWMServoState, RGBState, RGBStates
import time
from rclpy.callback_groups import ReentrantCallbackGroup


class ObjectTracker:
    def __init__(self, color, node, set_color, set_status=False):
        self.node = node
        self.y_stop = 120
        self.pro_size = (640, 480)  # 进一步提高图像处理分辨率，提高检测精度
        self.last_color_circle = None
        self.lost_target_count = 0

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

        self.threshold = 0.3  # 颜色检测阈值 - 提高阈值以提高识别准确性

        # 云台PID参数 - 提高响应速度，保持稳定性
        self.servo_x_pid = pid.PID(P=0.35, I=0.05, D=0.08)  # 提高P和I参数，增加响应速度
        self.servo_y_pid = pid.PID(P=0.35, I=0.05, D=0.08)  # 提高P和I参数，增加响应速度

        # 舵机参数
        self.servo_x = 1500
        self.servo_y = 1500
        self.servo_min_x = 400
        self.servo_max_x = 2600
        self.servo_min_y = 1200
        self.servo_max_y = 1900

        self.pan_tilt_x_threshold = 15  # 减小云台X轴死区，提高响应灵敏度
        self.pan_tilt_y_threshold = 15  # 减小云台Y轴死区，提高响应灵敏度
        
        # 舵机位置平滑处理参数
        self.last_servo_x = 1500
        self.last_servo_y = 1500
        self.servo_smooth_alpha = 0.6  # 舵机位置平滑系数，提高响应速度

    def update_pid(self, x, y, img_w, img_h):
        """更新云台PID，控制舵机位置，并返回PID输出用于小车运动"""
        if abs(x - img_w / 2) < self.pan_tilt_x_threshold:
            x = img_w / 2
        if abs(y - img_h / 2) < self.pan_tilt_y_threshold:
            y = img_h / 2

        # 动态PID调整 - 更保守的参数调整，防止过摇
        error_x = abs(x - img_w / 2)
        error_y = abs(y - img_h / 2)
        
        # 提高所有误差范围的PID参数，增加响应速度
        if error_x > 80:
            self.servo_x_pid.Kp = 0.5
            self.servo_x_pid.Ki = 0.08
            self.servo_x_pid.Kd = 0.12
        elif error_x > 40:
            self.servo_x_pid.Kp = 0.4
            self.servo_x_pid.Ki = 0.06
            self.servo_x_pid.Kd = 0.10
        else:
            self.servo_x_pid.Kp = 0.3
            self.servo_x_pid.Ki = 0.04
            self.servo_x_pid.Kd = 0.08
            
        if error_y > 80:
            self.servo_y_pid.Kp = 0.5
            self.servo_y_pid.Ki = 0.08
            self.servo_y_pid.Kd = 0.12
        elif error_y > 40:
            self.servo_y_pid.Kp = 0.4
            self.servo_y_pid.Ki = 0.06
            self.servo_y_pid.Kd = 0.10
        else:
            self.servo_y_pid.Kp = 0.3
            self.servo_y_pid.Ki = 0.04
            self.servo_y_pid.Kd = 0.08

        # X轴PID（水平方向）
        self.servo_x_pid.SetPoint = img_w / 2
        self.servo_x_pid.update(x)
        servo_x_output = int(self.servo_x_pid.output)
        
        # 限制舵机输出变化幅度，平衡速度和稳定性
        servo_x_output = np.clip(servo_x_output, -40, 40)  # 增大单次输出变化幅度，提高响应速度
        
        self.servo_x += servo_x_output
        self.servo_x = np.clip(self.servo_x, self.servo_min_x, self.servo_max_x)

        # Y轴PID（垂直方向）
        self.servo_y_pid.SetPoint = img_h / 2
        self.servo_y_pid.update(y)
        servo_y_output = int(self.servo_y_pid.output)
        
        # 限制舵机输出变化幅度，平衡速度和稳定性
        servo_y_output = np.clip(servo_y_output, -40, 40)  # 增大单次输出变化幅度，提高响应速度
        
        self.servo_y -= servo_y_output
        self.servo_y = np.clip(self.servo_y, self.servo_min_y, self.servo_max_y)
        
        # 舵机位置平滑处理，防止过摇
        self.servo_x = self.servo_x * self.servo_smooth_alpha + self.last_servo_x * (1 - self.servo_smooth_alpha)
        self.servo_y = self.servo_y * self.servo_smooth_alpha + self.last_servo_y * (1 - self.servo_smooth_alpha)
        
        # 更新上一次舵机位置
        self.last_servo_x = self.servo_x
        self.last_servo_y = self.servo_y

        return self.servo_x, self.servo_y, self.servo_x_pid.output, self.servo_y_pid.output

    def __call__(self, image, result_image, threshold):
        """处理图像，检测目标并返回结果"""
        h, w = image.shape[:2]
        image = cv2.resize(image, self.pro_size)
        
        # 添加光照自适应预处理
        image = self.adaptive_lighting_correction(image)
        
        # 添加直方图均衡化，提高颜色对比度
        image_yuv = cv2.cvtColor(image, cv2.COLOR_RGB2YUV)
        image_yuv[:,:,0] = cv2.equalizeHist(image_yuv[:,:,0])
        image = cv2.cvtColor(image_yuv, cv2.COLOR_YUV2RGB)
        
        image = cv2.cvtColor(image, cv2.COLOR_RGB2LAB)
        image = cv2.GaussianBlur(image, (5, 5), 5)

        # 根据手动或大模型设置的目标颜色确定阈值范围
        if self.set_status == False:
            # 动态调整颜色阈值范围
            min_color, max_color = self.get_adaptive_color_range(self.target_lab, threshold)
            target_color = self.target_lab, min_color, max_color
        else:
            # 使用预设颜色，但添加动态调整
            min_color, max_color = self.get_preset_color_range(self.set_color, threshold)
            target_color = 0, min_color, max_color

        # 图像处理：生成掩膜、腐蚀、膨胀、轮廓检测
        mask = cv2.inRange(image, tuple(target_color[1]), tuple(target_color[2]))
        
        # 添加形态学操作优化
        mask = self.enhance_color_mask(mask)
        
        eroded = cv2.erode(mask, cv2.getStructuringElement(cv2.MORPH_RECT, (3, 3)))
        dilated = cv2.dilate(eroded, cv2.getStructuringElement(cv2.MORPH_RECT, (3, 3)))
        contours = cv2.findContours(dilated, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_NONE)[-2]
        contour_area = map(lambda c: (c, math.fabs(cv2.contourArea(c))), contours)
        contour_area = list(filter(lambda c: c[1] > 40, contour_area))
        circle = None
        target_pos = None
        target_radius = 0

        # 检测到有效轮廓时，选择目标
        if len(contour_area) > 0:
            if self.last_color_circle is None:
                contour, area = max(contour_area, key=lambda c_a: c_a[1])
                circle = cv2.minEnclosingCircle(contour)
            else:
                (last_x, last_y), last_r = self.last_color_circle
                circles = map(lambda c: cv2.minEnclosingCircle(c[0]), contour_area)
                circle_dist = list(map(lambda c: (c, math.sqrt(((c[0][0] - last_x) ** 2) + ((c[0][1] - last_y) ** 2))),
                                       circles))
                circle, dist = min(circle_dist, key=lambda c: c[1])
                if dist < 100:
                    circle = circle

        # 如果检测到目标，绘制圆圈并更新位置
        if circle is not None:
            self.lost_target_count = 0
            (x, y), r = circle
            x = x / self.pro_size[0] * w
            y = y / self.pro_size[1] * h
            r = r / self.pro_size[0] * w
            target_pos = (x, y)
            target_radius = r
            self.last_color_circle = circle

            if self.set_status == False:
                result_image = cv2.circle(result_image, (int(x), int(y)), int(r), (self.target_rgb[0],
                                                                                   self.target_rgb[1],
                                                                                   self.target_rgb[2]), 2)
            else:
                result_image = cv2.circle(result_image, (int(x), int(y)), int(r), self.range_rgb[self.set_color], 2)
        else:
            self.lost_target_count += 1
            if self.lost_target_count > 10:
                self.last_color_circle = None

        return result_image, target_pos, target_radius

    def adaptive_lighting_correction(self, image):
        """光照自适应校正"""
        # 自动白平衡
        image = self.auto_white_balance(image)
        
        # 转换到LAB颜色空间进行光照校正
        lab = cv2.cvtColor(image, cv2.COLOR_RGB2LAB)
        
        # 分离L通道（亮度）
        l, a, b = cv2.split(lab)
        
        # 计算当前图像的平均亮度
        mean_l = np.mean(l)
        
        # 根据平均亮度调整对比度，使用更保守的参数
        if mean_l < 80:  # 暗光环境
            # 适度增强对比度
            clahe = cv2.createCLAHE(clipLimit=2.0, tileGridSize=(8,8))
            l = clahe.apply(l)
        elif mean_l > 200:  # 强光环境
            # 适度降低对比度，避免过曝
            l = cv2.convertScaleAbs(l, alpha=0.9, beta=5)
        
        # 合并通道
        corrected_lab = cv2.merge([l, a, b])
        corrected_image = cv2.cvtColor(corrected_lab, cv2.COLOR_LAB2RGB)
        
        return corrected_image

    def auto_white_balance(self, image):
        """自动白平衡"""
        # 转换到LAB颜色空间
        lab = cv2.cvtColor(image, cv2.COLOR_RGB2LAB)
        l, a, b = cv2.split(lab)
        
        # 计算a和b通道的平均值
        mean_a = np.mean(a)
        mean_b = np.mean(b)
        
        # 计算白平衡增益
        gain_a = 128.0 / (mean_a + 1e-6)
        gain_b = 128.0 / (mean_b + 1e-6)
        
        # 应用白平衡
        a = cv2.convertScaleAbs(a, alpha=gain_a, beta=0)
        b = cv2.convertScaleAbs(b, alpha=gain_b, beta=0)
        
        # 限制值在有效范围内
        a = np.clip(a, 0, 255)
        b = np.clip(b, 0, 255)
        
        # 合并通道
        balanced_lab = cv2.merge([l, a, b])
        balanced_image = cv2.cvtColor(balanced_lab, cv2.COLOR_LAB2RGB)
        
        return balanced_image

    def get_adaptive_color_range(self, target_lab, threshold):
        """获取自适应颜色范围"""
        # 根据当前光照条件动态调整颜色范围，使用更宽松的参数提高识别率
        base_range = 50 * threshold  # 增大基础范围，提高识别率
        
        # 根据L通道值调整范围
        l_value = target_lab[0]
        if l_value < 100:  # 暗色目标
            range_multiplier = 1.2  # 适度扩大范围
        elif l_value > 180:  # 亮色目标
            range_multiplier = 0.9  # 适度缩小范围
        else:
            range_multiplier = 1.0
        
        adjusted_range = base_range * range_multiplier
        
        min_color = [
            int(target_lab[0] - adjusted_range * 2),
            int(target_lab[1] - adjusted_range),
            int(target_lab[2] - adjusted_range)
        ]
        max_color = [
            int(target_lab[0] + adjusted_range * 2),
            int(target_lab[1] + adjusted_range),
            int(target_lab[2] + adjusted_range)
        ]
        
        # 确保值在有效范围内
        min_color = [max(0, min(255, c)) for c in min_color]
        max_color = [max(0, min(255, c)) for c in max_color]
        
        return min_color, max_color

    def get_preset_color_range(self, color_name, threshold):
        """获取预设颜色的动态范围"""
        if color_name not in self.lab_data:
            return [0, 0, 0], [255, 255, 255]
        
        base_min = self.lab_data[color_name]['min']
        base_max = self.lab_data[color_name]['max']
        
        # 根据阈值动态调整范围，使用更宽松的参数提高识别率
        range_adjustment = 20 * threshold  # 增大调整范围，提高识别率
        
        min_color = [
            max(0, base_min[0] - range_adjustment),
            max(0, base_min[1] - range_adjustment),
            max(0, base_min[2] - range_adjustment)
        ]
        max_color = [
            min(255, base_max[0] + range_adjustment),
            min(255, base_max[1] + range_adjustment),
            min(255, base_max[2] + range_adjustment)
        ]
        
        return min_color, max_color

    def enhance_color_mask(self, mask):
        """增强颜色掩膜"""
        # 使用中值滤波去除噪声
        mask = cv2.medianBlur(mask, 5)
        
        # 使用开运算去除小噪点
        kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (5, 5))
        mask = cv2.morphologyEx(mask, cv2.MORPH_OPEN, kernel)
        
        # 使用闭运算填充小孔洞
        mask = cv2.morphologyEx(mask, cv2.MORPH_CLOSE, kernel)
        
        return mask


class OjbectTrackingNode(Node):
    def __init__(self, name):
        rclpy.init()
        super().__init__(name, allow_undeclared_parameters=True,
                         automatically_declare_parameters_from_overrides=True)
        self.name = name
        self.set_callback = False
        self.color_picker = None
        self.is_running = False
        self.set_model = False
        self.__target_color = None
        self.heart = None

        self.pan_tilt_enabled = False  # 云台追踪使能标志
        self.chassis_following_enabled = False  # 车体追踪使能标志
        self.threshold = 0.3  # 提高颜色检测阈值，提高识别精度
        self.color_sensitivity = 1.0  # 颜色识别灵敏度
        self.lighting_mode = "normal"  # 光照模式：dark, normal, bright
        self.auto_white_balance_enabled = True  # 自动白平衡开关
        self.adaptive_threshold_enabled = True  # 自适应阈值开关
        self.lock = threading.RLock()
        self.image_sub = None
        self.result_image = None
        self.image_height = None
        self.image_width = None
        self.bridge = CvBridge()
        self.image_queue = queue.Queue(2)
        self.exit_funcation = False

        self.last_target_time = time.time()  # 记录最后检测到目标的时间

        # 云台
        self.servo_pub = self.create_publisher(SetPWMServoState, 'ros_robot_controller/pwm_servo/set_state', 10)
        self.servo_state = [1500, 1500]

        # 底盘
        self.pid_yaw = pid.PID(0.012, 0.003, 0.0003)  # 降低底盘Yaw轴PID参数，提高稳定性
        self.pid_dist = pid.PID(0.006, 0.003, 0.00005)  # 降低底盘距离PID参数，提高稳定性
        self.x_stop = 320  # 图像中心X坐标
        self.y_stop = 400  # 图像中心Y坐标

        # RGB灯
        self.rgb_pub = self.create_publisher(RGBStates, 'ros_robot_controller/set_rgb', 10)
        self.cmd_vel_pub = self.create_publisher(Twist, 'cmd_vel', 10)

        # 服务和话题
        self.result_publisher = self.create_publisher(Image, '~/image_result', 1)
        self.enter_srv = self.create_service(Trigger, '~/enter', self.enter_srv_callback)
        self.exit_srv = self.create_service(Trigger, '~/exit', self.exit_srv_callback)
        self.set_running_srv = self.create_service(SetBool, '~/set_running', self.set_running_srv_callback)
        self.set_target_color_srv = self.create_service(SetPoint, '~/set_target_color',
                                                       self.set_target_color_srv_callback)
        self.get_target_color_srv = self.create_service(Trigger, '~/get_target_color',
                                                       self.get_target_color_srv_callback)
        self.set_threshold_srv = self.create_service(SetFloat64, '~/set_threshold', self.set_threshold_srv_callback)
        self.set_chassis_following_srv = self.create_service(SetBool, '~/set_chassis_following',
                                                            self.set_chassis_following_callback)
        self.set_pan_tilt_srv = self.create_service(SetBool, '~/set_pan_tilt', self.set_pan_tilt_callback)
        self.create_service(SetString, '~/set_large_model_target_color',
                           self.set_large_model_target_color_srv_callback)
        # SSH环境下的目标颜色设置服务
        self.set_target_color_coord_srv = self.create_service(SetPoint, '~/set_target_color_coordinates',
                                                             self.set_target_color_coordinates_callback)
        # 添加颜色识别优化服务
        self.set_color_sensitivity_srv = self.create_service(SetFloat64, '~/set_color_sensitivity', 
                                                            self.set_color_sensitivity_callback)
        self.set_lighting_mode_srv = self.create_service(SetString, '~/set_lighting_mode',
                                                        self.set_lighting_mode_callback)
        # 添加自动退出控制服务
        self.set_auto_exit_srv = self.create_service(SetBool, '~/set_auto_exit',
                                                    self.set_auto_exit_callback)

        self.tracker = None

        self.last_servo_update = 0
        self.servo_state = [1500, 1500]
        # 创建心跳服务，直接调用退出服务
        self.heart = Heart(self, self.name + '/heartbeat', 5,
                           lambda _: self.exit_srv_callback(request=Trigger.Request(), response=Trigger.Response()))
        self.get_logger().info("心跳服务已创建: /object_tracking/heartbeat")
        self.debug = self.get_parameter('debug').value

        self.create_service(Trigger, '~/init_finish', self.get_node_state)
        self.get_logger().info('\033[1;32m%s\033[0m' % '节点启动')

        # 小车参数 - 平衡响应精度和稳定性
        self.car_x_threshold = 12  # 增大死区阈值，提高稳定性
        self.car_y_threshold = 12  # 增大死区阈值，提高稳定性
        self.target_radius = 100
        self.target_x = 0.5
        self.target_y = 0.5
        self.last_linear_x = 0.0
        self.last_angular_z = 0.0

        # 底盘PID参数 - 平衡响应速度和稳定性
        self.chassis_x_pid = pid.PID(P=0.015, I=0.005, D=0.002)  # 降低P和I参数，提高稳定性
        self.chassis_y_pid = pid.PID(P=0.015, I=0.005, D=0.002)  # 降低P和I参数，提高稳定性
        self.chassis_rot_pid = pid.PID(P=0.015, I=0.0005, D=0.002)  # 降低P参数，提高稳定性

        self.callback_group = ReentrantCallbackGroup()

        self.get_logger().info('调试模式: {}'.format(self.debug))
        if self.debug:
            threading.Thread(target=self.opencv_loop, daemon=True).start()
            display_available = os.environ.get('DISPLAY') is not None
            if display_available:
                self.get_logger().info("OpenCV显示线程已启动（图形界面模式）")
            else:
                self.get_logger().info("OpenCV处理线程已启动（SSH模式）")

        self.exit_allowed = True

    def common_set_range(self, val, vmin, vmax):
        """限制数值范围"""
        return common.set_range(val, vmin, vmax)

    def call_enter_service(self):
        """调用进入服务"""
        cli = self.create_client(Trigger, '/object_tracking/enter')
        while not cli.wait_for_service(timeout_sec=1.0):
            self.get_logger().info('等待进入服务...')
        req = Trigger.Request()
        future = cli.call_async(req)
        future.add_done_callback(self.enter_callback)

    def enter_callback(self, future):
        """进入服务回调"""
        try:
            response = future.result()
            self.get_logger().info('进入服务结果: %s' % response.message)
        except Exception as e:
            self.get_logger().error('服务调用失败: %r' % (e,))

    def set_pan_tilt_callback(self, request, response):
        """设置云台追踪使能"""
        with self.lock:
            self.pan_tilt_enabled = request.data
            if not self.pan_tilt_enabled:
                if self.tracker is not None:
                    self.tracker.servo_x_pid.clear()
                    self.tracker.servo_y_pid.clear()
                self.publish_servo(1500, 1500)
                # 如果车体追踪也被禁用，停止底盘运动
                if not self.chassis_following_enabled:
                    self.send_twist(0, 0, 0)
                    self.pid_yaw.clear()
                    self.pid_dist.clear()
            response.success = True
            response.message = "云台追踪 {}".format("已启用" if request.data else "已禁用")
        self.get_logger().info(response.message)
        return response

    def set_chassis_following_callback(self, request, response):
        """设置车体追踪使能"""
        with self.lock:
            self.chassis_following_enabled = request.data
            if not self.chassis_following_enabled:
                self.send_twist(0, 0, 0)
                self.pid_yaw.clear()
                self.pid_dist.clear()
                # 如果云台追踪也被禁用，停止云台运动
                if not self.pan_tilt_enabled:
                    if self.tracker is not None:
                        self.tracker.servo_x_pid.clear()
                        self.tracker.servo_y_pid.clear()
                    self.publish_servo(1500, 1500)
            response.success = True
            response.message = "车体追踪 {}".format("已启用" if request.data else "已禁用")
        self.get_logger().info(response.message)
        return response

    def get_node_state(self, request, response):
        """获取节点状态"""
        response.success = True
        return response



    def opencv_loop(self):
        """OpenCV显示线程 - SSH兼容版本"""
        # 检查是否有显示环境
        display_available = os.environ.get('DISPLAY') is not None

        if not display_available:
            self.get_logger().info("检测到SSH环境，禁用图形界面显示")
            # SSH环境下只进行图像处理，不显示窗口
            while rclpy.ok():
                try:
                    image = self.image_queue.get(block=True, timeout=0.1)
                    # 在SSH环境下可以保存图像到文件用于调试
                    if self.debug:
                        timestamp = int(time.time())
                        debug_image_path = f"/tmp/tracking_debug_{timestamp}.jpg"
                        cv2.imwrite(debug_image_path, cv2.cvtColor(image, cv2.COLOR_RGB2BGR))
                        self.get_logger().debug(f"调试图像已保存到: {debug_image_path}")
                except queue.Empty:
                    pass
                except Exception as e:
                    self.get_logger().error(f"SSH模式图像处理错误: {e}")
            return

        # 有显示环境时使用原来的OpenCV窗口
        try:
            cv2.namedWindow("result")
            cv2.setMouseCallback("result", self.mouse_callback)
            while rclpy.ok():
                try:
                    image = self.image_queue.get(block=True, timeout=0.1)
                    result = cv2.cvtColor(image, cv2.COLOR_RGB2BGR)
                    cv2.imshow("result", result)
                    k = cv2.waitKey(1)
                    if k == ord('q'):
                        break
                except queue.Empty:
                    pass
                except Exception as e:
                    self.get_logger().error(f"OpenCV线程错误: {e}")
            cv2.destroyAllWindows()
        except Exception as e:
            self.get_logger().error(f"无法创建OpenCV窗口: {e}")
            self.get_logger().info("回退到SSH模式")
            # 如果创建窗口失败，回退到SSH模式
            while rclpy.ok():
                try:
                    image = self.image_queue.get(block=True, timeout=0.1)
                    if self.debug:
                        timestamp = int(time.time())
                        debug_image_path = f"/tmp/tracking_debug_{timestamp}.jpg"
                        cv2.imwrite(debug_image_path, cv2.cvtColor(image, cv2.COLOR_RGB2BGR))
                        self.get_logger().debug(f"调试图像已保存到: {debug_image_path}")
                except queue.Empty:
                    pass
                except Exception as e:
                    self.get_logger().error(f"SSH模式图像处理错误: {e}")

    def mouse_callback(self, event, x, y, flags, param):
        """鼠标点击回调，用于手动选色"""
        if event == cv2.EVENT_LBUTTONDOWN:
            self.get_logger().info("点击坐标 x:{} y:{}".format(x, y))
            msg = SetPoint.Request()
            if self.image_height is not None and self.image_width is not None:
                msg.data.x = x / self.image_width
                msg.data.y = y / self.image_height
                self.set_target_color_srv_callback(msg, SetPoint.Response())

    def set_target_color_by_coordinates(self, x_ratio, y_ratio):
        """通过坐标比例设置目标颜色（SSH环境下的替代方法）"""
        self.get_logger().info("通过坐标设置目标颜色 x:{} y:{}".format(x_ratio, y_ratio))
        msg = SetPoint.Request()
        msg.data.x = x_ratio
        msg.data.y = y_ratio
        self.set_target_color_srv_callback(msg, SetPoint.Response())

    def publish_servo(self, servo_x, servo_y):
        """发布云台舵机控制指令"""
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

    def send_twist(self, linear_x, linear_y, angular_z):
        """发布底盘速度指令"""
        twist = Twist()
        twist.linear.x = float(linear_x)
        twist.linear.y = float(linear_y)
        twist.linear.z = 0.0
        twist.angular.x = 0.0
        twist.angular.y = 0.0
        twist.angular.z = float(angular_z)
        self.cmd_vel_pub.publish(twist)

    def publish_rgb(self, r, g, b):
        """发布RGB灯控制指令"""
        msg = RGBStates()
        state1 = RGBState(index=1, red=int(r), green=int(g), blue=int(b))
        state2 = RGBState(index=2, red=int(r), green=int(g), blue=int(b))
        msg.states = [state1, state2]
        self.rgb_pub.publish(msg)

    def smooth_value(self, current_value, previous_value, alpha):
        """速度平滑处理"""
        return alpha * current_value + (1 - alpha) * previous_value

    def image_callback(self, ros_image):
        """图像处理回调"""
        # 添加时间控制，提高处理频率
        current_time = time.time()
        if hasattr(self, 'last_process_time') and current_time - self.last_process_time < 0.02:  # 50Hz处理频率
            return
        self.last_process_time = current_time
        
        cv_image = self.bridge.imgmsg_to_cv2(ros_image, "rgb8")
        rgb_image = np.array(cv_image, dtype=np.uint8)
        self.image_height, self.image_width = rgb_image.shape[:2]

        result_image = np.copy(rgb_image)
        target_pos = None
        target_radius = 0
        with self.lock:
            # 颜色选择逻辑（保持不变）
            if self.color_picker is not None:
                target_color, result_image = self.color_picker(rgb_image, result_image)
                if target_color is not None:
                    self.color_picker = None
                    self.tracker = ObjectTracker(target_color, self, None, False)
                    self.publish_rgb(target_color[1][0], target_color[1][1], target_color[1][2])
                    self.is_running = True
                    self.set_model = False
            elif self.color_picker is None and self.set_model:
                self.set_model = False
                self.tracker = ObjectTracker(None, self, self.__target_color, True)
                self.get_logger().info("初始化追踪器: {}".format(self.tracker))
            else:
                if self.tracker is not None:
                    try:
                        result_image, target_pos, target_radius = self.tracker(rgb_image, result_image, self.threshold)
                    except Exception as e:
                        self.get_logger().error(str(e))

            # 控制逻辑
            if self.pan_tilt_enabled and not self.chassis_following_enabled:
                # 单独云台追踪
                if target_pos and self.tracker is not None:
                    x, y = target_pos
                    servo_x, servo_y, _, _ = self.tracker.update_pid(x, y, self.image_width, self.image_height)
                    self.publish_servo(servo_x, servo_y)
                    self.last_target_time = time.time()
                else:
                    self.handle_target_lost_pan_tilt()
            elif self.chassis_following_enabled and not self.pan_tilt_enabled:
                # 单独车体追踪
                if target_pos:
                    x, y = target_pos
                    self.control_chassis(x, y, 1500, target_radius)
                    self.publish_servo(1500, 1600)
                    self.last_target_time = time.time()
                else:
                    self.handle_target_lost_chassis()
                    # 车体追踪模式下也启用自动退出
                    if time.time() - self.last_target_time > 10 and self.exit_funcation:  # 延长到10秒
                        self.get_logger().info("10秒未检测到目标，自动退出")
                        self.exit_funcation = False
                        self.exit_srv_callback(Trigger.Request(), Trigger.Response())
            elif self.pan_tilt_enabled and self.chassis_following_enabled:
                # 云台+车体联合追踪：云台PID输出控制小车运动
                if target_pos and self.tracker is not None:
                    x, y = target_pos
                    servo_x, servo_y, pid_x_output, pid_y_output = self.tracker.update_pid(x, y, self.image_width, self.image_height)
                    self.publish_servo(servo_x, servo_y)  # 云台继续跟踪
                    # 使用云台PID输出控制小车：X轴输出控制左右转，Y轴输出控制前后运动
                    angular_z = common.set_range(pid_x_output * 0.02, -3.0, 3.0)  # 降低系数，提高稳定性
                    linear_x = common.set_range(pid_y_output * 0.02, -0.8, 0.8)  # 降低系数，提高稳定性
                    # 优化平滑速度 - 增大平滑系数，提高稳定性
                    linear_x = self.smooth_value(linear_x, self.last_linear_x, 0.6)  # 增大平滑系数，提高稳定性
                    angular_z = self.smooth_value(angular_z, self.last_angular_z, 0.6)  # 增大平滑系数，提高稳定性
                    self.last_linear_x = linear_x
                    self.last_angular_z = angular_z
                    self.send_twist(linear_x, 0.0, angular_z)
                    self.last_target_time = time.time()
                else:
                    self.handle_target_lost_both()
                    # 云台+车体联合追踪模式下也启用自动退出
                    if time.time() - self.last_target_time > 10 and self.exit_funcation:  # 延长到10秒
                        self.get_logger().info("10秒未检测到目标，自动退出")
                        self.exit_funcation = False
                        self.exit_srv_callback(Trigger.Request(), Trigger.Response())
            elif not self.pan_tilt_enabled and not self.chassis_following_enabled:
                # 追踪已禁用，停止所有运动
                self.send_twist(0, 0, 0)
                self.publish_servo(1500, 1500)
                self.pid_yaw.clear()
                self.pid_dist.clear()
                # 重置目标时间，避免自动退出
                self.last_target_time = time.time()
            else:
                # 其他情况，停止所有运动
                self.send_twist(0, 0, 0)
                self.publish_servo(1500, 1500)
                self.pid_yaw.clear()
                self.pid_dist.clear()

        self.result_publisher.publish(self.bridge.cv2_to_imgmsg(result_image, "rgb8"))
        if self.debug:
            if self.image_queue.full():
                try:
                    self.image_queue.get_nowait()
                except queue.Empty:
                    pass
            try:
                self.image_queue.put_nowait(result_image)
            except queue.Full:
                pass


    def handle_target_lost_pan_tilt(self):
        """处理云台追踪目标丢失，保持云台位置"""
        if self.tracker is not None:
            self.publish_servo(self.tracker.servo_x, self.tracker.servo_y)
        else:
            self.publish_servo(1500, 1500)  # 使用默认位置

    def handle_target_lost_chassis(self):
        """处理车体追踪目标丢失，停止底盘运动"""
        self.send_twist(0.0, 0.0, 0.0)
        self.pid_yaw.clear()
        self.pid_dist.clear()

    def handle_target_lost_both(self):
        """处理云台+车体追踪目标丢失，停止底盘并保持云台位置"""
        self.send_twist(0.0, 0.0, 0.0)
        if self.tracker is not None:
            self.publish_servo(self.tracker.servo_x, self.tracker.servo_y)
        else:
            self.publish_servo(1500, 1500)  # 使用默认位置
        self.pid_yaw.clear()
        self.pid_dist.clear()

    def control_chassis(self, x, y, servo_x, target_radius):
        """控制底盘运动（单独车体追踪模式）"""
        yaw_error = x - self.x_stop
        dist_error = y - self.y_stop

        self.pid_yaw.update(yaw_error)
        self.pid_dist.update(dist_error)

        angular_z = common.set_range(self.pid_yaw.output, -4.0, 4.0)
        linear_x = common.set_range(self.pid_dist.output, -1.0, 1.0)

        if abs(linear_x) < 0.05:  # 增大死区阈值，提高稳定性
            linear_x = 0.0
        if abs(angular_z) < 0.08:  # 增大死区阈值，提高稳定性
            angular_z = 0.0

        # 优化平滑速度 - 增大平滑系数，提高稳定性
        linear_x = self.smooth_value(linear_x, self.last_linear_x, 0.6)  # 增大平滑系数
        angular_z = self.smooth_value(angular_z, self.last_angular_z, 0.6)  # 增大平滑系数

        self.last_linear_x = linear_x
        self.last_angular_z = angular_z

        self.send_twist(linear_x, 0.0, angular_z)

    def enter_srv_callback(self, request, response):
        """进入服务回调"""
        self.get_logger().info('\033[1;32m物体追踪进入\033[0m')
        with self.lock:
            self.is_running = False
            self.threshold = 0.5
            self.tracker = None
            self.color_picker = None
            self.pan_tilt_enabled = False
            self.chassis_following_enabled = False
            self.pid_yaw.clear()
            self.pid_dist.clear()
            self.publish_servo(1500, 1600)
            self.last_linear_x = 0.0
            self.last_angular_z = 0.0
            self.publish_rgb(0, 0, 0)
            self.exit_allowed = True
            self.exit_funcation = False  # 默认禁用自动退出功能

            if self.image_sub is None:
                self.image_sub = self.create_subscription(Image, 'image_raw', self.image_callback, 1,
                                                         callback_group=self.callback_group)
            self.chassis_x_pid.clear()
            self.chassis_y_pid.clear()
            self.chassis_rot_pid.clear()
            
            # 心跳服务已在初始化时创建，无需重新创建

        response.success = True
        response.message = "进入成功"
        return response

    def exit_srv_callback(self, request, response):
        """退出服务回调"""
        self.get_logger().info('\033[1;32m物体追踪退出\033[0m')
        with self.lock:
            if not self.exit_allowed:
                self.get_logger().info("退出被阻止")
                response.success = True
                response.message = "退出被阻止"
                return response

            self.exit_allowed = False
            try:
                if self.image_sub is not None:
                    self.destroy_subscription(self.image_sub)
                    self.image_sub = None
            except Exception as e:
                self.get_logger().error(str(e))

            self.is_running = False
            self.color_picker = None
            self.tracker = None
            self.pan_tilt_enabled = False
            self.chassis_following_enabled = False
            self.send_twist(0, 0, 0)
            self.pid_yaw.clear()
            self.pid_dist.clear()
            self.threshold = 0.5
            self.publish_rgb(0, 0, 0)
            self.publish_servo(1500, 1500)
            self.last_linear_x = 0.0
            self.last_angular_z = 0.0

            if self.heart is not None:
                self.heart.destroy()
                self.heart = None
                self.get_logger().info("心跳服务已销毁")
            else:
                self.get_logger().info("心跳服务不存在，无需销毁")

        response.success = True
        response.message = "退出成功"
        return response

    def set_target_color_srv_callback(self, request, response):
        """设置手动目标颜色"""
        self.get_logger().info('\033[1;32m设置目标颜色\033[0m')
        with self.lock:
            self.set_model = False
            x, y = request.data.x, request.data.y
            if x == -1 and y == -1:
                # 清除目标颜色，停止追踪
                self.color_picker = None
                self.tracker = None
                self.is_running = False
                self.pan_tilt_enabled = False
                self.chassis_following_enabled = False
                self.send_twist(0, 0, 0)
                self.pid_yaw.clear()
                self.pid_dist.clear()
                self.publish_rgb(0, 0, 0)
                self.publish_servo(1500, 1500)
            else:
                self.tracker = None
                self.color_picker = ColorPicker(request.data, 10)
        response.success = True
        response.message = "目标颜色设置成功"
        return response

    def set_large_model_target_color_srv_callback(self, request, response):
        """大模型设置目标颜色（单车体追踪，5秒未检测到目标自动退出）"""
        self.get_logger().info('\033[1;32m大模型设置目标颜色\033[0m')
        with self.lock:
            target_color_name = request.data.lower()
            if target_color_name not in ObjectTracker(None, self, None, True).range_rgb:
                response.success = False
                response.message = "无效颜色: '{}'. 有效颜色: {}".format(
                    target_color_name, ", ".join(ObjectTracker(None, self, None, True).range_rgb.keys()))
                return response

            self.tracker = ObjectTracker(None, self, target_color_name, True)
            self.__target_color = target_color_name
            self.set_model = True
            self.is_running = True
            self.tracker.set_color = target_color_name
            self.tracker.set_status = True

            self.chassis_following_enabled = True  # 仅启用车体追踪
            self.pan_tilt_enabled = False  # 禁用云台追踪
            self.exit_funcation = True
            self.last_target_time = time.time()  # 重置目标检测时间

            self.get_logger().info("车体追踪已启用")
            self.publish_rgb(*ObjectTracker(None, self, None, True).range_rgb[target_color_name])

            response.success = True
            response.message = "目标颜色设置为 '{}', 车体追踪已启用".format(target_color_name)
        return response

    def get_target_color_srv_callback(self, request, response):
        """获取当前目标颜色"""
        self.get_logger().info('\033[1;32m获取目标颜色\033[0m')
        response.success = False
        response.message = "获取目标颜色"
        with self.lock:
            if self.tracker is not None and hasattr(self.tracker, 'target_rgb') and self.tracker.target_rgb is not None:
                response.success = True
                rgb = self.tracker.target_rgb
                response.message = "{},{},{}".format(int(rgb[0]), int(rgb[1]), int(rgb[2]))
        return response

    def set_running_srv_callback(self, request, response):
        """设置运行状态"""
        self.get_logger().info('\033[1;32m设置运行状态\033[0m')
        with self.lock:
            self.is_running = request.data
            if self.is_running:
                self.chassis_following_enabled = True
            else:
                # 如果停止运行，禁用所有追踪
                self.chassis_following_enabled = False
                self.pan_tilt_enabled = False
                self.send_twist(0, 0, 0)
                self.pid_yaw.clear()
                self.pid_dist.clear()
                if self.tracker is not None:
                    self.tracker.servo_x_pid.clear()
                    self.tracker.servo_y_pid.clear()
                self.publish_servo(1500, 1500)
        response.success = True
        response.message = "运行状态设置成功"
        return response

    def set_threshold_srv_callback(self, request, response):
        """设置颜色阈值"""
        self.get_logger().info('\033[1;32m设置阈值\033[0m')
        with self.lock:
            self.threshold = request.data
        response.success = True
        response.message = "阈值设置成功"
        return response

    def set_target_color_coordinates_callback(self, request, response):
        """SSH环境下通过坐标设置目标颜色服务回调"""
        try:
            x_ratio = request.data.x
            y_ratio = request.data.y
            self.set_target_color_by_coordinates(x_ratio, y_ratio)
            response.success = True
            response.message = f"通过坐标设置目标颜色: x={x_ratio:.3f}, y={y_ratio:.3f}"
            self.get_logger().info(response.message)
        except Exception as e:
            response.success = False
            response.message = f"通过坐标设置目标颜色失败: {str(e)}"
            self.get_logger().error(response.message)
        return response

    def set_color_sensitivity_callback(self, request, response):
        """设置颜色识别灵敏度"""
        self.get_logger().info('\033[1;32m设置颜色识别灵敏度\033[0m')
        with self.lock:
            self.threshold = request.data
        response.success = True
        response.message = "颜色识别灵敏度设置成功"
        return response

    def set_lighting_mode_callback(self, request, response):
        """设置光照模式"""
        self.get_logger().info('\033[1;32m设置光照模式\033[0m')
        with self.lock:
            mode = request.data.lower()
            if mode == "dark":
                self.threshold = 0.1
                self.get_logger().info("设置为暗光模式，阈值: 0.1")
            elif mode == "normal":
                self.threshold = 0.5
                self.get_logger().info("设置为正常模式，阈值: 0.5")
            elif mode == "bright":
                self.threshold = 1.0
                self.get_logger().info("设置为强光模式，阈值: 1.0")
            else:
                response.success = False
                response.message = "无效光照模式: '{}'. 有效模式: dark, normal, bright".format(mode)
                return response
        response.success = True
        response.message = "光照模式设置成功"
        return response

    def set_auto_exit_callback(self, request, response):
        """设置自动退出功能"""
        self.get_logger().info('\033[1;32m设置自动退出功能\033[0m')
        with self.lock:
            self.exit_funcation = request.data
            if self.exit_funcation:
                self.get_logger().info("自动退出功能已启用")
            else:
                self.get_logger().info("自动退出功能已禁用")
        response.success = True
        response.message = "自动退出功能设置成功"
        return response



    def destroy_node(self):
        """销毁节点"""
        if self.heart is not None:
            self.heart.destroy()
            self.heart = None
        super().destroy_node()


def main():
    node = OjbectTrackingNode('object_tracking')
    try:
        rclpy.spin(node)
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":
    main()
