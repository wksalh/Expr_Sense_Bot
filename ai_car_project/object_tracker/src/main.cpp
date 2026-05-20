#include <fstream>
#include <sstream>
#include "object_tracker.h"

int main(int argc, char *argv[])
{

    int benchmark_repeat_times = 0; // 性能测试，不需要测试则设为 0

    // 模型参数设置
    float conf_thread = 0.2f; // 检测置信度阈值 设置小于等于0.0f的值将使用模型默认值
    float iou_thread = 0.3f; // 检测nms阈值 设置小于等于0.0f的值将使用模型默认值
    float score_thread = 0.2f; // 检测得分阈值 设置小于等于0.0f的值将使用模型默认值
    int nn_budget = 10; // 跟踪最大特征向量数量
    float max_cosine_distance = 0.4f; // 跟踪cosine距离阈值
    bool use_reid = true; // 是否使用ReID特征进行跟踪

    // 初始化
    char *p_auth_code = "D7F81710-BC7C-4B9C-A213-9B9BA14166A7"; // 算法 授权码
    char* det_model   = "./models/person/sim.engine"; // 检测模型路径
    char* track_model = "./models/reid/sim.engine"; // 追踪模型路径
    char* p_license_file = "./license.lic";
    std::string output_path = "./output/"; // 结果图像保存路径
    std::string video_path  = "./videos/book.mp4"; // 输入视频路径或相机ID (例如 "0" 表示使用索引为0的摄像头)

    auto engine = std::make_unique<Tracker>(); // 1. 创建追踪引擎实例
    engine->set_param(conf_thread, iou_thread, score_thread, nn_budget, max_cosine_distance, use_reid); // 2. 设置模型参数

    auto init_res = engine->init(det_model, track_model, p_license_file, 0, p_auth_code); // 初始化检测引擎
    int status = engine->get_status(0); // 获取初始化状态

    std::cout << "Initialization status:" << status << std::endl;
    if (init_res != PROCESS_OK)
    {
        std::cerr << "Failed to initialize engine. init_res = " << init_res << "\n";
        engine.release();
        return EXIT_FAILURE;
    }

    std::cout << "Start tracker...\n";

    // 加载视频
    cv::VideoCapture capture;

    if (video_path == "0") {
        // 模式1：使用摄像头（索引0）
        std::cout << "begin open camera (index 0)" << std::endl;
        capture.open(0);  // 初始化摄像头
        // is_camera_mode = true;

    } else {
        // 模式2：读取视频文件（原逻辑）
        std::cout << "begin read video: " << video_path << std::endl;
        capture.open(video_path);  // 初始化视频读取
    }
    if (!capture.isOpened()) {
        printf("could not read this video file...\n");
        return -1;
    }
    
    cv::Mat frame;
    int frame_id = 0;

    while (true)
    {
        if (!capture.read(frame)) // if not success, break loop
        {
            std::cout<<"\n Cannot read the video file. please check your video.\n";
            break;
        }
        // 测试
        if (benchmark_repeat_times > 0 && frame_id == 0)
        {
            engine->get_benchmark_info(frame, benchmark_repeat_times); // 获取性能信息
        }
        // 推理
        std::vector<TrackResults> infer_result; // 存储推理结果
        TRACK_RESULT_CODE res = engine->inference(frame.data, frame.rows, frame.cols, frame.channels(), infer_result); // 3. 进行推理
        std::cout << "res = " << res << std::endl;
        // 绘制结果
        
        engine->save_result(frame, infer_result, output_path, std::to_string(frame_id));
        std::cout << "Frame " << frame_id << " processed." << std::endl;
        frame_id++;
    }
    std::cout << "运行结束" << std::endl;
    engine->uninit_model();
    capture.release();
}
