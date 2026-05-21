# 目标追踪

## 环境准备
在运行前配置环境变量:
```bash
cd /path/to/tracker/

export LD_LIBRARY_PATH=$(pwd)/lib:$(pwd)/3rd_party/jsoncpp_1.9.5/lib/:$(pwd)/3rd_party/opencv_4.2.0/lib/
export ADSP_LIBRARY_PATH=$(pwd)/lib
```

## 编写代码
创建main.cpp文件，内容如下:
```cpp
#include <opencv2/opencv.hpp>
#include <fstream>
#include <sstream>
#include "Tracker.h"

int main(int argc, char *argv[])
{
    int benchmark_repeat_times = 0;     // 性能测试，不需要测试则设为 0

    // 模型参数设置
    float conf_thread = 0.2f;           // 检测置信度阈值 设置小于等于0.0f的值将使用模型默认值
    float iou_thread = 0.3f;            // 检测nms阈值 设置小于等于0.0f的值将使用模型默认值
    float score_thread = 0.2f;          // 检测得分阈值 设置小于等于0.0f的值将使用模型默认值
    int nn_budget = 10;                 // 跟踪模型保存的最大帧数量
    float max_cosine_distance = 0.4f;   // 跟踪cosine距离阈值
    bool use_reid = true;               // 是否使用ReID特征进行跟踪

    // 初始化
    char *p_auth_code = "xxx";                        // 算法 授权码
    char* det_model   = "./models/***/sim.engine";    // 检测模型路径
    char* track_model = "./models/reid/sim.engine";   // 追踪模型路径
    char* p_license_file = "./license.lic";           // 授权文件存储路径
    std::string output_path = "./output/";            // 结果图像保存路径
    std::string video_path  = "./videos/book.mp4";    // 输入视频路径或相机ID (例如 "0" 表示使用索引为0的摄像头)

    auto engine = std::make_unique<Tracker>();                                                          // 1.创建追踪引擎实例
    engine->set_param(conf_thread, iou_thread, score_thread, nn_budget, max_cosine_distance, use_reid); // 2. 设置模型参数
    auto init_res = engine->init(det_model, track_model, p_license_file, 0, p_auth_code);               // 3.初始化检测引擎

    ...

    return 0;
}
```

## 编译方法
```bash
mkdir build && cd build
cmake ..
make
cd ..
```

## 检测运行时库文件
检查命令
```bash
cd /path/to/object_track/
ldd ./build/object_track
```
几个关键的运行库的链接输出如下：
```bash
libengine_snpe.so => ./lib/libengine_snpe.so (0x0000007f877d0000)
libobject_tracker.so => ./lib/libobject_track.so (0x0000007f87510000)
libopencv_core.so.4.2 => ./3rd_party/opencv_4.2.0/lib/libopencv_core.so.4.2 (0x0000007f87190000)
libopencv_videoio.so.4.2 => ./3rd_party/opencv_4.2.0/lib/libopencv_videoio.so.4.2 (0x0000007f85c40000)
libSNPE.so => ./lib/libSNPE.so (0x0000007f84000000)
libjsoncpp.so.25 => ./3rd_party/jsoncpp/lib/libjsoncpp.so.25 (0x0000007f83f70000)
libopencv_imgproc.so.4.2 => ./3rd_party/opencv_4.2.0/lib/libopencv_imgproc.so.4.2 (0x0000007f83b60000)
libopencv_imgcodecs.so.4.2 => ./3rd_party/opencv_4.2.0/lib/libopencv_imgcodecs.so.4.2 (0x0000007f83800000)
libglog.so.3 => ./lib/libglog.so.3 (0x0000007f83750000)
libql_slas.so => ./lib/libql_slas.so (0x0000007f83410000)
libauthorization.so => ./lib/libauthorization.so (0x0000007fb9400000)
libgflags.so.2.2 => ./lib/libgflags.so.2.2 (0x0000007fb82c0000)
```

## 运行方法
检测模型包括（人脸/人手/人体） 会根据检测模型的输入路径自动判断使用哪种检测与追踪
```cpp
char* det_model   = "./models/face/sim.engine";    // 人脸检测+追踪
char* det_model   = "./models/hand/sim.engine";    // 人手检测+追踪
char* det_model   = "./models/person/sim.engine";  // 人体检测+追踪
```

```bash
./build/object_tracker
```

## 返回结果
```cpp
struct TrackResults {
    int id;                  // 跟踪ID
    int x, y, width, height; // 目标坐标
    float confidence;        // 目标检测置信度
};
```

## 推理时间及内存占用监测
设置 benchmark_repeat_times 大于 0 即可开启性能测试，例如设置为 100：
```
int benchmark_repeat_times = 100; // 性能测试，设置为 100 次
```

<img src="output/image.png" width="500">


## 追踪效果
<div style="display: flex; gap: 8px;">
  <img src="output/output_2.png" width="30%">
  <img src="output/output_6.png" width="30%">
  <img src="output/output_19.png" width="30%">
</div>

