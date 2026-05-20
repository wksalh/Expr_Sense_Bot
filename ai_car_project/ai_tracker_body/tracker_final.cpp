#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>
#include <opencv2/opencv.hpp>
#include "ExtractorWrapper.h"

namespace py = pybind11;

// 辅助函数：numpy数组转cv::Mat
cv::Mat numpy_to_cv_mat(py::array_t<unsigned char>& img_array) {
    py::buffer_info buf = img_array.request();
    
    if (buf.ndim != 3)
        throw std::runtime_error("Number of dimensions must be 3 (H, W, C)");
    
    int height = buf.shape[0];
    int width = buf.shape[1];
    int channels = buf.shape[2];
    
    if (channels != 3 && channels != 1)
        throw std::runtime_error("Channels must be 1 or 3");
    
    cv::Mat mat;
    if (channels == 3) {
        mat = cv::Mat(height, width, CV_8UC3, buf.ptr);
        // 假设输入是RGB，转换为BGR（OpenCV默认）
        cv::cvtColor(mat, mat, cv::COLOR_RGB2BGR);
    } else {
        mat = cv::Mat(height, width, CV_8UC1, buf.ptr);
    }
    
    return mat.clone();
}

// Python友好的追踪结果结构
struct PyTrackResult {
    int id;
    int x;
    int y;
    int width;
    int height;
    
    PyTrackResult(int id, int x, int y, int w, int h)
        : id(id), x(x), y(y), width(w), height(h) {}
    
    // 转换为Python字典
    py::dict to_dict() const {
        py::dict d;
        d["id"] = id;
        d["x"] = x;
        d["y"] = y;
        d["width"] = width;
        d["height"] = height;
        return d;
    }
    
    // 字符串表示
    std::string __repr__() const {
        return "TrackResult(id=" + std::to_string(id) + 
               ", x=" + std::to_string(x) + 
               ", y=" + std::to_string(y) + 
               ", width=" + std::to_string(width) + 
               ", height=" + std::to_string(height) + ")";
    }
};

// Python包装类
class PyTracker {
private:
    ExtractorWrapper* tracker_;
    
public:
    PyTracker(const std::string& config_path) {
        // 将std::string转换为char*
        char* config_cstr = new char[config_path.length() + 1];
        strcpy(config_cstr, config_path.c_str());
        
        tracker_ = new ExtractorWrapper(config_cstr);
        delete[] config_cstr;
    }
    
    ~PyTracker() {
        if (tracker_) {
            delete tracker_;
        }
    }
    
    // 处理单帧图像
    std::vector<PyTrackResult> process(py::array_t<unsigned char> img_array) {
        // 转换numpy数组为cv::Mat
        cv::Mat frame = numpy_to_cv_mat(img_array);
        
        // 调用C++追踪器
        std::vector<TrackResult> cpp_results = tracker_->process(frame);
        
        // 转换为Python友好的格式
        std::vector<PyTrackResult> py_results;
        for (const auto& res : cpp_results) {
            py_results.push_back(PyTrackResult(res.id, res.x, res.y, res.width, res.height));
        }
        
        return py_results;
    }
    
    // 处理单帧图像并返回可视化结果
    py::tuple process_with_visualization(py::array_t<unsigned char> img_array) {
        cv::Mat frame = numpy_to_cv_mat(img_array);
        
        // 调用C++追踪器
        std::vector<TrackResult> cpp_results = tracker_->process(frame);
        
        // 绘制边界框和ID
        for (const auto& res : cpp_results) {
            cv::Rect rect(res.x, res.y, res.width, res.height);
            cv::rectangle(frame, rect, cv::Scalar(255, 255, 0), 2);
            
            std::string label = cv::format("%d", res.id);
            cv::putText(frame, label, cv::Point(rect.x, rect.y), 
                       cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(255, 255, 0), 2);
        }
        
        // 转换回numpy数组
        cv::Mat rgb_frame;
        cv::cvtColor(frame, rgb_frame, cv::COLOR_BGR2RGB);
        
        // 创建numpy数组
        py::array_t<unsigned char> result_array = py::array_t<unsigned char>(
            {rgb_frame.rows, rgb_frame.cols, 3},
            rgb_frame.data
        );
        
        // 转换为Python结果
        std::vector<PyTrackResult> py_results;
        for (const auto& res : cpp_results) {
            py_results.push_back(PyTrackResult(res.id, res.x, res.y, res.width, res.height));
        }
        
        return py::make_tuple(py_results, result_array);
    }
};

// Pybind11模块定义
PYBIND11_MODULE(ai_tracker_body, m) {
    m.doc() = "Python interface for object tracker";
    
    // 绑定PyTrackResult类
    py::class_<PyTrackResult>(m, "TrackResult")
        .def(py::init<int, int, int, int, int>())
        .def_readwrite("id", &PyTrackResult::id)
        .def_readwrite("x", &PyTrackResult::x)
        .def_readwrite("y", &PyTrackResult::y)
        .def_readwrite("width", &PyTrackResult::width)
        .def_readwrite("height", &PyTrackResult::height)
        .def("to_dict", &PyTrackResult::to_dict)
        .def("__repr__", &PyTrackResult::__repr__);
    
    // 绑定PyTracker类
    py::class_<PyTracker>(m, "Tracker")
        .def(py::init<const std::string&>())
        .def("process", &PyTracker::process, 
             "Process single frame and return tracking results")
        .def("process_with_visualization", &PyTracker::process_with_visualization,
             "Process frame and return results with visualization");
}