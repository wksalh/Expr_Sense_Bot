#!/usr/bin/env python3
import os
import pygame
import sys
import time
from PIL import Image, ImageSequence
import threading
import queue

class MultiEmotionGIFPlayer:
    def __init__(self):
        # 设置Weston环境变量
        self.setup_weston_environment()
        
        # 仅初始化视频部分，不初始化音频
        self.init_pygame_without_audio()
        
        # 状态变量
        self.emotion_file = "/tmp/emotion_log.txt"
        self.last_content = ""  # 记录上一次文件内容
        self.emotion_start_time = 0  # 记录开始显示当前情绪的时间
        self.showing_emotion = False  # 是否正在显示临时情绪
        self.current_emotion = "device_unused"  # 当前显示的情绪
        self.emotion_lock = threading.Lock()  # 情绪状态锁
        
        # 使用队列处理文件变化事件
        self.event_queue = queue.Queue()
        
        # 情绪到GIF文件的映射
        self.emotion_map = {
            "喜爱": "happy_love.gif",
            "开心": "happy_smile.gif", 
            "崇拜": "happy_adore.gif",
            "委屈": "sad_grievance.gif",
            "难过": "sad_heartbroken.gif",
            "眩晕": "surprise_giddy.gif",
            "疑问": "surprise_query.gif",
            "震惊": "surprise_shock.gif",
            "生气": "angry_cross.gif",
            "害怕": "frightened_scare.gif"
        }
        
        # 默认GIF
        self.default_gif = "device_unused.gif"
        
        # 加载所有GIF帧
        self.gif_frames = {}
        self.load_all_gifs()
        
        # 当前显示的帧
        self.current_frames = self.gif_frames[self.default_gif]
        self.current_frame_index = 0
        self.last_frame_time = pygame.time.get_ticks()
        self.frame_delay = 0
        
        # 性能优化：缓存常用值
        self.screen_center_x = self.screen_width // 2
        self.screen_center_y = self.screen_height // 2
        self.black_color = (0, 0, 0)
        
        # 启动文件监控线程
        self.running = True
        self.monitor_thread = threading.Thread(target=self.monitor_file_changes)
        self.monitor_thread.daemon = True
        self.monitor_thread.start()
        
        # 文件最后修改时间
        self.last_mod_time = 0
    
    def setup_weston_environment(self):
        """设置Weston环境变量"""
        os.environ['DISPLAY'] = ':0'
        os.environ['SDL_VIDEODRIVER'] = 'x11'
        
        # 设置Weston运行时目录
        weston_runtime_dir = '/dev/socket/weston'
        if os.path.exists(weston_runtime_dir):
            os.environ['XDG_RUNTIME_DIR'] = weston_runtime_dir
            os.environ['WAYLAND_DISPLAY'] = 'wayland-1'
        else:
            os.environ['XDG_RUNTIME_DIR'] = '/run/user/0'
    
    def init_pygame_without_audio(self):
        """初始化Pygame但不初始化音频系统"""
        pygame.display.init()
        
        # 设置全屏
        try:
            self.screen = pygame.display.set_mode((0, 0), pygame.FULLSCREEN)
            self.screen_width, self.screen_height = self.screen.get_size()
        except pygame.error:
            self.screen = pygame.display.set_mode((800, 600))
            self.screen_width, self.screen_height = 800, 600
        
        pygame.display.set_caption("Multi-Emotion GIF Player")
    
    def load_all_gifs(self):
        """加载所有GIF文件"""
        # 首先加载默认GIF
        if os.path.exists(self.default_gif):
            self.gif_frames[self.default_gif] = self.load_gif_frames(self.default_gif)
        else:
            print(f"错误: 找不到默认GIF文件 {self.default_gif}")
            sys.exit(1)
        
        # 加载所有情绪GIF
        missing_gifs = []
        for emotion, gif_file in self.emotion_map.items():
            if os.path.exists(gif_file):
                self.gif_frames[gif_file] = self.load_gif_frames(gif_file)
                print(f"已加载: {gif_file} -> {emotion}")
            else:
                missing_gifs.append(gif_file)
        
        if missing_gifs:
            print(f"警告: 找不到以下GIF文件: {', '.join(missing_gifs)}")
    
    def load_gif_frames(self, gif_path):
        """加载GIF并分解为帧"""
        frames = []
        try:
            gif = Image.open(gif_path)
            
            # 计算缩放比例
            first_frame = next(ImageSequence.Iterator(gif))
            frame_width, frame_height = first_frame.size
            scale_factor = min(self.screen_width / frame_width, self.screen_height / frame_height)
            
            # 只在需要缩放时计算新的尺寸
            if abs(scale_factor - 1.0) > 0.1:
                new_width = int(frame_width * scale_factor)
                new_height = int(frame_height * scale_factor)
                needs_scaling = True
            else:
                needs_scaling = False
            
            # 处理所有帧
            for frame in ImageSequence.Iterator(gif):
                # 转换为RGBA
                frame_rgba = frame.convert("RGBA")
                
                # 转换为Pygame表面
                frame_str = frame_rgba.tobytes()
                frame_surface = pygame.image.fromstring(frame_str, frame_rgba.size, "RGBA")
                
                # 缩放以适应屏幕（如果需要）
                if needs_scaling:
                    frame_surface = pygame.transform.scale(frame_surface, (new_width, new_height))
                
                # 获取帧延迟时间（毫秒）
                delay = frame.info.get('duration', 100)
                if delay < 10:
                    delay = 100
                
                frames.append({
                    'surface': frame_surface,
                    'delay': delay
                })
                
        except Exception as e:
            print(f"加载GIF失败 {gif_path}: {e}")
        
        return frames
    
    def monitor_file_changes(self):
        """监控文件变化"""
        while self.running:
            try:
                # 检查文件是否存在
                if os.path.exists(self.emotion_file):
                    # 获取文件修改时间
                    current_mod_time = os.path.getmtime(self.emotion_file)
                    
                    # 如果文件被修改了
                    if current_mod_time != self.last_mod_time:
                        self.last_mod_time = current_mod_time
                        
                        # 读取文件内容
                        with open(self.emotion_file, 'r') as f:
                            content = f.read().strip()
                        
                        # 将事件放入队列
                        self.event_queue.put(content)
                
                # 更频繁地检查文件（每50ms）
                time.sleep(0.05)
                
            except Exception as e:
                # 文件读取失败，等待后重试
                time.sleep(0.1)
    
    def process_file_event(self, content):
        """处理文件变化事件"""
        with self.emotion_lock:
            # 如果内容没有变化，直接返回
            if content == self.last_content:
                return False
            
            # 更新最后内容
            self.last_content = content
            
            # 如果正在显示情绪GIF，忽略新的情绪变化
            if self.showing_emotion:
                print(f"情绪显示中，忽略文件变化: {content}")
                return False
            
            # 检查是否匹配任何情绪关键字
            for emotion, gif_file in self.emotion_map.items():
                if emotion in content:
                    # 检测到情绪，开始显示对应的GIF
                    if gif_file in self.gif_frames:
                        print(f"检测到情绪: {emotion}, 切换到 {gif_file}")
                        self.current_frames = self.gif_frames[gif_file]
                        self.current_frame_index = 0
                        self.emotion_start_time = time.time()
                        self.showing_emotion = True
                        self.current_emotion = emotion
                        return True
                    else:
                        print(f"警告: GIF文件不存在 {gif_file}")
                        continue
            
            # 如果没有匹配任何情绪关键字，显示默认GIF
            print(f"未检测到情绪，切换回默认表情")
            self.current_frames = self.gif_frames[self.default_gif]
            self.current_frame_index = 0
            self.showing_emotion = False
            self.current_emotion = "device_unused"
            return True
    
    def update(self):
        """更新显示"""
        current_time = pygame.time.get_ticks()
        
        # 处理文件变化事件（非阻塞）
        try:
            while not self.event_queue.empty():
                content = self.event_queue.get_nowait()
                self.process_file_event(content)
        except queue.Empty:
            pass
        
        # 如果正在显示临时情绪，检查是否已经超过5秒
        if self.showing_emotion and time.time() - self.emotion_start_time >= 5:
            print(f"情绪 {self.current_emotion} 显示5秒结束，准备切换")
            
            with self.emotion_lock:
                # 先切换到默认
                self.current_frames = self.gif_frames[self.default_gif]
                self.current_frame_index = 0
                self.showing_emotion = False
                self.current_emotion = "device_unused"
                print("已切换回默认表情")
                
                # 然后立即检查当前文件内容
                if os.path.exists(self.emotion_file):
                    with open(self.emotion_file, 'r') as f:
                        current_content = f.read().strip()
                    
                    # 更新最后内容，避免重复触发
                    self.last_content = current_content
                    
                    # 如果当前文件内容有情绪，立即切换
                    for emotion, gif_file in self.emotion_map.items():
                        if emotion in current_content:
                            if gif_file in self.gif_frames:
                                print(f"5秒后检测到新情绪: {emotion}, 立即切换到 {gif_file}")
                                self.current_frames = self.gif_frames[gif_file]
                                self.current_frame_index = 0
                                self.emotion_start_time = time.time()
                                self.showing_emotion = True
                                self.current_emotion = emotion
                            break
        
        # 检查是否应该切换到下一帧
        if self.current_frames and self.current_frame_index < len(self.current_frames):
            frame_delay = self.current_frames[self.current_frame_index]['delay']
            if current_time - self.last_frame_time >= frame_delay:
                self.current_frame_index = (self.current_frame_index + 1) % len(self.current_frames)
                self.last_frame_time = current_time
    
    def draw(self):
        """绘制当前帧"""
        # 清屏
        self.screen.fill(self.black_color)
        
        # 获取当前帧
        if self.current_frames and self.current_frame_index < len(self.current_frames):
            current_frame = self.current_frames[self.current_frame_index]['surface']
            frame_rect = current_frame.get_rect()
            
            # 居中显示
            x = self.screen_center_x - frame_rect.width // 2
            y = self.screen_center_y - frame_rect.height // 2
            
            # 绘制当前帧
            self.screen.blit(current_frame, (x, y))
        
        # 更新显示
        pygame.display.flip()
    
    def handle_events(self):
        """处理事件"""
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                return False
            elif event.type == pygame.KEYDOWN:
                if event.key in [pygame.K_ESCAPE, pygame.K_q]:
                    return False
        
        return True
    
    def run(self):
        """主循环"""
        clock = pygame.time.Clock()
        running = True
        
        # 初始读取文件内容
        try:
            if os.path.exists(self.emotion_file):
                with open(self.emotion_file, 'r') as f:
                    self.last_content = f.read().strip()
                    self.last_mod_time = os.path.getmtime(self.emotion_file)
                    print(f"初始文件内容: '{self.last_content}'")
        except Exception as e:
            print(f"读取初始文件失败: {e}")
        
        print("程序启动，等待情绪输入...")
        
        while running:
            # 处理事件
            running = self.handle_events()
            
            # 更新状态
            self.update()
            
            # 绘制
            self.draw()
            
            # 控制帧率
            clock.tick(60)
        
        # 停止监控线程
        self.running = False
        if self.monitor_thread.is_alive():
            self.monitor_thread.join(timeout=1.0)
        
        pygame.quit()

def main():
    """主函数"""
    # 检查必要的库
    try:
        import pygame
        from PIL import Image
    except ImportError as e:
        print(f"导入库失败: {e}")
        return 1
    
    # 创建并运行应用程序
    try:
        app = MultiEmotionGIFPlayer()
        app.run()
    except Exception as e:
        print(f"程序运行出错: {e}")
        return 1
    
    return 0

if __name__ == "__main__":
    sys.exit(main())