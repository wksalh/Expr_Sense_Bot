cmake_minimum_required(VERSION 3.12)
project(tracker_pybind)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找 Python 和 pybind11
find_package(Python3 REQUIRED COMPONENTS Interpreter Development)
find_package(pybind11 CONFIG REQUIRED)

# 项目自定义路径
set(TRACKER_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(THIRD_PARTY_DIR ${TRACKER_ROOT}/3rd_party)
set(LIB_DIR ${TRACKER_ROOT}/lib)
set(INCLUDE_DIR ${TRACKER_ROOT}/include)

# OpenCV 4.2.0（手动指定）
set(OpenCV_INCLUDE_DIR ${THIRD_PARTY_DIR}/opencv_4.2.0/include)
set(OpenCV_LIB_DIR ${THIRD_PARTY_DIR}/opencv_4.2.0/lib)
set(OpenCV_LIBS
    ${OpenCV_LIB_DIR}/libopencv_core.so
    ${OpenCV_LIB_DIR}/libopencv_imgproc.so
    ${OpenCV_LIB_DIR}/libopencv_imgcodecs.so
    ${OpenCV_LIB_DIR}/libopencv_highgui.so
    ${OpenCV_LIB_DIR}/libopencv_videoio.so
)

# jsoncpp（路径已改为 jsoncpp_1.9.5）
set(JSONCPP_LIB ${THIRD_PARTY_DIR}/jsoncpp_1.9.5/lib/libjsoncpp.so.25)

# 你的跟踪库及其依赖（仅链接必要的 .so，不包括运行时加载的 skel 库）
set(TRACKER_LIBS
    ${LIB_DIR}/libobject_tracker.so
    ${LIB_DIR}/libperception.so      # 新增依赖
    ${LIB_DIR}/libSNPE.so
    ${LIB_DIR}/libengine_snpe.so
    ${LIB_DIR}/libauthorization.so
    # libSnpeHtpV68Skel.so 和 libSnpeHtpV68Stub.so 是运行时动态加载的，不链接
)

# 创建 pybind11 模块
pybind11_add_module(tracker_binding py_tracker_binding.cpp)

# 包含目录
target_include_directories(tracker_binding PRIVATE
    ${INCLUDE_DIR}
    ${OpenCV_INCLUDE_DIR}
    ${THIRD_PARTY_DIR}/jsoncpp_1.9.5/include
)

# 链接库
target_link_libraries(tracker_binding PRIVATE
    ${OpenCV_LIBS}
    ${JSONCPP_LIB}
    ${TRACKER_LIBS}
)

# 设置运行时 RPATH，避免每次设置 LD_LIBRARY_PATH
set_target_properties(tracker_binding PROPERTIES
    INSTALL_RPATH "${LIB_DIR}:${OpenCV_LIB_DIR}:${THIRD_PARTY_DIR}/jsoncpp_1.9.5/lib"
    BUILD_WITH_INSTALL_RPATH TRUE
)