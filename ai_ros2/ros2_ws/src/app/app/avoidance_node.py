#!/usr/bin/env python3
# encoding: utf-8
# 麦轮底盘避障节点

import time
import rclpy
from rclpy.node import Node
from geometry_msgs.msg import Twist
from std_msgs.msg import Int32, Bool
from std_srvs.srv import Trigger, SetBool

class MecanumAvoidanceNode(Node):
    """麦轮底盘避障节点 """
    
    def __init__(self):
        super().__init__('avoidance_node')
        
        # 宏开关配置
        self.avoidance_enabled = True     # 避障开关（默认为关闭）
        
        # 避障参数
        self.SAFE_DISTANCE = 25.0          # 安全距离(cm)
        
        # 关键时间参数
        self.TURN_SPEED = 2.0              # 转向速度
        self.TURN_DURATION = 0.8           # 转向持续时间(秒)
        self.FORWARD_SPEED = 0.3           # 前进速度
        self.FORWARD_DURATION = 1.0        # 转向后前进时间(秒)
        self.BUFFER_DURATION = 0.2         # 动作间缓冲时间
        
        # 超声波频率控制
        self.last_sonar_time = 0
        self.SONAR_MIN_INTERVAL = 0.07     # 70ms最小间隔
        
        # 状态机
        self.state = "IDLE"
        self.state_start_time = 0
        
        # 指令处理
        self.current_cmd = Twist()         # 当前处理的指令
        self.last_cmd_type = "NONE"        # 最后指令类型：NONE, FORWARD, OTHER
        self.cmd_publish_time = 0          # 上次发布指令的时间
        
        # 避障控制
        self.consecutive_left_turns = 0    # 连续左转次数
        self.max_consecutive_left_turns = 3  # 最大连续左转次数
        self.avoidance_active = False      # 避障是否激活（仅当避障开启时才可能激活）
        
        # 超声波数据
        self.current_distance = 500.0
        self.distance_buffer = []
        
        # 订阅/发布
        self.sonar_sub = self.create_subscription(
            Int32, 'sonar_controller/get_distance',
            self.sonar_callback, 1)
        
        self.cmd_sub = self.create_subscription(
            Twist, 'raw_cmd_vel',
            self.cmd_callback, 1)
        
        self.cmd_pub = self.create_publisher(Twist, 'cmd_vel', 1)
        
        # 避障状态发布
        self.status_pub = self.create_publisher(Bool, 'avoidance_status', 10)
        
        # 服务
        # SetBool服务用于精确控制开关状态
        self.create_service(SetBool, '~/set_enabled', self.set_enabled_callback)
        # Trigger服务用于向后兼容
        self.create_service(Trigger, '~/enter', self.enter_callback)
        self.create_service(Trigger, '~/exit', self.exit_callback)
        
        # 定时器
        self.create_timer(0.05, self.state_machine_loop)
        self.create_timer(0.5, self.status_log)
        self.create_timer(1.0, self.publish_status)  # 每秒发布一次状态
        
        self.get_logger().info("麦轮避障节点启动（宏开关最终版）")
        self.get_logger().info(f"初始状态: 避障{'启用' if self.avoidance_enabled else '禁用'}")
    
    def publish_status(self):
        """发布避障状态"""
        status_msg = Bool()
        status_msg.data = self.avoidance_enabled
        self.status_pub.publish(status_msg)
    
    def sonar_callback(self, msg):
        """超声波数据处理"""
        # 只有避障开启时才处理超声波数据
        if not self.avoidance_enabled:
            return
        
        # 只有避障激活时才进一步处理
        if not self.avoidance_active:
            return
        
        current_time = time.time()
        
        if current_time - self.last_sonar_time < self.SONAR_MIN_INTERVAL:
            return
        
        self.last_sonar_time = current_time
        
        raw_mm = msg.data
        
        # 过滤无效值
        if raw_mm == 255 or raw_mm == 0:
            return
        
        if raw_mm < 20 or raw_mm > 4000:
            return
        
        distance_cm = raw_mm / 10.0
        
        # 滤波
        self.distance_buffer.append(distance_cm)
        if len(self.distance_buffer) > 3:
            self.distance_buffer.pop(0)
        
        if self.distance_buffer:
            self.current_distance = sum(self.distance_buffer) / len(self.distance_buffer)
    
    def is_forward_command(self, twist):
        """判断是否为前进指令"""
        # 前进指令：线速度 > 0，角速度 ≈ 0
        return twist.linear.x > 0.01 and abs(twist.angular.z) < 0.01
    
    def cmd_callback(self, msg):
        """原始指令处理 - 支持宏开关"""
        current_time = time.time()
        
        # 更新当前指令和时间戳
        self.current_cmd = msg
        self.cmd_publish_time = current_time
        
        # 宏开关处理：如果避障关闭，直接转发指令
        if not self.avoidance_enabled:
            # 直接转发所有指令
            self.cmd_pub.publish(msg)
            return
        
        # 避障开启时的逻辑
        
        # 判断指令类型
        if self.is_forward_command(msg):
            # 前进指令
            self.get_logger().debug(f"前进指令: linear.x={msg.linear.x:.2f}")
            
            if self.last_cmd_type != "FORWARD":
                # 从其他指令切换到前进指令
                self.last_cmd_type = "FORWARD"
                self.avoidance_active = True
                
                # 如果当前是IDLE状态，开始前进
                if self.state == "IDLE":
                    self.state = "FORWARD"
                    self.state_start_time = current_time
                    self.consecutive_left_turns = 0
            
            # 发布前进指令
            self.cmd_pub.publish(msg)
            
        else:
            # 非前进指令（后退、转向、停止）
            cmd_type = "停止"
            if msg.linear.x < -0.01:
                cmd_type = "后退"
            elif abs(msg.angular.z) > 0.01:
                cmd_type = "转向"
            
            self.get_logger().debug(f"{cmd_type}指令: linear.x={msg.linear.x:.2f}, angular.z={msg.angular.z:.2f}")
            
            # 立即停止任何避障状态
            if self.avoidance_active:
                self.avoidance_active = False
                self.state = "IDLE"
                self.consecutive_left_turns = 0
            
            self.last_cmd_type = "OTHER"
            
            # 立即发布指令
            self.cmd_pub.publish(msg)
    
    def should_check_obstacle(self):
        """判断当前状态是否需要检测障碍物"""
        # 只有避障开启且激活且在前进相关状态才检测障碍物
        if not self.avoidance_enabled:
            return False
        
        if not self.avoidance_active:
            return False
        
        # 以下状态不进行障碍物检测
        no_check_states = [
            "STOP_BEFORE_TURN", "TURNING_LEFT", "STOP_AFTER_TURN",
            "CHECK_AFTER_TURN", "STOP_BEFORE_FORWARD", "FORWARD_AFTER_TURN",
            "STOP_BEFORE_RIGHT_TURN", "TURNING_RIGHT", "STOP_AFTER_RIGHT_TURN",
            "RESUME_FORWARD"
        ]
        
        return self.state not in no_check_states
    
    def state_machine_loop(self):
        """状态机主循环"""
        # 宏开关处理：如果避障关闭，不执行状态机
        if not self.avoidance_enabled:
            # 透明转发模式：直接转发最新指令
            current_time = time.time()
            # 检查是否最近收到过指令（2秒内）
            if current_time - self.cmd_publish_time < 2.0:
                self.cmd_pub.publish(self.current_cmd)
            return
        
        current_time = time.time()
        
        # 非前进指令持续发布
        # 如果当前是非前进指令，持续发布该指令
        if self.last_cmd_type == "OTHER":
            # 检查是否最近收到过非前进指令（2秒内）
            if current_time - self.cmd_publish_time < 2.0:
                # 持续发布非前进指令
                self.cmd_pub.publish(self.current_cmd)
            return
        
        # 前进指令下的避障逻辑
        # 如果避障未激活，不执行状态机
        if not self.avoidance_active:
            # 只有在空闲状态才发布前进指令
            if self.state == "IDLE":
                self.cmd_pub.publish(self.current_cmd)
            return
        
        elapsed = current_time - self.state_start_time
        
        # 状态机逻辑
        if self.state == "IDLE":
            # IDLE状态不发布指令，等待cmd_callback处理
            pass
        
        elif self.state == "FORWARD":
            # 前进状态
            
            if (self.should_check_obstacle() and 
                self.current_distance <= self.SAFE_DISTANCE):
                
                self.get_logger().info(f"检测到障碍物: {self.current_distance:.1f}cm")
                
                # 切换到停止前状态
                self.state = "STOP_BEFORE_TURN"
                self.state_start_time = current_time
                self.publish_stop()
                self.get_logger().info("前进->左转，停止缓冲")
                return
            
            # 无障碍物，正常前进
            self.cmd_pub.publish(self.current_cmd)
        
        elif self.state == "STOP_BEFORE_TURN":
            # 转向前缓冲
            
            if elapsed >= self.BUFFER_DURATION:
                # 缓冲完成，开始左转
                self.state = "TURNING_LEFT"
                self.state_start_time = current_time
                self.consecutive_left_turns += 1
                
                # 发布左转指令
                twist = Twist()
                twist.angular.z = self.TURN_SPEED
                self.cmd_pub.publish(twist)
                
                self.get_logger().info(f"开始左转 (连续第{self.consecutive_left_turns}次)")
            else:
                self.publish_stop()
        
        elif self.state == "TURNING_LEFT":
            # 左转状态
            
            if elapsed >= self.TURN_DURATION:
                # 左转完成
                self.get_logger().info(f"左转完成 ({self.TURN_DURATION}s)")
                
                # 切换到左转后缓冲
                self.state = "STOP_AFTER_TURN"
                self.state_start_time = current_time
                self.publish_stop()
            else:
                # 持续左转
                twist = Twist()
                twist.angular.z = self.TURN_SPEED
                self.cmd_pub.publish(twist)
        
        elif self.state == "STOP_AFTER_TURN":
            # 左转后缓冲
            
            if elapsed >= self.BUFFER_DURATION:
                # 缓冲完成，开始检测
                self.state = "CHECK_AFTER_TURN"
                self.state_start_time = current_time
                self.get_logger().info("开始检测前方障碍物")
            else:
                self.publish_stop()
        
        elif self.state == "CHECK_AFTER_TURN":
            # 检测状态
            
            # 等待传感器稳定
            if elapsed < 0.15:
                self.publish_stop()
                return
            
            # 检查是否超过最大连续左转次数
            if self.consecutive_left_turns >= self.max_consecutive_left_turns:
                self.get_logger().warn("达到最大连续左转次数，停止避障")
                self.state = "IDLE"
                self.avoidance_active = False
                self.consecutive_left_turns = 0
                self.publish_stop()
                return
            
            # 检查前方障碍物
            if self.current_distance > self.SAFE_DISTANCE:
                # 无障碍物，准备前进
                self.state = "STOP_BEFORE_FORWARD"
                self.state_start_time = current_time
                self.publish_stop()
                self.get_logger().info("前方无障碍物，准备前进")
            else:
                # 仍有障碍物，再次左转
                self.get_logger().info(f"前方仍有障碍物: {self.current_distance:.1f}cm，再次左转")
                
                self.state = "STOP_BEFORE_TURN"
                self.state_start_time = current_time
                self.publish_stop()
        
        elif self.state == "STOP_BEFORE_FORWARD":
            # 前进前缓冲
            
            if elapsed >= self.BUFFER_DURATION:
                # 缓冲完成，开始前进
                self.state = "FORWARD_AFTER_TURN"
                self.state_start_time = current_time
                
                # 发布前进指令
                twist = Twist()
                twist.linear.x = self.FORWARD_SPEED
                self.cmd_pub.publish(twist)
                
                self.get_logger().info(f"开始前进{self.FORWARD_DURATION}秒")
            else:
                self.publish_stop()
        
        elif self.state == "FORWARD_AFTER_TURN":
            # 前进状态
            
            if elapsed >= self.FORWARD_DURATION:
                # 前进完成，准备右转
                self.state = "STOP_BEFORE_RIGHT_TURN"
                self.state_start_time = current_time
                self.publish_stop()
                self.get_logger().info("前进完成，准备右转")
            else:
                # 持续前进
                twist = Twist()
                twist.linear.x = self.FORWARD_SPEED
                self.cmd_pub.publish(twist)
        
        elif self.state == "STOP_BEFORE_RIGHT_TURN":
            # 右转前缓冲
            
            if elapsed >= self.BUFFER_DURATION:
                # 缓冲完成，开始右转
                self.state = "TURNING_RIGHT"
                self.state_start_time = current_time
                
                # 发布右转指令
                twist = Twist()
                twist.angular.z = -self.TURN_SPEED
                self.cmd_pub.publish(twist)
                
                self.get_logger().info("开始右转")
            else:
                self.publish_stop()
        
        elif self.state == "TURNING_RIGHT":
            # 右转状态
            
            if elapsed >= self.TURN_DURATION:
                # 右转完成，重置连续左转计数
                self.consecutive_left_turns = 0
                self.get_logger().info(f"右转完成 ({self.TURN_DURATION}s)")
                
                # 切换到右转后缓冲
                self.state = "STOP_AFTER_RIGHT_TURN"
                self.state_start_time = current_time
                self.publish_stop()
            else:
                # 持续右转
                twist = Twist()
                twist.angular.z = -self.TURN_SPEED
                self.cmd_pub.publish(twist)
        
        elif self.state == "STOP_AFTER_RIGHT_TURN":
            # 右转后缓冲
            
            if elapsed >= self.BUFFER_DURATION:
                # 缓冲完成，准备恢复前进
                self.state = "RESUME_FORWARD"
                self.state_start_time = current_time
                self.get_logger().info("准备恢复前进")
            else:
                self.publish_stop()
        
        elif self.state == "RESUME_FORWARD":
            # 恢复前进状态
            
            if elapsed >= self.BUFFER_DURATION:
                # 缓冲完成，继续前进
                self.state = "FORWARD"
                self.state_start_time = current_time
                self.get_logger().info("避障循环完成，继续前进")
            else:
                self.publish_stop()
    
    def publish_stop(self):
        """发布停止指令"""
        twist = Twist()
        self.cmd_pub.publish(twist)
    
    def set_enabled_callback(self, request, response):
        """设置避障启用状态（SetBool服务）"""
        enable = request.data
        
        if enable and not self.avoidance_enabled:
            # 启用避障
            self.avoidance_enabled = True
            self.state = "IDLE"
            self.last_cmd_type = "NONE"
            self.avoidance_active = False
            self.consecutive_left_turns = 0
            response.success = True
            response.message = "避障已启用"
            self.get_logger().info("避障系统启用")
            
        elif not enable and self.avoidance_enabled:
            # 禁用避障
            self.avoidance_enabled = False
            self.state = "IDLE"
            self.last_cmd_type = "NONE"
            self.avoidance_active = False
            self.consecutive_left_turns = 0
            response.success = True
            response.message = "避障已禁用"
            self.get_logger().info("避障系统禁用")
            
            # 确保发送停止指令
            self.publish_stop()
            
        else:
            # 状态没有变化
            response.success = True
            if enable:
                response.message = "避障已处于启用状态"
            else:
                response.message = "避障已处于禁用状态"
        
        return response
    
    def enter_callback(self, request, response):
        """启用避障（Trigger服务 - 向后兼容）"""
        return self.set_enabled_callback(SetBool.Request(data=True), response)
    
    def exit_callback(self, request, response):
        """禁用避障（Trigger服务 - 向后兼容）"""
        return self.set_enabled_callback(SetBool.Request(data=False), response)
    
    def status_log(self):
        """状态日志"""
        status_msg = f"状态: {self.state}"
        
        # 只有在避障开启时才显示详细状态
        if self.avoidance_enabled:
            # 显示剩余时间
            if "TURNING" in self.state:
                elapsed = time.time() - self.state_start_time
                remaining = max(0, self.TURN_DURATION - elapsed)
                status_msg += f" (剩余{remaining:.1f}s)"
            elif self.state == "FORWARD_AFTER_TURN":
                elapsed = time.time() - self.state_start_time
                remaining = max(0, self.FORWARD_DURATION - elapsed)
                status_msg += f" (剩余{remaining:.1f}s)"
            elif "STOP" in self.state or self.state == "RESUME_FORWARD":
                elapsed = time.time() - self.state_start_time
                remaining = max(0, self.BUFFER_DURATION - elapsed)
                status_msg += f" (缓冲{remaining:.1f}s)"
            
            # 显示距离和连续左转次数
            if self.state != "IDLE":
                status_msg += f" | 距离: {self.current_distance:.1f}cm"
                if self.avoidance_active:
                    status_msg += f" | 连续左转: {self.consecutive_left_turns}/{self.max_consecutive_left_turns}"
            
            # 显示当前指令类型
            status_msg += f" | 指令: {self.last_cmd_type}"
        else:
            status_msg += " | 模式: 透明转发"
        
        self.get_logger().info(status_msg)

def main():
    rclpy.init()
    node = MecanumAvoidanceNode()
    
    try:
        # 默认不启用避障（透明转发模式）
        rclpy.spin(node)
    except KeyboardInterrupt:
        node.get_logger().info("节点终止")
    finally:
        node.destroy_node()
        rclpy.shutdown()

if __name__ == '__main__':
    main()