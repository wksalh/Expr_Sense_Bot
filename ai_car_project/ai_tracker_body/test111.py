import ai_tracker_body
import cv2
import time

# 初始化追踪器
tracker = ai_tracker_body.Tracker("config.json")

# 处理视频，只输出坐标
cap = cv2.VideoCapture("person2.avi")
frame_count = 0

while True:
    ret, frame = cap.read()
    if not ret:
        break
    
    frame_count += 1
    rgb_frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
    
    start = time.time()
    results = tracker.process(rgb_frame)
    proc_time = (time.time() - start) * 1000
    
    if frame_count % 10 == 0 or len(results) > 0:
        print(f"Frame {frame_count}: {len(results)} objects, {proc_time:.1f}ms")
        for res in results:
            print(f"  ID {res.id}: x={res.x}, y={res.y}, w={res.width}, h={res.height}")

cap.release()
print(f"Total frames: {frame_count}")