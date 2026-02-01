#include "ocr_engine.h"
#include <opencv2/opencv.hpp>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <memory>
#include <mutex>
#include <numeric>
#include <algorithm>
#include <math.h>
#include "clipper.h"

#ifdef WITH_LITE
#include <paddle_api.h>
using namespace paddle::lite_api;
#else
#include <paddle_inference_api.h>
using namespace paddle_infer;
#endif

namespace PaddleOCR {

// --- Utilities ---
class Utility {
public:
    static std::vector<std::string> ReadDict(const std::string &path) {
        std::ifstream in(path);
        std::string line;
        std::vector<std::string> m_vec;
        if (in) {
            while (getline(in, line)) {
                m_vec.push_back(line);
            }
        }
        return m_vec;
    }

    static int argmax(const float *start, const float *end) {
        return std::distance(start, std::max_element(start, end));
    }

    static float clamp(float x, float min, float max) {
        if (x > max) return max;
        if (x < min) return min;
        return x;
    }
};

// --- Preprocessing ---
class Preprocessor {
public:
    static void Normalize(cv::Mat *im, const std::vector<float> &mean,
                          const std::vector<float> &scale, const bool is_scale) {
        double e = is_scale ? 1.0 / 255.0 : 1.0;
        im->convertTo(*im, CV_32FC3, e);
        for (int h = 0; h < im->rows; h++) {
            for (int w = 0; w < im->cols; w++) {
                cv::Vec3f &val = im->at<cv::Vec3f>(h, w);
                val[0] = (val[0] - mean[0]) * scale[0];
                val[1] = (val[1] - mean[1]) * scale[1];
                val[2] = (val[2] - mean[2]) * scale[2];
            }
        }
    }

    static void Permute(const cv::Mat *im, float *data) {
        int rh = im->rows;
        int rw = im->cols;
        int rc = im->channels();
        for (int i = 0; i < rc; ++i) {
            cv::extractChannel(*im, cv::Mat(rh, rw, CV_32FC1, data + i * rh * rw), i);
        }
    }

    static void ResizeDet(const cv::Mat &img, cv::Mat &resize_img, int max_size_len, float &ratio_h, float &ratio_w) {
        int w = img.cols;
        int h = img.rows;
        float ratio = 1.f;
        int max_wh = std::max(w, h);
        if (max_wh > max_size_len) {
            ratio = float(max_size_len) / float(max_wh);
        }
        int resize_h = int(float(h) * ratio);
        int resize_w = int(float(w) * ratio);
        resize_h = std::max(32, (resize_h / 32) * 32);
        resize_w = std::max(32, (resize_w / 32) * 32);
        cv::resize(img, resize_img, cv::Size(resize_w, resize_h));
        ratio_h = float(resize_h) / float(h);
        ratio_w = float(resize_w) / float(w);
    }

    static void ResizeRec(const cv::Mat &img, cv::Mat &resize_img, int rec_h, int rec_w) {
        float ratio = float(img.cols) / float(img.rows);
        int w = int(ceilf(float(rec_h) * ratio));
        if (w > rec_w) w = rec_w;
        cv::resize(img, resize_img, cv::Size(w, rec_h), 0.f, 0.f, cv::INTER_LINEAR);
        if (w < rec_w) {
            cv::copyMakeBorder(resize_img, resize_img, 0, 0, 0, rec_w - w, cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0));
        }
    }
};

// --- Postprocessing ---
class DBPostProcessor {
public:
    static std::vector<std::vector<int>> OrderPointsClockwise(std::vector<std::vector<int>> pts) {
        std::vector<std::vector<int>> box = pts;
        std::sort(box.begin(), box.end(), [](const std::vector<int> &a, const std::vector<int> &b) {
            return a[0] < b[0];
        });
        std::vector<std::vector<int>> leftmost = {box[0], box[1]};
        std::vector<std::vector<int>> rightmost = {box[2], box[3]};
        if (leftmost[0][1] > leftmost[1][1]) std::swap(leftmost[0], leftmost[1]);
        if (rightmost[0][1] > rightmost[1][1]) std::swap(rightmost[0], rightmost[1]);
        return {leftmost[0], rightmost[0], rightmost[1], leftmost[1]};
    }

    static std::vector<std::vector<float>> GetMiniBoxes(cv::RotatedRect box, float &ssid) {
        ssid = std::max(box.size.width, box.size.height);
        cv::Mat points;
        cv::boxPoints(box, points);
        std::vector<std::vector<float>> array;
        for (int i = 0; i < 4; i++) {
            array.push_back({points.at<float>(i, 0), points.at<float>(i, 1)});
        }
        std::sort(array.begin(), array.end(), [](const std::vector<float> &a, const std::vector<float> &b) {
            return a[0] < b[0];
        });
        std::vector<float> idx1, idx2, idx3, idx4;
        if (array[3][1] <= array[2][1]) { idx2 = array[3]; idx3 = array[2]; } else { idx2 = array[2]; idx3 = array[3]; }
        if (array[1][1] <= array[0][1]) { idx1 = array[1]; idx4 = array[0]; } else { idx1 = array[0]; idx4 = array[1]; }
        return {idx1, idx2, idx3, idx4};
    }

    static float BoxScoreFast(const std::vector<std::vector<float>> &box, const cv::Mat &pred) {
        int width = pred.cols;
        int height = pred.rows;
        float box_x[4] = {box[0][0], box[1][0], box[2][0], box[3][0]};
        float box_y[4] = {box[0][1], box[1][1], box[2][1], box[3][1]};
        int xmin = std::max(0, (int)floor(*std::min_element(box_x, box_x + 4)));
        int xmax = std::min(width - 1, (int)ceil(*std::max_element(box_x, box_x + 4)));
        int ymin = std::max(0, (int)floor(*std::min_element(box_y, box_y + 4)));
        int ymax = std::min(height - 1, (int)ceil(*std::max_element(box_y, box_y + 4)));
        cv::Mat mask = cv::Mat::zeros(ymax - ymin + 1, xmax - xmin + 1, CV_8UC1);
        cv::Point rook_points[4];
        for (int i = 0; i < 4; i++) rook_points[i] = cv::Point((int)box[i][0] - xmin, (int)box[i][1] - ymin);
        const cv::Point *ppt[1] = {rook_points};
        int npt[] = {4};
        cv::fillPoly(mask, ppt, npt, 1, cv::Scalar(1));
        return cv::mean(pred(cv::Rect(xmin, ymin, xmax - xmin + 1, ymax - ymin + 1)), mask)[0];
    }

    static std::vector<std::vector<std::vector<int>>> BoxesFromBitmap(const cv::Mat &pred, const cv::Mat &bitmap, float box_thresh, float unclip_ratio) {
        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(bitmap, contours, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);
        std::vector<std::vector<std::vector<int>>> boxes;
        for (auto &contour : contours) {
            if (contour.size() <= 2) continue;
            float ssid;
            cv::RotatedRect rect = cv::minAreaRect(contour);
            auto box = GetMiniBoxes(rect, ssid);
            if (ssid < 3) continue;
            if (BoxScoreFast(box, pred) < box_thresh) continue;
            
            // Unclip
            float area = 0, dist = 0;
            for (int i = 0; i < 4; i++) {
                area += box[i][0] * box[(i + 1) % 4][1] - box[i][1] * box[(i + 1) % 4][0];
                dist += sqrtf(powf(box[i][0] - box[(i + 1) % 4][0], 2) + powf(box[i][1] - box[(i + 1) % 4][1], 2));
            }
            float distance = fabsf(area) * unclip_ratio / dist;
            ClipperLib::ClipperOffset offset;
            ClipperLib::Path p;
            for (int i = 0; i < 4; i++) p << ClipperLib::IntPoint((int)box[i][0], (int)box[i][1]);
            offset.AddPath(p, ClipperLib::jtRound, ClipperLib::etClosedPolygon);
            ClipperLib::Paths soln;
            offset.Execute(soln, distance);
            if (soln.empty()) continue;
            std::vector<cv::Point2f> points;
            for (auto &pt : soln[0]) points.emplace_back(pt.X, pt.Y);
            cv::RotatedRect unclip_rect = cv::minAreaRect(points);
            auto unclip_box = GetMiniBoxes(unclip_rect, ssid);
            if (ssid < 5) continue;
            
            std::vector<std::vector<int>> int_box;
            for (int i = 0; i < 4; i++) {
                int_box.push_back({(int)Utility::clamp(roundf(unclip_box[i][0]), 0, (float)pred.cols),
                                   (int)Utility::clamp(roundf(unclip_box[i][1]), 0, (float)pred.rows)});
            }
            boxes.push_back(OrderPointsClockwise(int_box));
        }
        return boxes;
    }
};

// --- Main Analyzer ---
class OCRAnalyzer {
public:
    OCRAnalyzer(const std::string &det_model_path, const std::string &rec_model_path, const std::string &keys_path) {
#ifdef WITH_LITE
        MobileConfig det_config;
        det_config.set_model_from_file(det_model_path + "/model.nb");
        det_predictor = CreatePaddlePredictor<MobileConfig>(det_config);

        MobileConfig rec_config;
        rec_config.set_model_from_file(rec_model_path + "/model.nb");
        rec_predictor = CreatePaddlePredictor<MobileConfig>(rec_config);
#else
        Config det_config;
        det_config.SetModel(det_model_path + "/inference.pdmodel", det_model_path + "/inference.pdiparams");
        det_config.DisableGpu();
        det_config.EnableMKLDNN();
        det_predictor = CreatePredictor(det_config);

        Config rec_config;
        rec_config.SetModel(rec_model_path + "/inference.pdmodel", rec_model_path + "/inference.pdiparams");
        rec_config.DisableGpu();
        rec_config.EnableMKLDNN();
        rec_predictor = CreatePredictor(rec_config);
#endif
        label_list = Utility::ReadDict(keys_path);
        label_list.push_back(" ");
    }

    std::string Run(const std::string &img_path) {
#ifdef _WIN32
        // Support Unicode paths on Windows
        std::ifstream fs(img_path, std::ios::binary);
        if (!fs) return "{\"error\":\"cannot open file stream\"}";
        std::vector<unsigned char> data((std::istreambuf_iterator<char>(fs)), std::istreambuf_iterator<char>());
        cv::Mat img = cv::imdecode(data, cv::IMREAD_COLOR);
#else
        cv::Mat img = cv::imread(img_path, cv::IMREAD_COLOR);
#endif
        if (img.empty()) return "{\"error\":\"cannot decode image\"}";

        // 1. Detection
        cv::Mat det_img;
        float ratio_h, ratio_w;
        Preprocessor::ResizeDet(img, det_img, 960, ratio_h, ratio_w);
        Preprocessor::Normalize(&det_img, {0.485f, 0.456f, 0.406f}, {1/0.229f, 1/0.224f, 1/0.225f}, true);
        std::vector<float> det_input(1 * 3 * det_img.rows * det_img.cols);
        Preprocessor::Permute(&det_img, det_input.data());

#ifdef WITH_LITE
        auto det_in_t = det_predictor->GetInput(0);
        det_in_t->Resize({1, 3, det_img.rows, det_img.cols});
        float* det_in_ptr = det_in_t->mutable_data<float>();
        memcpy(det_in_ptr, det_input.data(), sizeof(float) * det_input.size());
        det_predictor->Run();
        auto det_out_t = det_predictor->GetOutput(0);
        const float* det_out_ptr = det_out_t->data<float>();
        cv::Mat pred(det_out_t->shape()[2], det_out_t->shape()[3], CV_32F, (float*)det_out_ptr);
#else
        auto det_in_names = det_predictor->GetInputNames();
        auto det_in_t = det_predictor->GetInputHandle(det_in_names[0]);
        det_in_t->Reshape({1, 3, det_img.rows, det_img.cols});
        det_in_t->CopyFromCpu(det_input.data());
        det_predictor->Run();
        auto det_out_names = det_predictor->GetOutputNames();
        auto det_out_t = det_predictor->GetOutputHandle(det_out_names[0]);
        std::vector<int> det_out_shape = det_out_t->shape();
        std::vector<float> det_out_data(det_out_shape[2] * det_out_shape[3]);
        det_out_t->CopyToCpu(det_out_data.data());
        cv::Mat pred(det_out_shape[2], det_out_shape[3], CV_32F, det_out_data.data());
#endif

        cv::Mat bitmap;
        cv::threshold(pred, bitmap, 0.3, 255, cv::THRESH_BINARY);
        bitmap.convertTo(bitmap, CV_8U);
        auto boxes = DBPostProcessor::BoxesFromBitmap(pred, bitmap, 0.5, 2.0);

        // Scale boxes back
        for (auto &box : boxes) {
            for (auto &pt : box) {
                pt[0] = (int)(pt[0] / ratio_w);
                pt[1] = (int)(pt[1] / ratio_h);
            }
        }

        // 2. Recognition
        std::string result_json = "{\"lines\": [";
        for (size_t i = 0; i < boxes.size(); i++) {
            cv::Mat crop_img = GetRotateCropImage(img, boxes[i]);
            cv::Mat rec_img;
            Preprocessor::ResizeRec(crop_img, rec_img, 48, 320);
            Preprocessor::Normalize(&rec_img, {0.5f, 0.5f, 0.5f}, {1/0.5f, 1/0.5f, 1/0.5f}, true);
            std::vector<float> rec_input(1 * 3 * rec_img.rows * rec_img.cols);
            Preprocessor::Permute(&rec_img, rec_input.data());

#ifdef WITH_LITE
            auto rec_in_t = rec_predictor->GetInput(0);
            rec_in_t->Resize({1, 3, rec_img.rows, rec_img.cols});
            float* rec_in_ptr = rec_in_t->mutable_data<float>();
            memcpy(rec_in_ptr, rec_input.data(), sizeof(float) * rec_input.size());
            rec_predictor->Run();
            auto rec_out_t = rec_predictor->GetOutput(0);
            auto rec_shape = rec_out_t->shape();
            const float* rec_out_ptr = rec_out_t->data<float>();
            std::vector<float> rec_out_data(std::accumulate(rec_shape.begin(), rec_shape.end(), 1, std::multiplies<int64_t>()));
            memcpy(rec_out_data.data(), rec_out_ptr, sizeof(float) * rec_out_data.size());
#else
            auto rec_in_names = rec_predictor->GetInputNames();
            auto rec_in_t = rec_predictor->GetInputHandle(rec_in_names[0]);
            rec_in_t->Reshape({1, 3, rec_img.rows, rec_img.cols});
            rec_in_t->CopyFromCpu(rec_input.data());
            rec_predictor->Run();
            auto rec_out_names = rec_predictor->GetOutputNames();
            auto rec_out_t = rec_predictor->GetOutputHandle(rec_out_names[0]);
            auto rec_shape = rec_out_t->shape();
            std::vector<float> rec_out_data(std::accumulate(rec_shape.begin(), rec_shape.end(), 1, std::multiplies<int>()));
            rec_out_t->CopyToCpu(rec_out_data.data());
#endif

            // CTC Decode
            std::string text = "";
            float score = 0;
            int count = 0;
            int last_idx = -1;
            for (int n = 0; n < rec_shape[1]; n++) {
                int argmax_idx = Utility::argmax(&rec_out_data[n * rec_shape[2]], &rec_out_data[(n + 1) * rec_shape[2]]);
                float max_val = rec_out_data[n * rec_shape[2] + argmax_idx];
                if (argmax_idx > 0 && argmax_idx < label_list.size() && argmax_idx != last_idx) {
                    text += label_list[argmax_idx - 1];
                    score += max_val;
                    count++;
                }
                last_idx = argmax_idx;
            }
            if (count > 0) score /= count;

            result_json += "\"" + text + "\"" + (i == boxes.size() - 1 ? "" : ",");
        }
        result_json += "]}";
        return result_json;
    }

private:
#ifdef WITH_LITE
    std::shared_ptr<PaddlePredictor> det_predictor;
    std::shared_ptr<PaddlePredictor> rec_predictor;
#else
    std::shared_ptr<Predictor> det_predictor;
    std::shared_ptr<Predictor> rec_predictor;
#endif
    std::vector<std::string> label_list;

    cv::Mat GetRotateCropImage(const cv::Mat &src, const std::vector<std::vector<int>> &box) {
        cv::Point2f pointsf[4];
        for (int i = 0; i < 4; i++) pointsf[i] = cv::Point2f(box[i][0], box[i][1]);
        int width = (int)sqrt(pow(box[0][0] - box[1][0], 2) + pow(box[0][1] - box[1][1], 2));
        int height = (int)sqrt(pow(box[0][0] - box[3][0], 2) + pow(box[0][1] - box[3][1], 2));
        cv::Point2f pts_std[4] = { {0,0}, {(float)width,0}, {(float)width,(float)height}, {0,(float)height} };
        cv::Mat M = cv::getPerspectiveTransform(pointsf, pts_std);
        cv::Mat dst;
        cv::warpPerspective(src, dst, M, cv::Size(width, height), cv::BORDER_REPLICATE);
        if (dst.rows >= dst.cols * 1.5) {
            cv::transpose(dst, dst);
            cv::flip(dst, dst, 0);
        }
        return dst;
    }
};

} // namespace PaddleOCR

static std::shared_ptr<PaddleOCR::OCRAnalyzer> g_analyzer;
static std::mutex g_ocr_mutex;

extern "C" {

EXPORT int init_ocr_engine(const char* det_path, const char* rec_path, const char* keys_path) {
    std::lock_guard<std::mutex> lock(g_ocr_mutex);
    try {
        g_analyzer = std::make_shared<PaddleOCR::OCRAnalyzer>(det_path, rec_path, keys_path);
        return 1;
    } catch (const std::exception &e) {
        std::cerr << "OCR Init Failed: " << e.what() << std::endl;
        return 0;
    } catch (...) {
        return 0;
    }
}

EXPORT char* perform_ocr(const char* image_path) {
    std::lock_guard<std::mutex> lock(g_ocr_mutex);
    if (!g_analyzer) return nullptr;
    try {
        std::string result = g_analyzer->Run(image_path);
        char* c_res = (char*)malloc(result.length() + 1);
        if (c_res) strcpy(c_res, result.c_str());
        return c_res;
    } catch (...) {
        return nullptr;
    }
}

EXPORT void free_ocr_result(char* result) {
    if (result) free(result);
}

}
