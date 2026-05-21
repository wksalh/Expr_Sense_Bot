#include <fstream>
#include <sstream>
#include <opencv2/opencv.hpp>
#include "ExtractorWrapper.h"

int main(int argc, char *argv[])
{

    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <video folder> <configs path>" << std::endl;
        return -1;
    }
    // 设置CPU亲和性和优先级
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(5, &mask);
    sched_setaffinity(0, sizeof(cpu_set_t), &mask);

    int prio = 99;
    sched_setscheduler(0, SCHED_FIFO, (struct sched_param*)&prio);

    // 初始化
    ExtractorWrapper extractor(argv[2]);

    // 加载视频
    // int frame_id = 0;
    cv::VideoCapture capture;

    if (std::string(argv[1]) == "0") {
        // 模式1：使用摄像头（索引0）
        std::cout << "begin open camera (index 0)" << std::endl;
        capture.open(0);  // 初始化摄像头
        // is_camera_mode = true;

    } else {
        // 模式2：读取视频文件（原逻辑）
        std::cout << "begin read video: " << argv[1] << std::endl;
        capture.open(argv[1]);  // 初始化视频读取
    }
    if (!capture.isOpened()) {
        printf("could not read this video file...\n");
        return -1;
    }

    cv::VideoWriter video_writer;
    // 获取原视频的分辨率（宽、高）
    int video_width = static_cast<int>(capture.get(cv::CAP_PROP_FRAME_WIDTH));
    int video_height = static_cast<int>(capture.get(cv::CAP_PROP_FRAME_HEIGHT));
    // 可选：如果之前启用了旋转，需要交换宽高（当前旋转已注释，若启用需解开下面两行）
    // if 旋转启用:
    // std::swap(video_width, video_height);  // 旋转90度后，宽高互换

    // 创建视频写入器（参数：路径、编码格式、帧率、分辨率）
    video_writer.open(
        "output_video.avi",
        cv::VideoWriter::fourcc('X', 'V', 'I', 'D'),
        24,
        cv::Size(video_width, video_height)
    );

    // 检查视频写入器是否创建成功
    if (!video_writer.isOpened()) {
        std::cerr << "Possible reasons: Path not exists, permission denied, or fourcc not supported" << std::endl;
        capture.release();
        return -1;
    }
    
    cv::Mat frame;
    while (true)
    {
        if (!capture.read(frame)) // if not success, break loop
        {
            std::cout<<"\n Cannot read the video file. please check your video.\n";
            break;
        }
        // 读取收集拍摄视频图像会旋转  
        // cv::rotate(frame, frame, cv::ROTATE_90_CLOCKWISE);

        // 推理
        std::vector<TrackResult> results = extractor.process(frame);

        // 绘制
        for(TrackResult &res : results)
        {
            
            cv::Rect rect = cv::Rect(res.x, res.y, res.width, res.height);
            rectangle(frame, rect, cv::Scalar(255, 255, 0), 2);

            std::string label = cv::format("%d", res.id);
            cv::putText(frame, label, cv::Point(rect.x, rect.y), cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(255, 255, 0), 2);
        }

        video_writer.write(frame);
        // frame_id += 1;
    }
    std::cout << "运行结束" << std::endl;
    capture.release();
    video_writer.release();
}
