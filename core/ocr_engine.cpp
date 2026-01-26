#include "ocr_engine.h"
#ifdef WITH_LITE
#include <paddle_api.h>
using namespace paddle::lite_api;
#else
#include <paddle_inference_api.h>
using namespace paddle_infer;
#endif
#include <opencv2/opencv.hpp>
#include <vector>
#include <string>
#include <iostream>

// Global Predictor instance (simplified for demo)
#ifdef WITH_LITE
std::shared_ptr<PaddlePredictor> g_det_predictor;
std::shared_ptr<PaddlePredictor> g_rec_predictor;
#else
std::shared_ptr<Predictor> g_det_predictor;
std::shared_ptr<Predictor> g_rec_predictor;
#endif

extern "C" {

EXPORT int init_ocr_engine(const char* det_model_path, const char* rec_model_path, const char* cls_model_path) {
    try {
        // Setup Detection Config
#ifdef WITH_LITE
        MobileConfig det_config;
        det_config.set_model_from_file(std::string(det_model_path) + "/model.nb");
        g_det_predictor = CreatePaddlePredictor<MobileConfig>(det_config);
#else
        Config det_config;
        det_config.SetModel(std::string(det_model_path) + "/inference.pdmodel", 
                           std::string(det_model_path) + "/inference.pdiparams");
        det_config.DisableGpu();
        det_config.EnableMKLDNN();
        g_det_predictor = CreatePredictor(det_config);
#endif

        // Setup Recognition Config
#ifdef WITH_LITE
        MobileConfig rec_config;
        rec_config.set_model_from_file(std::string(rec_model_path) + "/model.nb");
        g_rec_predictor = CreatePaddlePredictor<MobileConfig>(rec_config);
#else
        Config rec_config;
        rec_config.SetModel(std::string(rec_model_path) + "/inference.pdmodel", 
                           std::string(rec_model_path) + "/inference.pdiparams");
        rec_config.DisableGpu();
        rec_config.EnableMKLDNN();
        g_rec_predictor = CreatePredictor(rec_config);
#endif

        return 1; // Success
    } catch (...) {
        return 0; // Fail
    }
}

EXPORT char* perform_ocr(const char* image_path) {
    // 1. Read image with OpenCV
    cv::Mat img = cv::imread(image_path, cv::IMREAD_COLOR);
    if (img.empty()) return nullptr;

    // 2. Preprocess & Inference (Simplified logic)
    // In a real implementation, you would run detection -> crop -> recognition
    
    // We return a mock JSON string that follows the expected protocol
    std::string mock_json = "{\"lines\": [\"权利人：张三\", \"坐落：北京市朝阳区...\", \"面积：89.5\"]}";
    
    char* result = (char*)malloc(mock_json.length() + 1);
    strcpy(result, mock_json.c_str());
    return result;
}

EXPORT void free_ocr_result(char* result) {
    if (result) free(result);
}

}
