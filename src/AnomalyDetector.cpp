#include "AnomalyDetector.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"

AnomalyDetector::AnomalyDetector() {}

bool AnomalyDetector::begin() {
    // 1. Khởi tạo báo lỗi
    static tflite::MicroErrorReporter micro_error_reporter;

    // 2. Load mô hình từ bộ nhớ Flash
    model = tflite::GetModel(model_data);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        Serial.printf("[!] Model version mismatch! Expected %d, got %d\n", 
                      TFLITE_SCHEMA_VERSION, model->version());
        return false;
    }
    Serial.println("[+] Model loaded successfully.");

    // 3. Đăng ký các toán tử (Ops) - PHẢI KHỚP VỚI KERAS MODEL
    // Chúng ta cần 5 toán tử: Conv2D, MaxPool2D, Reshape (cho Flatten), FullyConnected (cho Dense), Logistic (cho Sigmoid)
    static tflite::MicroMutableOpResolver<5> resolver;
    resolver.AddConv2D();
    resolver.AddMaxPool2D();
    resolver.AddReshape();        // Cần thiết cho lớp layers.Flatten()
    resolver.AddFullyConnected(); // Cần thiết cho các lớp layers.Dense()
    resolver.AddLogistic();       // Cần thiết cho activation='sigmoid'

    // 4. Khởi tạo trình thông dịch (Interpreter)
    static tflite::MicroInterpreter static_interpreter(
        model, resolver, tensor_arena, kTensorArenaSize, &micro_error_reporter
    );
    interpreter = &static_interpreter;

    // 5. Cấp phát bộ nhớ cho các Tensors
    if (interpreter->AllocateTensors() != kTfLiteOk) {
        Serial.println("[!] AllocateTensors failed! Try increasing kTensorArenaSize.");
        return false;
    }
    Serial.println("[+] Tensors allocated.");

    // 6. Trỏ tới Tensor đầu vào và đầu ra
    input = interpreter->input(0);
    output = interpreter->output(0);

    Serial.printf("[+] Input Shape: %d dim, Size: [%d, %d, %d, %d]\n", 
                  input->dims->size, input->dims->data[0], input->dims->data[1], 
                  input->dims->data[2], input->dims->data[3]);
    Serial.printf("[+] Output Shape: %d dim, Size: [%d]\n", 
                  output->dims->size, output->dims->data[1]);

    return true;
}

float AnomalyDetector::predict(float* input_data) {
    // Copy 220 giá trị đặc trưng (20 frame x 11 feature) vào input tensor
    for (int i = 0; i < 220; i++) {
        input->data.f[i] = input_data[i];
    }

    // Thực hiện suy luận
    TfLiteStatus invoke_status = interpreter->Invoke();
    if (invoke_status != kTfLiteOk) {
        Serial.printf("[-] Invoke failed! Status code: %d\n", invoke_status);
        return -1.0f; 
    }

    // Trả về giá trị xác suất từ 0.0 đến 1.0 (nhãn Sigmoid)
    return output->data.f[0];
}