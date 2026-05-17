#ifndef __TINYML_TASK__
#define __TINYML_TASK__
#include <Arduino.h>

// Cấu trúc để nhận dữ liệu từ Queue cảm biến
struct RawSensorData {
    float temperature;
    float humidity;
};

extern QueueHandle_t sensorQueue;

void tinyMLTask(void *pvParameters);

#endif