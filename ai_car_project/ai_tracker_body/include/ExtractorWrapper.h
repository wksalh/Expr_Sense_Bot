#ifndef EXTRACTOR_WRAPPER_H
#define EXTRACTOR_WRAPPER_H
#include <vector>
namespace cv {
    class Mat;
}

struct TrackResult {
    int id;                 // 跟踪ID
    int x, y, width, height;// 目标坐标
};

class ExtractorWrapper {
public:
    // 构造函数
    ExtractorWrapper(char* configs_path);

    // 析构函数
    virtual ~ExtractorWrapper();
    // 推理
    std::vector<TrackResult> process(cv::Mat& frame_data);

private:
    
    class ModelImpl; 
    ModelImpl* impl_; 
};

#endif // EXTRACTOR_WRAPPER_H