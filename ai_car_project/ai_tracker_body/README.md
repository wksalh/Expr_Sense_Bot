***************************
ai_tracker_body 目标追踪（人脸/人体/人手）功能 
***************************


1.跟随代码运行
cd /mnt/car_test/ros2_ws/src/app/app
source /usr/ros/humble/setup.bash
source /mnt/car_test/ros2_ws/install/setup.bash
export LD_LIBRARY_PATH=/mnt/car_test/ai_tracker_body/lib:$LD_LIBRARY_PATH
export ADSP_LIBRARY_PATH="/mnt/car_test/ai_tracker_body/lib/;/system/lib/rfsa/adsp;/system/vendor/lib/rfsa/adsp;/dsp"
export ROS_LOG_DIR=/tmp/ros2_logs && mkdir -p /tmp/ros2_logs && export need_compile=True
python3 

2.ros2控制执行
ros2 service call /human_tracking/enter std_srvs/srv/Trigger   #启动跟随

# 人脸检测
ros2 service call /object_tracking/set_detector large_models_msgs/srv/SetString "{data: 'face'}"

# 人体检测
ros2 service call /object_tracking/set_detector large_models_msgs/srv/SetString "{data: 'body'}"

# 人手检测
ros2 service call /object_tracking/set_detector large_models_msgs/srv/SetString "{data: 'hand'}"

# 单独云台跟踪
ros2 service call /object_tracking/set_pan_tilt std_srvs/srv/SetBool "{data: true}"

# 单独车体跟踪
ros2 service call /object_tracking/set_chassis_following std_srvs/srv/SetBool "{data: true}"

# 联合跟踪（同时启用两者）
ros2 service call /object_tracking/set_pan_tilt std_srvs/srv/SetBool "{data: true}"
ros2 service call /object_tracking/set_chassis_following std_srvs/srv/SetBool "{data: true}"