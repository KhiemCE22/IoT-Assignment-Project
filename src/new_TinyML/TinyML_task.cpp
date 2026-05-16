#include <Arduino.h>
#include "AnomalyDetector.h"
#include "AIFeatureEngine.h"
#include "TinyML_task.h"

QueueHandle_t sensorQueue;

void tinyMLTask(void *pvParameters) {

    setupTime();
    sensorQueue = xQueueCreate(10, sizeof(RawSensorData));
    
    // 1. Khởi tạo các đối tượng cục bộ bên trong Task (Không biến toàn cục)
    static AnomalyDetector detector;
    static FeatureEngine engine;
    
    // Khởi tạo detector (nạp model, phân bổ tensor)
    if (!detector.begin()) {
        Serial.println("[-] AI Detector initialization failed!");
        vTaskDelete(NULL);
    }

    RawSensorData receivedData;

    Serial.println("[+] tinyMLTask is running...");

    while (1) {
        // 2. Chờ nhận dữ liệu từ Queue cảm biến
        // Ở đây ta chờ tối đa 2 giây, nếu không có dữ liệu thì loop lại
        if (xQueueReceive(sensorQueue, &receivedData, pdMS_TO_TICKS(2000)) == pdPASS) {

            // Serial.print("Humidity: ");
            // Serial.print(receivedData.humidity);
            // Serial.print("%  Temperature: ");
            // Serial.print(receivedData.temperature);
            // Serial.println("°C");
            
            // 3. Tiền xử lý: Biến đổi 2 thông số thô thành mảng phẳng 220 đặc trưng
            // Hàm này tự thực hiện: Median Filter, Gradient, Std Dev, FFT và Scaling
            float* input_tensor = engine.get_ai_input(
                receivedData.temperature, 
                receivedData.humidity
            );

            // 4. Suy luận (Inference)
            unsigned long start_time = millis();
            float anomaly_score = detector.predict(input_tensor);
            unsigned long inference_time = millis() - start_time;

            Serial.print("[+] AI inference result: anomaly_score = ");
            Serial.println(anomaly_score);

            // 5. Xử lý kết quả dựa trên ngưỡng 0.5 (Sigmoid)
            if (anomaly_score >= 0.0f) { // Kiểm tra nếu không có lỗi (-1.0f)
                if (anomaly_score > 0.5f) {
                    Serial.printf("!!! CẢNH BÁO: Phát hiện bất thường (%.4f) !!!\n", anomaly_score);
                    // Gửi lệnh bật chuông hoặc ngắt thiết bị tại đây
                } else {
                    Serial.printf("[OK] Trạng thái bình thường (%.4f)\n", anomaly_score);
                }
                
                // Log hiệu suất để đánh giá (Nhiệm vụ 5)
                Serial.printf("   > Thời gian suy luận: %lu ms\n", inference_time);
            }
            else {
                Serial.println("[!] Predict failed due to invalid input or inference error.");
            }
            
            Serial.println("-----------------------------------");
        }
        
        // Task delay để nhường tài nguyên cho các Task khác
        vTaskDelay(pdMS_TO_TICKS(10)); 
    }
}
