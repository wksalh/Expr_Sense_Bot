#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>
#include <vector>

// 使用相对路径包含OpenCV
#include "opencv_4.2.0/include/opencv2/opencv.hpp"
#include "ExtractorWrapper.h"

namespace py = pybind11;

// 辅助函数：numpy数组转cv::Mat
cv::Mat numpy_to_cv_mat(py::array_t<unsigned char>& img_array) {
    py::buffer_info buf = img_array.request();
    
    if (buf.ndim != 3)
        throw std::runtime_error("Image must be 3-dimensional (H, W, C)");
    
    int height = buf.shape[0];
    int width = buf.shape[1];
    int channels = buf.shape[2];
    
    if (channels != 3)
        throw std::runtime_error("Image must have 3 channels (RGB)");
    
    cv::Mat mat(height, width, CV_8UC3, buf.ptr);
    cv::cvtColor(mat, mat, cv::COLOR_RGB2BGR);
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
    
    py::dict to_dict() const {
        py::dict d;
        d["id"] = id;
        d["x"] = x;
        d["y"] = y;
        d["width"] = width;
        d["height"] = height;
        return d;
    }
};

// Python包装类
class PyTracker {
private:
    ExtractorWrapper* tracker_;
    
public:
    PyTracker(const std::string& config_path) {
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
        cv::Mat frame = numpy_to_cv_mat(img_array);
        std::vector<TrackResult> cpp_results = tracker_->process(frame);
        
        std::vector<PyTrackResult> py_results;
        for (const auto& res : cpp_results) {
            py_results.push_back(PyTrackResult(res.id, res.x, res.y, res.width, res.height));
        }
        
        return py_results;
    }
};

// Pybind11模块定义
PYBIND11_MODULE(ai_tracker_body, m) {
    m.doc() = "Python interface for object tracker";
    
    py::class_<PyTrackResult>(m, "TrackResult")
        .def(py::init<int, int, int, int, int>())
        .def_readwrite("id", &PyTrackResult::id)
        .def_readwrite("x", &PyTrackResult::x)
        .def_readwrite("y", &PyTrackResult::y)
        .def_readwrite("width", &PyTrackResult::width)
        .def_readwrite("height", &PyTrackResult::height)
        .def("to_dict", &PyTrackResult::to_dict)
        .def("__repr__", [](const PyTrackResult& r) {
            return "TrackResult(id=" + std::to_string(r.id) + 
                   ", x=" + std::to_string(r.x) + 
                   ", y=" + std::to_string(r.y) + 
                   ", width=" + std::to_string(r.width) + 
                   ", height=" + std::to_string(r.height) + ")";
        });
    
    py::class_<PyTracker>(m, "Tracker")
        .def(py::init<const std::string&>())
        .def("process", &PyTracker::process, 
             "Process single frame and return tracking results");
}