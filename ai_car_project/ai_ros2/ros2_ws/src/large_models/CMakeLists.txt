cmake_minimum_required(VERSION 3.8)
project(large_models)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# 查找依赖
find_package(ament_cmake REQUIRED)
find_package(ament_cmake_python REQUIRED)

# 尝试查找ROS2包，如果找不到就跳过
find_package(rclcpp QUIET)
find_package(geometry_msgs QUIET)
find_package(ros_robot_controller_msgs QUIET)

# 设置包含目录
include_directories(include/large_models)

# 创建共享库
add_library(posix_shm_manager SHARED
  src/posix_shm_manager.c
)

# 设置库的包含目录
target_include_directories(posix_shm_manager PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

# 链接库
target_link_libraries(posix_shm_manager
  pthread
  rt
)

# 安装库文件
install(TARGETS posix_shm_manager
  EXPORT posix_shm_managerTargets
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  RUNTIME DESTINATION bin
  INCLUDES DESTINATION include
)

# 安装头文件
install(DIRECTORY include/
  DESTINATION include
)

# 安装Python模块
install(DIRECTORY large_models/
  DESTINATION lib/python3.10/site-packages/large_models
  FILES_MATCHING PATTERN "*.py"
)

# 安装Python可执行文件
install(PROGRAMS
  large_models/qth_control_move.py
  DESTINATION lib/large_models
  RENAME qth_control_move
  PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

# 安装launch文件
install(DIRECTORY launch/
  DESTINATION share/large_models/launch
  FILES_MATCHING PATTERN "*.launch.py"
)

# 创建ament包
ament_package()
