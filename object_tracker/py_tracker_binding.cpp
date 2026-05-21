#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>
#include <opencv2/opencv.hpp>
#include "object_tracker.h"
#include <cstring>

namespace py = pybind11;

// 将numpy数组（RGB uint8）转换为cv::Mat（BGR）
cv::Mat numpy_to_cv_mat(py::array_t<uint8_t>& img_array) {
    py::buffer_info buf = img_array.request();
    if (buf.ndim != 3) throw std::runtime_error("Input must be HWC 3-channel image");
    int h = buf.shape[0], w = buf.shape[1], c = buf.shape[2];
    if (c != 3) throw std::runtime_error("Input must have 3 channels");
    cv::Mat mat(h, w, CV_8UC3, buf.ptr);
    cv::cvtColor(mat, mat, cv::COLOR_RGB2BGR);
    return mat.clone();
}

// 将cv::Mat（BGR）转换为numpy数组（RGB）
py::array_t<uint8_t> cv_mat_to_numpy(const cv::Mat& bgr_mat) {
    cv::Mat rgb_mat;
    cv::cvtColor(bgr_mat, rgb_mat, cv::COLOR_BGR2RGB);
    return py::array_t<uint8_t>({rgb_mat.rows, rgb_mat.cols, 3}, rgb_mat.data);
}

// 辅助：std::string -> char* (可修改)
char* string_to_char_ptr(const std::string& str) {
    char* ptr = new char[str.size() + 1];
    std::strcpy(ptr, str.c_str());
    return ptr;
}

// Python包装类
class PyTracker {
private:
    Tracker* tracker_;

public:
    PyTracker() {
        tracker_ = new Tracker();
    }

    ~PyTracker() {
        if (tracker_) {
            tracker_->uninit_model();
            delete tracker_;
        }
    }

    void set_param(float conf_thresh, float iou_thresh, float score_thresh,
                   int nn_budget, float max_cosine_distance, bool use_reid) {
        tracker_->set_param(conf_thresh, iou_thresh, score_thresh,
                            nn_budget, max_cosine_distance, use_reid);
    }

    bool init(const std::string& det_model, const std::string& track_model,
              const std::string& license_file, int device_id, const std::string& auth_code) {
        char* det_cstr = string_to_char_ptr(det_model);
        char* track_cstr = string_to_char_ptr(track_model);
        char* license_cstr = string_to_char_ptr(license_file);
        char* auth_cstr = string_to_char_ptr(auth_code);

        TRACK_RESULT_CODE ret = tracker_->init(det_cstr, track_cstr, license_cstr, device_id, auth_cstr);

        delete[] det_cstr;
        delete[] track_cstr;
        delete[] license_cstr;
        delete[] auth_cstr;

        return ret == PROCESS_OK;
    }

    int get_status(int opt) {
        return tracker_->get_status(opt);
    }

    // 处理单帧图像，返回检测/跟踪结果
    std::vector<TrackResults> process(py::array_t<uint8_t> img_array) {
        cv::Mat frame = numpy_to_cv_mat(img_array);
        if (!frame.isContinuous()) frame = frame.clone();
        unsigned char* data = frame.data;
        int rows = frame.rows, cols = frame.cols, channels = frame.channels();

        std::vector<TrackResults> results;
        TRACK_RESULT_CODE ret = tracker_->inference(data, rows, cols, channels, results);
        if (ret != PROCESS_OK) {
            throw std::runtime_error("Inference failed with code: " + std::to_string(ret));
        }
        return results;
    }

    // 处理并返回带可视化的图像
    py::tuple process_with_visualization(py::array_t<uint8_t> img_array) {
        cv::Mat frame = numpy_to_cv_mat(img_array);
        if (!frame.isContinuous()) frame = frame.clone();
        unsigned char* data = frame.data;
        int rows = frame.rows, cols = frame.cols, channels = frame.channels();

        std::vector<TrackResults> results;
        TRACK_RESULT_CODE ret = tracker_->inference(data, rows, cols, channels, results);
        if (ret != PROCESS_OK) {
            throw std::runtime_error("Inference failed with code: " + std::to_string(ret));
        }

        // 绘制结果
        for (const auto& res : results) {
            cv::Rect rect(res.x, res.y, res.width, res.height);
            cv::rectangle(frame, rect, cv::Scalar(0, 255, 255), 2);
            std::string label = std::to_string(res.id);
            cv::putText(frame, label, cv::Point(rect.x, rect.y),
                        cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 255, 255), 2);
        }
        py::array_t<uint8_t> vis_array = cv_mat_to_numpy(frame);
        return py::make_tuple(results, vis_array);
    }

    void get_benchmark_info(py::array_t<uint8_t> img_array, int repeat_times = 100) {
        cv::Mat frame = numpy_to_cv_mat(img_array);
        tracker_->get_benchmark_info(frame, repeat_times);
    }

    void save_result(py::array_t<uint8_t> img_array, std::vector<TrackResults>& results,
                     std::string& output_path, const std::string& image_name) {
        cv::Mat frame = numpy_to_cv_mat(img_array);
        tracker_->save_result(frame, results, output_path, image_name);
    }
};

// 绑定结构体 TrackResults
void define_track_results(py::module& m) {
    py::class_<TrackResults>(m, "TrackResults")
        .def_readwrite("id", &TrackResults::id)
        .def_readwrite("x", &TrackResults::x)
        .def_readwrite("y", &TrackResults::y)
        .def_readwrite("width", &TrackResults::width)
        .def_readwrite("height", &TrackResults::height)
        .def_readwrite("confidence", &TrackResults::confidence)
        .def("__repr__", [](const TrackResults& r) {
            return "<TrackResults id=" + std::to_string(r.id) +
                   " x=" + std::to_string(r.x) +
                   " y=" + std::to_string(r.y) +
                   " w=" + std::to_string(r.width) +
                   " h=" + std::to_string(r.height) +
                   " conf=" + std::to_string(r.confidence) + ">";
        });
}

// 模块定义
PYBIND11_MODULE(tracker_binding, m) {
    m.doc() = "Python binding for object tracker";
    define_track_results(m);

    py::class_<PyTracker>(m, "Tracker")
        .def(py::init<>())
        .def("set_param", &PyTracker::set_param,
             py::arg("conf_thresh"), py::arg("iou_thresh"), py::arg("score_thresh"),
             py::arg("nn_budget"), py::arg("max_cosine_distance"), py::arg("use_reid"))
        .def("init", &PyTracker::init,
             py::arg("det_model"), py::arg("track_model"),
             py::arg("license_file"), py::arg("device_id"), py::arg("auth_code"))
        .def("get_status", &PyTracker::get_status)
        .def("process", &PyTracker::process)
        .def("process_with_visualization", &PyTracker::process_with_visualization)
        .def("get_benchmark_info", &PyTracker::get_benchmark_info)
        .def("save_result", &PyTracker::save_result);
}