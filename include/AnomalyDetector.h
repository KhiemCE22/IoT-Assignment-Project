#ifndef ANOMALY_DETECTOR_H
#define ANOMALY_DETECTOR_H

#include <Arduino.h>
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "model_data.h" // File chứa mảng model_data[] từ Jupyter

class AnomalyDetector {
public:
    AnomalyDetector();
    bool begin();
    float predict(float* input_data); // Nhận vào mảng 220 phần tử

private:
    const tflite::Model* model = nullptr;
    tflite::MicroInterpreter* interpreter = nullptr;
    TfLiteTensor* input = nullptr;
    TfLiteTensor* output = nullptr;

    // Tensor Arena: Bộ nhớ tạm để chạy mô hình
    static const int kTensorArenaSize = 30 * 1024; 
    uint8_t tensor_arena[kTensorArenaSize];
};

#endif