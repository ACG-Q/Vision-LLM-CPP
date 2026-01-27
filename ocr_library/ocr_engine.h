#ifndef HOME_AI_OCR_ENGINE_H
#define HOME_AI_OCR_ENGINE_H

#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT __attribute__((visibility("default")))
#endif

extern "C" {
    // Initialize the OCR engine with model paths
    EXPORT int init_ocr_engine(const char* det_path, const char* rec_path, const char* keys_path);

    // Perform OCR on an image file
    // Returns a JSON string of recognized results (must be freed by the caller)
    EXPORT char* perform_ocr(const char* image_path);

    // Free the string returned by perform_ocr
    EXPORT void free_ocr_result(char* result);
}

#endif // HOME_AI_OCR_ENGINE_H
