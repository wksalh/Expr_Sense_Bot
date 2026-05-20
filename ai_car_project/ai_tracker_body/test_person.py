#!/usr/bin/env python3
"""
目标追踪Python接口测试脚本
使用方法: python3 test_person.py <视频文件路径> [<配置文件路径>]
"""

import cv2
import numpy as np
import sys
import os
import time

# 导入自定义追踪模块
try:
    import ai_tracker_body
    print("Successfully imported ai_tracker_body module")
except ImportError as e:
    print(f"Error importing ai_tracker_body: {e}")
    print("Make sure to run build.sh first")
    sys.exit(1)


def test_image(tracker, image_path):
    """测试单张图片"""
    print(f"\nTesting with image: {image_path}")
    
    # 读取图片
    img = cv2.imread(image_path)
    if img is None:
        print(f"Cannot read image: {image_path}")
        return
    
    # 转换为RGB（tracker期望RGB格式）
    img_rgb = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)
    
    # 处理图像
    start_time = time.time()
    results = tracker.process(img_rgb)
    process_time = (time.time() - start_time) * 1000
    
    print(f"Processing time: {process_time:.2f} ms")
    print(f"Found {len(results)} objects:")
    
    # 显示结果
    for i, res in enumerate(results):
        print(f"  Object {i+1}: ID={res.id}, BBox=({res.x}, {res.y}, {res.width}, {res.height})")
        
        # 绘制边界框
        cv2.rectangle(img, (res.x, res.y), 
                     (res.x + res.width, res.y + res.height), 
                     (0, 255, 0), 2)
        
        # 绘制ID
        cv2.putText(img, f"ID:{res.id}", (res.x, res.y - 10),
                   cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 255, 0), 2)
    
    # 显示图片
    cv2.imshow('Tracking Results', img)
    cv2.waitKey(0)
    cv2.destroyAllWindows()


def test_video(tracker, video_path, output_path="output_video.avi"):
    """测试视频文件"""
    print(f"\nTesting with video: {video_path}")
    
    # 打开视频
    cap = cv2.VideoCapture(video_path)
    if not cap.isOpened():
        print(f"Cannot open video: {video_path}")
        return
    
    # 获取视频信息
    fps = cap.get(cv2.CAP_PROP_FPS)
    width = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
    height = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
    
    print(f"Video info: {width}x{height}, FPS: {fps}")
    
    # 创建视频写入器
    fourcc = cv2.VideoWriter_fourcc(*'XVID')
    out = cv2.VideoWriter(output_path, fourcc, fps, (width, height))
    
    frame_count = 0
    total_time = 0
    
    while True:
        ret, frame = cap.read()
        if not ret:
            break
        
        frame_count += 1
        
        # 转换为RGB
        frame_rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        
        # 处理当前帧
        start_time = time.time()
        results = tracker.process(frame_rgb)
        process_time = (time.time() - start_time) * 1000
        
        total_time += process_time
        
        # 绘制结果
        for res in results:
            cv2.rectangle(frame, (res.x, res.y), 
                         (res.x + res.width, res.y + res.height), 
                         (0, 255, 0), 2)
            
            cv2.putText(frame, f"ID:{res.id}", (res.x, res.y - 10),
                       cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 255, 0), 2)
        
        # 显示处理信息
        cv2.putText(frame, f"Frame: {frame_count}", (10, 30),
                   cv2.FONT_HERSHEY_SIMPLEX, 1, (255, 255, 255), 2)
        
        cv2.putText(frame, f"Objects: {len(results)}", (10, 70),
                   cv2.FONT_HERSHEY_SIMPLEX, 1, (255, 255, 255), 2)
        
        # 写入输出视频
        out.write(frame)
        
        # 显示实时画面
        cv2.imshow('Video Tracking', frame)
        
        # 按'q'退出
        if cv2.waitKey(1) & 0xFF == ord('q'):
            break
    
    # 释放资源
    cap.release()
    out.release()
    cv2.destroyAllWindows()
    
    # 输出统计信息
    if frame_count > 0:
        avg_time = total_time / frame_count
        print(f"\nProcessed {frame_count} frames")
        print(f"Average processing time: {avg_time:.2f} ms/frame")
        print(f"Output saved to: {output_path}")


def test_camera(tracker, camera_id=0):
    """测试摄像头实时追踪"""
    print(f"\nTesting with camera (ID: {camera_id})")
    
    cap = cv2.VideoCapture(camera_id)
    if not cap.isOpened():
        print(f"Cannot open camera {camera_id}")
        return
    
    print("Press 'q' to quit, 's' to save frame")
    
    frame_count = 0
    save_count = 0
    
    while True:
        ret, frame = cap.read()
        if not ret:
            print("Failed to grab frame")
            break
        
        frame_count += 1
        
        # 转换为RGB
        frame_rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        
        # 处理当前帧
        start_time = time.time()
        results = tracker.process(frame_rgb)
        process_time = (time.time() - start_time) * 1000
        
        # 绘制结果
        for res in results:
            cv2.rectangle(frame, (res.x, res.y), 
                         (res.x + res.width, res.y + res.height), 
                         (0, 255, 0), 2)
            
            cv2.putText(frame, f"ID:{res.id}", (res.x, res.y - 10),
                       cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 255, 0), 2)
        
        # 显示处理时间
        cv2.putText(frame, f"Frame: {frame_count}", (10, 30),
                   cv2.FONT_HERSHEY_SIMPLEX, 1, (255, 255, 255), 2)
        
        cv2.putText(frame, f"Time: {process_time:.1f}ms", (10, 70),
                   cv2.FONT_HERSHEY_SIMPLEX, 1, (255, 255, 255), 2)
        
        cv2.putText(frame, f"Objects: {len(results)}", (10, 110),
                   cv2.FONT_HERSHEY_SIMPLEX, 1, (255, 255, 255), 2)
        
        # 显示画面
        cv2.imshow('Camera Tracking', frame)
        
        # 按键处理
        key = cv2.waitKey(1) & 0xFF
        if key == ord('q'):
            break
        elif key == ord('s'):
            # 保存当前帧
            save_count += 1
            filename = f"frame_{save_count}.jpg"
            cv2.imwrite(filename, frame)
            print(f"Frame saved to {filename}")
    
    cap.release()
    cv2.destroyAllWindows()
    print(f"Processed {frame_count} frames from camera")


def main():
    """主函数"""
    # 解析命令行参数
    if len(sys.argv) < 2:
        print("Usage:")
        print("  python3 test_person.py <video_file> [config_file]")
        print("  python3 test_person.py 0 [config_file]   (for camera)")
        print("\nExamples:")
        print("  python3 test_person.py person2.avi config.json")
        print("  python3 test_person.py 0 config.json")
        sys.exit(1)
    
    # 配置文件路径（默认为config.json）
    config_file = "config.json"
    if len(sys.argv) > 2:
        config_file = sys.argv[2]
    
    # 检查配置文件
    if not os.path.exists(config_file):
        print(f"Warning: Config file {config_file} not found")
        config_file = "config_fixed.json"  # 尝试备用配置
        if not os.path.exists(config_file):
            print(f"Error: No config file found")
            sys.exit(1)
    
    print(f"Using config file: {config_file}")
    
    # 初始化追踪器
    print("Initializing tracker...")
    try:
        tracker = ai_tracker_body.Tracker(config_file)
        print("Tracker initialized successfully")
    except Exception as e:
        print(f"Error initializing tracker: {e}")
        print("Make sure config.json is correctly configured")
        sys.exit(1)
    
    # 判断输入类型
    input_arg = sys.argv[1]
    
    if input_arg == "0":
        # 摄像头模式
        test_camera(tracker)
    elif os.path.isfile(input_arg):
        # 视频文件模式
        if input_arg.lower().endswith(('.jpg', '.jpeg', '.png')):
            # 图片文件
            test_image(tracker, input_arg)
        else:
            # 视频文件
            test_video(tracker, input_arg)
    else:
        print(f"Error: Invalid input: {input_arg}")
        print("Use '0' for camera or provide a valid file path")
        sys.exit(1)


if __name__ == "__main__":
    main()