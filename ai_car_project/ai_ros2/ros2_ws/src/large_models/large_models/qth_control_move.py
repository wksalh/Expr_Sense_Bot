#!/usr/bin/env python3
# encoding: utf-8


import sys
import os
import time
import threading
import ctypes
import signal
import rclpy
import cv2
from cv_bridge import CvBridge
from sensor_msgs.msg import Image
from rclpy.node import Node
from geometry_msgs.msg import Twist
from ros_robot_controller_msgs.msg import RGBState, RGBStates, SetPWMServoState, PWMServoState
from std_srvs.srv import Trigger
from large_models_msgs.srv import SetString
import subprocess


# 强制刷新输出缓冲区，确保在launch环境中打印信息能立即显示
def print_flush(*args, **kwargs):
    print(*args, **kwargs)
    sys.stdout.flush()

# 全局变量用于信号处理
shutdown_requested = False

def signal_handler(signum, frame):
    """信号处理函数"""
    global shutdown_requested
    print_flush(f"\n收到信号 {signum}，正在退出...")
    shutdown_requested = True

# 添加当前目录到路径
current_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.append(current_dir)

# 尝试加载C库
try:
    # 尝试加载编译好的共享库
    # 从当前工作目录到install目录的相对路径
    lib_path = os.path.join('install', 'large_models', 'lib', 'libposix_shm_manager.so')
    print(f"尝试加载库文件: {lib_path}")
    if os.path.exists(lib_path):
        lib = ctypes.CDLL(lib_path)
        SHARED_MEMORY_AVAILABLE = True
        print(f"成功加载共享库: {lib_path}")
    else:
        # 如果找不到编译好的库，使用模拟模式
        lib = None
        SHARED_MEMORY_AVAILABLE = False
        print_flush(f"警告: 未找到编译好的共享库")
except Exception as e:
    lib = None
    SHARED_MEMORY_AVAILABLE = False
    print_flush(f"警告: 无法加载共享库: {e}")

# 如果库可用，设置函数签名
if lib:
    # 设置函数返回类型
    lib.posix_shm_manager_init.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_bool]
    lib.posix_shm_manager_init.restype = ctypes.c_bool
    
    lib.posix_shm_manager_cleanup.argtypes = [ctypes.c_void_p]
    lib.posix_shm_manager_cleanup.restype = None
    
    lib.posix_shm_write_control_data.argtypes = [ctypes.c_void_p, ctypes.c_int32, ctypes.c_int32, ctypes.c_bool]
    lib.posix_shm_write_control_data.restype = ctypes.c_bool
    
    
    lib.posix_shm_signal_data_ready.argtypes = [ctypes.c_void_p]
    lib.posix_shm_signal_data_ready.restype = ctypes.c_bool
    
    lib.posix_shm_wait_for_data.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_int32), ctypes.POINTER(ctypes.c_int32), ctypes.POINTER(ctypes.c_bool)]
    lib.posix_shm_wait_for_data.restype = ctypes.c_bool
    
    lib.posix_shm_get_error_string.restype = ctypes.c_char_p

class PosixShmWrapper:
    """
    POSIX共享内存包装器
    提供Python接口访问C库功能
    """
    
    def __init__(self):
        self.lib = lib
        self.manager = None
        self.initialized = False
    
    def init_manager(self, shm_name, create_new=False):
        """
        初始化共享内存管理器
        
        Args:
            shm_name: 共享内存名称
            create_new: 是否创建新的共享内存
        """
        if self.lib is None:
            self.initialized = True
            return True
        
        # 创建管理器结构体 - 使用正确的结构体类型
        # 定义PosixShmManager结构体
        class PosixShmManager(ctypes.Structure):
            _fields_ = [
                ("shm_name", ctypes.c_char * 256),
                ("sem_name", ctypes.c_char * 256),
                ("shm_fd", ctypes.c_int),
                ("data", ctypes.c_void_p),
                ("semaphore", ctypes.c_void_p),
                ("initialized", ctypes.c_bool)
            ]
        
        self.manager = PosixShmManager()
        
        # 调用C库初始化函数
        result = self.lib.posix_shm_manager_init(
            ctypes.byref(self.manager),
            shm_name.encode('utf-8'),
            create_new
        )
        
        if result:
            self.initialized = True
            print(f"POSIX共享内存管理器初始化成功: {shm_name}")
        else:
            # 不调用C库函数获取错误信息，避免段错误
            print("POSIX共享内存管理器初始化失败")
        
        return result
    
    def cleanup_manager(self):
        """清理共享内存管理器"""
        if self.lib is None:
            return
        
        if self.initialized:
            # 不调用C库清理函数，避免ctypes对象清理时的段错误
            # 让操作系统自动清理共享内存和信号量资源
            self.initialized = False
            # 不设置self.manager = None，避免ctypes对象清理问题
            print("POSIX共享内存管理器已清理")
    
    def wait_for_data(self):
        """
        阻塞等待数据（推荐使用，不浪费CPU）
        
        Returns:
            tuple: (action, times, valid) 或 None
        """
        if self.lib is None:
            return (1, 1000, True)
        
        if not self.manager or not self.initialized:
            raise RuntimeError("共享内存管理器未初始化")
        
        action = ctypes.c_int32()
        times = ctypes.c_int32()
        valid = ctypes.c_bool()
        
        try:
            result = self.lib.posix_shm_wait_for_data(
                ctypes.byref(self.manager),
                ctypes.byref(action),
                ctypes.byref(times),
                ctypes.byref(valid)
            )
            
            if result:
                return (action.value, times.value, valid.value)
            else:
                return None
        except Exception as e:
            print(f"阻塞等待POSIX数据时出错: {e}")
            return None


class PosixControlReader:
    """
    POSIX控制数据读取器
    真正等待信号量通知，而不是轮询
    """
    
    def __init__(self, callback=None, shm_name="/ros2_control_shm"):
        """
        初始化POSIX信号量等待读取器
        
        Args:
            callback: 数据回调函数，接收(action, times, valid)参数
            shm_name: 共享内存名称
        """
        self.shm = PosixShmWrapper()
        print_flush(f"正在初始化PosixControlReader，shm_name={shm_name}")
        self.callback = callback
        self.running = False
        self.thread = None
        self.last_data = (0, 0, False)
        
        # 尝试初始化，如果失败则等待
        result = self.shm.init_manager(shm_name, False)
        print_flush(f"PosixShmWrapper初始化结果: {result}")
        
        if result:
            print_flush("POSIX控制读取器初始化完成")
            print_flush("POSIX控制读取器初始化成功")
        else:
            print_flush("POSIX控制读取器初始化失败，等待共享内存创建...")
            # 等待共享内存被创建
            self._wait_for_shared_memory(shm_name)
    
    def _wait_for_shared_memory(self, shm_name):
        """阻塞等待直到共享内存被创建并初始化成功；收到退出信号则中止。"""
        retry_count = 0
        while not shutdown_requested:
            retry_count += 1
            print(f"等待共享内存创建... (第{retry_count}次)")
            time.sleep(1)
            
            # 检查共享内存文件是否存在
            shm_path = f"/dev/shm{shm_name}"
            if os.path.exists(shm_path):
                print("检测到共享内存文件，尝试初始化...")
                # 清理之前的实例
                if hasattr(self.shm, 'cleanup_manager'):
                    try:
                        self.shm.cleanup_manager()
                    except Exception:
                        pass
                # 重新创建PosixShmWrapper实例
                self.shm = PosixShmWrapper()
                result = self.shm.init_manager(shm_name, False)
                if result:
                    print("共享内存已创建，POSIX控制读取器初始化成功")
                    return True
                else:
                    print("共享内存文件存在但初始化失败，继续等待...")
        
        print("收到退出信号，停止等待共享内存初始化")
        return False
    
    def start(self):
        """启动信号量等待线程"""
        if self.running:
            return
        
        self.running = True
        self.thread = threading.Thread(target=self._wait_loop, daemon=True)
        self.thread.start()
        print("POSIX控制读取器已启动")
    
    def stop(self):
        """停止信号量等待线程"""
        self.running = False
        if self.thread:
            self.thread.join()
        print("POSIX控制读取器已停止")
    
    def _wait_loop(self):
        """阻塞等待循环（不浪费CPU）"""
        while self.running and not shutdown_requested:
            try:
                # 检查是否还在运行
                if not self.running or shutdown_requested:
                    break
                    
                # 使用阻塞等待数据（推荐方式，不浪费CPU）
                # print_flush("DEBUG: 等待POSIX信号量通知...")
                data = self.shm.wait_for_data()
                if data is not None:
                    action, times, valid = data
                    # print_flush(f"DEBUG: 收到数据: action={action}, times={times}, valid={valid}")
                    
                    # 处理所有命令，不检查重复数据
                    self.last_data = data
                    
                    if valid and self.callback:
                        self.callback(action, times, valid)
                    elif valid:
                        print_flush(f"收到POSIX信号量通知，执行控制命令: action={action}, times={times}s")
                    else:
                        print_flush(f"收到无效数据: action={action}, times={times}, valid={valid}")
                    
                    # print_flush("DEBUG: 处理完数据，立即重新开始等待...")
                else:
                    # print_flush("DEBUG: 没有收到数据")
                    pass
                
                # 立即重新开始等待，不添加任何延迟
                # 这确保读取器能及时接收下一次数据
                
            except Exception as e:
                if self.running and not shutdown_requested:  # 只有在还在运行时才打印错误
                    print_flush(f"等待POSIX数据时出错: {e}")
                break  # 出错时退出循环
    
    def cleanup(self):
        """清理资源"""
        self.shm.cleanup_manager()

class PosixControlReaderApp(Node):
    """
    POSIX控制数据读取器应用
    用于接收信号量，读取共享内存进行控制
    """
    
    def __init__(self, shm_name="/ros2_control_shm"):
        """
        初始化读取器
        
        Args:
            shm_name: 共享内存名称
        """
        # 初始化ROS2节点（如果尚未初始化）
        try:
            rclpy.init()
        except RuntimeError:
            # ROS2已经初始化过了，忽略错误
            pass
        super().__init__('posix_control_reader')
        
        self.reader = None
        self.running = False
        self.command_count = 0
        self.shm_name = shm_name
        # 图像订阅（获取其他节点的摄像头画面）
        self.bridge = CvBridge()
        self.last_bgr_image = None
        self.last_image_time = 0.0
        
        # 动作执行控制：使用锁和线程来避免阻塞和并发问题
        self.action_lock = threading.Lock()
        self.current_action_thread = None
        self.action_cancel_flag = threading.Event()
        
        # 创建ROS2发布器
        self.mecanum_pub = self.create_publisher(Twist, 'raw_cmd_vel', 1)  # 底盘控制
        self.pwm_pub = self.create_publisher(SetPWMServoState, 'ros_robot_controller/pwm_servo/set_state', 10)    
        self.sonar_rgb_pub = self.create_publisher(RGBStates, 'sonar_controller/set_rgb', 10)   
        self.rgb_pub = self.create_publisher(RGBStates, 'ros_robot_controller/set_rgb', 10)
        # 订阅摄像头话题
        try:
            self.create_subscription(Image, 'image_raw', self._image_callback, 1)
            print_flush("已订阅摄像头话题: image_raw")
        except Exception as e:
            print_flush(f"警告: 订阅image_raw失败: {e}")
        
        # 等待发布器连接
        time.sleep(0.5)  # 给发布器一些时间初始化
        print_flush(f"ROS2发布器已创建: raw_cmd_vel, pwm_servo, rgb_controllers")
        
        # 检查发布器状态
        print_flush(f"  raw_cmd_vel发布器订阅者数量: {self.mecanum_pub.get_subscription_count()}")
        print_flush(f"  pwm_servo发布器订阅者数量: {self.pwm_pub.get_subscription_count()}")
        print_flush(f"  sonar_rgb发布器订阅者数量: {self.sonar_rgb_pub.get_subscription_count()}")
        print_flush(f"  rgb发布器订阅者数量: {self.rgb_pub.get_subscription_count()}")
        
        if SHARED_MEMORY_AVAILABLE:
            try:
                self.reader = PosixControlReader(self.control_callback, shm_name)
                print_flush("POSIX控制读取器初始化成功")
            except Exception as e:
                print_flush(f"POSIX控制读取器初始化失败: {e}")
                sys.exit(1)

    def _capture_and_notify(self):
        """抓拍一张图片到 /tmp，并通过POSIX信号量通知其他进程取图。"""
        if cv2 is None:
            print_flush("错误: 未安装opencv-python，无法抓拍")
            return

        # 从已订阅的image_raw获取“此时”的画面（不占用摄像头设备）
        start_ts = time.time()
        timeout_s = 3.0
        poll_interval_s = 0.02
        target_image = None
        # 优先等待产生比start_ts新的帧
        end_ts = start_ts + timeout_s
        while time.time() < end_ts:
            if self.last_bgr_image is not None and self.last_image_time >= start_ts:
                target_image = self.last_bgr_image
                break
            time.sleep(poll_interval_s)
        # 回退：接受最近5秒内的最新帧
        if target_image is None and self.last_bgr_image is not None:
            if (time.time() - self.last_image_time) <= 5.0:
                target_image = self.last_bgr_image
        if target_image is None:
            print_flush("错误: 未从image_raw获取到有效图像")
            return

        timestamp = int(time.time())
        # 采用固定路径，便于被通知方直接读取
        filename = "/tmp/qth_shot_latest.jpg"
        try:
            saved = cv2.imwrite(filename, target_image)
        except Exception as e:
            saved = False
            print_flush(f"错误: 保存图片失败: {e}")

        if not saved:
            print_flush("错误: 无法保存抓拍图片")
            return

        # 创建标志文件通知截图完成
        try:
            flag_file = "/tmp/screenshot_done.flag"
            with open(flag_file, 'w') as f:
                f.write(f"screenshot_completed_{timestamp}")
            print_flush(f"截图完成: {filename}，已创建标志文件: {flag_file}")
        except Exception as e:
            print_flush(f"警告: 创建标志文件失败: {e}")

    def _image_callback(self, ros_image: Image):
        try:
            bgr = self.bridge.imgmsg_to_cv2(ros_image, desired_encoding='bgr8')
            self.last_bgr_image = bgr
            self.last_image_time = time.time()
        except Exception as e:
            print_flush(f"警告: 转换图像失败: {e}")
        
    def control_callback(self, action, times, valid):
        """
        控制回调函数
        当收到新的控制数据时被调用
        
        Args:
            action (int): 动作值
            times (int): 执行时间（秒）
            valid (bool): 数据是否有效
        """
        if not valid:
            return
        
        # 直接执行所有命令，不检查重复
        self.command_count += 1
        
        print_flush(f"\n[命令 {self.command_count}] 收到POSIX信号量通知，读取到控制指令:")
        print_flush(f"  动作: {action}")
        print_flush(f"  时间: {times}s")
        
        # 如果正在执行动作，设置取消标志让旧线程自己停止
        # 注意：不等待线程结束，让回调快速返回，避免阻塞信号量等待循环
        with self.action_lock:
            if self.current_action_thread is not None and self.current_action_thread.is_alive():
                self.action_cancel_flag.set()  # 设置取消标志，旧线程会检测并自己停止
        
        # 在新线程中执行控制动作，避免阻塞回调
        # 注意：不等待上一个线程结束，让回调快速返回，避免阻塞信号量等待循环
        self.action_cancel_flag.clear()  # 确保取消标志已清除
        
        # 快速创建并启动线程，减少锁持有时间
        action_thread = threading.Thread(
            target=self._execute_control_action_threaded,
            args=(action, times),
            daemon=True
        )
        action_thread.start()
        
        # 快速更新线程引用，使用非阻塞方式
        try:
            if self.action_lock.acquire(blocking=False):
                try:
                    self.current_action_thread = action_thread
                finally:
                    self.action_lock.release()
            else:
                # 如果获取不到锁，直接设置（说明上一个线程已经结束或即将结束）
                self.current_action_thread = action_thread
        except Exception as e:
            print_flush(f"  警告: 更新动作线程引用时出错: {e}")
            self.current_action_thread = action_thread
    
    def pwm_controller(self, *position_data):
        """PWM舵机控制器"""
        pwm_list = []
        msg = SetPWMServoState()
        msg.duration = 0.2
        for position in position_data:
            pos = PWMServoState()
            pos.id = [position[0]]
            pos.position = [int(position[1])]
            pwm_list.append(pos)
        msg.state = pwm_list
        self.pwm_pub.publish(msg)
    
    def sonar_rgb_controller(self, r, g, b):
        """RGB灯光控制器"""
        def create_led(index):
            led = RGBState()
            led.index = index
            led.red = r
            led.green = g
            led.blue = b
            return led
        # 发布给 sonar_rgb_pub
        msg1 = RGBStates()
        msg1.states = [create_led(0), create_led(1)]
        self.sonar_rgb_pub.publish(msg1)
    
        # 发布给 rgb_pub
        msg2 = RGBStates()
        msg2.states = [create_led(1), create_led(2)]
        self.rgb_pub.publish(msg2)

    def _stop(self):
        """停止小车，发布停止命令"""
        stop_twist = Twist()
        subscriber_count = self.mecanum_pub.get_subscription_count()
        if subscriber_count > 0:
            self.mecanum_pub.publish(stop_twist)
            print_flush("  已发布停止命令")
        else:
            print_flush("  警告: 没有订阅者，停止命令可能不会被处理")
    
    def _wait_for_subscriber(self, max_wait=1.0):
        """等待订阅者连接"""
        subscriber_count = self.mecanum_pub.get_subscription_count()
        if subscriber_count == 0:
            print_flush("  等待订阅者连接...")
            wait_start = time.time()
            for i in range(int(max_wait * 10)):
                if self.action_cancel_flag.is_set():
                    print_flush("  等待订阅者时收到取消信号")
                    return False
                time.sleep(0.1)
                subscriber_count = self.mecanum_pub.get_subscription_count()
                if subscriber_count > 0:
                    wait_elapsed = time.time() - wait_start
                    print_flush(f"  订阅者已连接，数量: {subscriber_count} (等待耗时: {wait_elapsed:.3f}s)")
                    return True
                if i % 5 == 0:  # 每0.5秒打印一次
                    print_flush(f"  等待中... ({i+1}/{int(max_wait * 10)})")
        return self.mecanum_pub.get_subscription_count() > 0

    # 跟踪服务发布
    def start_tracking(self):
        """启动AI跟踪"""
        # 调用进入跟踪
        client1 = self.create_client(Trigger, '/ai_tracking/enter')
        client1.wait_for_service(timeout_sec=5.0)
        client1.call_async(Trigger.Request())
        
        # 设置检测器为body
        client2 = self.create_client(SetString, '/ai_tracking/set_detector')
        client2.wait_for_service(timeout_sec=5.0)
        req = SetString.Request()
        req.data = 'body'
        client2.call_async(req)

    def _execute_twist_with_duration(self, twist, duration, min_duration=None, action_name="", wait_subscriber=False):
        """
        执行Twist命令并等待指定时间后停止
        
        Args:
            twist: Twist消息对象
            duration: 执行时间（秒）
            min_duration: 最小执行时间（秒），如果指定则使用max(duration, min_duration)
            action_name: 动作名称，用于日志
            wait_subscriber: 是否等待订阅者连接
        """
        actual_duration = max(duration, min_duration) if min_duration else duration
        
        if wait_subscriber:
            self._wait_for_subscriber()
        
        # 发布Twist消息
        self.mecanum_pub.publish(twist)
        print_flush(f"  消息已发布，等待 {actual_duration}s...")
        
        # 精确时间控制，并检查取消标志
        start_time = time.time()
        while time.time() - start_time < actual_duration:
            if self.action_cancel_flag.is_set():
                print_flush("  动作被取消")
                break
            time.sleep(0.1)
        
        # 无论是否被取消，都要发布停止命令
        # 如果被取消，立即停止；如果正常完成，也要停止
        try:
            self._stop()
        except Exception as e:
            print_flush(f"  错误: _stop() 执行失败: {e}")
            import traceback
            traceback.print_exc()
    
    def _execute_control_action_threaded(self, action, times):
        """在线程中执行控制动作"""
        try:
            self.execute_control_action(action, times)
        except Exception as e:
            print_flush(f"  动作执行异常: {e}")
            import traceback
            traceback.print_exc()
        finally:
            # 快速清理，避免阻塞
            # 注意：_stop() 已经在 execute_control_action 中调用了，这里只需要清理线程引用
            try:
                # 尝试非阻塞获取锁，如果获取不到就跳过（说明已经被新线程接管）
                if self.action_lock.acquire(blocking=False):
                    try:
                        if self.current_action_thread == threading.current_thread():
                            self.current_action_thread = None
                            # 不再调用 _stop()，因为已经在 execute_control_action 中调用了
                    finally:
                        self.action_lock.release()
            except Exception as e:
                print_flush(f"  警告: 清理动作线程时出错: {e}")
                import traceback
                traceback.print_exc()
    
    def execute_control_action(self, action, times):
        """
        执行控制动作
        参考llm_control_move.py的具体控制实现
        
        Args:
            action (int): 动作值
            times (int): 执行时间（秒）
        """
        print_flush(f"  执行动作: ", end="")
        
        # 检查是否有订阅者监听
        subscriber_count = self.mecanum_pub.get_subscription_count()
        if subscriber_count == 0:
            print_flush(f"\n  警告: 没有订阅者监听raw_cmd_vel话题，消息可能不会被处理")
            print_flush(f"  建议: 确保机器人控制系统正在运行并监听raw_cmd_vel话题")
        
        try:
            if action == 1:  # 前进
                print_flush(f"前进 {times}s")
                twist = Twist()
                twist.linear.x = 0.3
                print_flush(f"  发布Twist消息: linear.x={twist.linear.x}")
                print_flush(f"  发布器订阅者数量: {self.mecanum_pub.get_subscription_count()}")
                self._execute_twist_with_duration(twist, times, wait_subscriber=True)
                
            elif action == 2:  # 后退
                print(f"后退 {times}s")
                twist = Twist()
                twist.linear.x = -0.5
                print(f"  发布Twist消息: linear.x={twist.linear.x}")
                self._execute_twist_with_duration(twist, times)
                
            elif action == 3:  # 左转
                print(f"左转 {times}s")
                twist = Twist()
                twist.angular.z = 2.0
                print(f"  发布Twist消息: angular.z={twist.angular.z}")
                self._execute_twist_with_duration(twist, times, min_duration=0.5)
                
            elif action == 4:  # 右转
                print(f"右转 {times}s")
                twist = Twist()
                twist.angular.z = -2.0
                print(f"  发布Twist消息: angular.z={twist.angular.z}")
                self._execute_twist_with_duration(twist, times, min_duration=0.5)
                
            elif action == 5:  # 左移
                print(f"左移 {times}s")
                twist = Twist()
                twist.linear.y = 0.6
                print(f"  发布Twist消息: linear.y={twist.linear.y}")
                self._execute_twist_with_duration(twist, times)
                
            elif action == 6:  # 右移
                print(f"右移 {times}s")
                twist = Twist()
                twist.linear.y = -0.6
                print(f"  发布Twist消息: linear.y={twist.linear.y}")
                self._execute_twist_with_duration(twist, times)
                
            elif action == 7:  # 旋转
                print(f"旋转 {times}s")
                twist = Twist()
                twist.angular.z = -2.0  # 逆时针旋转
                print(f"  发布Twist消息: angular.z={twist.angular.z}")
                self._execute_twist_with_duration(twist, times, min_duration=0.5)
                
            elif action == 8:  # 停止
                print("停止")
                print(f"  发布停止Twist消息")
                self.mecanum_pub.publish(Twist())
                
            elif action == 13:  # 点头
                print("点头动作")
                self.pwm_controller([1, 1800])
                time.sleep(0.3)
                self.pwm_controller([1, 1200])
                time.sleep(0.3)
                self.pwm_controller([1, 1500])
                time.sleep(0.3)
                
            elif action == 14:  # 摇头
                print("摇头动作")
                self.pwm_controller([2, 1200])
                time.sleep(0.3)
                self.pwm_controller([2, 1800])
                time.sleep(0.3)
                self.pwm_controller([2, 1500])
                time.sleep(0.3)
                
            elif action == 15:  # 红灯
                print("亮红灯")
                self.sonar_rgb_controller(255, 0, 0)
                
            elif action == 16:  # 绿灯
                print("亮绿灯")
                self.sonar_rgb_controller(0, 255, 0)
                
            elif action == 17:  # 蓝灯
                print("亮蓝灯")
                self.sonar_rgb_controller(0, 0, 255)
                
            elif action == 18:  # 关灯
                print("关灯")
                self.sonar_rgb_controller(0, 0, 0)
                
            elif action == 12:  # 截图并通过POSIX通知
                print("抓拍并通知")
                self._capture_and_notify()

            elif action == 19:  # 声源定位跟随
                print("声源定位跟随")
                if not hasattr(self, 'face_detection_started'):
                    self.face_detection_started = False
                
                if not self.face_detection_started:
                    # 启动脚本
                    start_script = '''
                    cd /mnt/car_test/object_tracker
                    export LD_LIBRARY_PATH=$(pwd)/lib:$(pwd)/3rd_party/opencv_4.2.0/lib:$(pwd)/3rd_party/jsoncpp_1.9.5/lib:$LD_LIBRARY_PATH
                    export ADSP_LIBRARY_PATH=$(pwd)/lib
                    export PYTHONPATH=$(pwd)/build:$PYTHONPATH
                    source /usr/ros/humble/setup.bash
                    source /mnt/car_test/ros2_ws/install/setup.bash
                    export ROS_LOG_DIR=/tmp/ros2_logs
                    mkdir -p /tmp/ros2_logs
                    python3 /mnt/car_test/ros2_ws/src/app/app/face_detection_test.py > /tmp/face_detection.log 2>&1 &
                    '''

                    subprocess.Popen(['bash', '-c', start_script])
                    self.face_detection_started = True
                    self.get_logger().info("首次启动人脸检测程序")
                    time.sleep(3)
                else:
                    self.get_logger().info("人脸检测程序已启动，跳过启动步骤")
                    time.sleep(1)
                
                # 执行后续操作
                twist = Twist()
                twist.angular.z = 2.2
                duration = (360 - times) / 90.0
                self._execute_twist_with_duration(twist, float(f"{duration:.1f}"), min_duration=0.5)
                
                self.create_client(Trigger, '/ai_tracking/enter').call_async(Trigger.Request())
                self.create_client(SetString, '/ai_tracking/set_detector').call_async(
                    SetString.Request(data='body'))
            elif action == 20:  # 关闭人体跟随
                print("关闭人体跟随")
                result = subprocess.run(["pgrep", "-f", "face_detection_test.py"], capture_output=True, text=True)
                print(f"将被结束的进程PID: {result.stdout}")
                subprocess.run(["pkill", "-f", "face_detection_test.py"])

            else:
                print(f"未知动作: {action}")
                return
            
        except Exception as e:
            print_flush(f"  动作执行失败: {e}")
            import traceback
            traceback.print_exc()
    
    
    def start(self):
        """启动控制监听"""
        if self.reader:
            self.reader.start()
            self.running = True
            print("POSIX控制监听已启动，等待信号量通知...")
        else:
            print("POSIX共享内存读取器不可用")
    
    def stop(self):
        """停止控制监听"""
        if self.reader:
            self.reader.stop()
            self.running = False
            print("POSIX控制监听已停止")
    
    def cleanup(self):
        """清理资源"""
        if self.reader:
            self.reader.cleanup()



def main():
    """ROS2节点主函数"""
    global shutdown_requested
    
    # 设置信号处理
    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)
    
    rclpy.init()
    
    # 创建读取器
    reader_app = PosixControlReaderApp()
    
    # 启动控制监听
    reader_app.start()
    
    # 创建ROS2执行器
    from rclpy.executors import MultiThreadedExecutor
    executor = MultiThreadedExecutor()
    executor.add_node(reader_app)
    
    try:
        print_flush("按 Ctrl+C 停止...")
        # 使用spin_once而不是spin，这样更容易被KeyboardInterrupt捕获
        while not shutdown_requested:
            executor.spin_once(timeout_sec=0.1)
    except KeyboardInterrupt:
        print_flush("\n用户中断")
    finally:
        # 停止控制监听
        reader_app.stop()
        
        print_flush(f"\n=== 执行摘要 ===")
        print_flush(f"总共处理了 {reader_app.command_count} 个控制命令")
        print_flush("POSIX读取进程完成")
        
        # 清理ROS2节点
        reader_app.destroy_node()
        try:
            rclpy.shutdown()
        except Exception:
            # ROS2可能已经自动关闭，忽略错误
            pass


if __name__ == '__main__':
    main()