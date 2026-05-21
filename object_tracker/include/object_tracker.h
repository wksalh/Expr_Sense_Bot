#ifdef TRACKER_BUILD_SHARED
#define TRACKER_API __attribute__((visibility("default")))
#else
#define TRACKER_API
#endif

#ifndef OBJECT_TRACKER_H 
#define OBJECT_TRACKER_H
#include <vector>
#include <string>
#include <opencv2/opencv.hpp>

enum TRACK_RESULT_CODE {
  PROCESS_OK = 0,
  INIT_ERROR = -1,
  ERR_INFERENCE_ERROW = -2,
  ERR_Image_Empty = -3,
  ERR_JSON_NULL = -4
};

struct TrackResults {
    int id;                  // 跟踪ID
    int x, y, width, height; // 目标坐标
    float confidence;        // 目标检测置信度
};

class TRACKER_API Tracker {
public:

    Tracker() {};
    /**
     * @brief  人脸识别初始化函数
     *
     * @param[in]   det_model               检测模型
     * @param[in]   track_model             跟踪模型
     * @param[in]   p_license_file          授权文件存储路径
     * @param[in]   module                  模块型号
     * @param[in]   p_auth_code             算法授权码
     * @return                              返回状态值
     */
    TRACK_RESULT_CODE init(char *det_model, char *track_model, char *p_license_file, int module, char *p_auth_code);
    
    /**
     * @brief  模型参数设置函数
     *
     * @param[in]   conf_thread             检测置信度阈值
     * @param[in]   iou_thread              检测nms阈值
     * @param[in]   score_thread            检测得分阈值
     * @param[in]   nn_budget               跟踪最大特征向量数量
     * @param[in]   max_cosine_distance     跟踪cosine距离阈值
     * @param[in]   use_reid                是否使用ReID特征进行跟踪
     */
    void set_param(float conf_thread, float iou_thread, float score_thread, int nn_budget, float max_cosine_distance, bool use_reid);
    
    /**
     * @brief  获取初始化状态
     *
     * @param[in]   opt             返回值选项，0表算法初始化状态，1表算法授权状态
     * @return                      返回状态值
     * TODO: 
     * 1.算法初始化状态，0表成功，-1表检测模型初始化失败，-2表追踪模型初始化失败，-3表检测与追踪模型初始化均失败 -4表JSON解析错误, -5表opt参数错误
     * 2.算法授权状态，0表成功，其他值见：QuecSlasErrCode_e
     */
    int get_status(int opt);
    
    /**
     * @brief 推理函数
     * 
     * #param p_datas 输入图像数据的无符号字符指针
     * @param rows 输入图像的行数
     * @param cols 输入图像的列数
     * @param channels 输入图像的通道数
     * @param v_results 推理结果，存储追踪结果，每个结果包含追踪id和对应的边界框坐标及检测得分
     *
     * @return int - 推理状态码
     */
    TRACK_RESULT_CODE inference(unsigned char*p_datas, int rows, int cols, int channels, std::vector<TrackResults> &v_results);

    /**
     * @brief 获取性能信息函数
     *
     * @param img_src: 输入图像
     * @param repeat_times: 重复推理次数
     */
    void get_benchmark_info(cv::Mat &img_src, int repeat_times = 100);

    /**
     * @brief 保存结果函数
     *
     * @param img_src: 输入图像
     * @param v_results: 追踪结果列表
     * @param output_path: 结果保存路径
     * @param image_path: 输出图像名称
     */
    void save_result(const cv::Mat &img_src,
                    std::vector<TrackResults> &v_results,
                    std::string &output_path,
                    const std::string &image_name);

    /**
     * @brief 反初始化函数
     */
    void uninit_model();

    ~Tracker() {};

private:
    void* handle_;

};

#endif // OBJECT_TRACKER_H