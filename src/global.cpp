#include "global.h"
float glob_temperature = 0;
float glob_humidity = 0;

String WIFI_SSID;
String WIFI_PASS;
String CORE_IOT_TOKEN;
String CORE_IOT_SERVER;
String CORE_IOT_PORT;

String ssid = "ESP32-YOUR NETWORK HERE!!!";
String password = "12345678";
String wifi_ssid = "Saigonhome 1 - 2.4G";
String wifi_password = "13681368";
boolean isWifiConnected = false;
SemaphoreHandle_t xBinarySemaphoreInternet = xSemaphoreCreateBinary();
// Semaphore used to notify the LED task that a new temperature
// sample is available and it should re-evaluate its blink pattern.
SemaphoreHandle_t xSemaphoreLED = xSemaphoreCreateBinary();
// Semaphore used to notify the NeoPixel task that a new humidity
// sample is available and it should update its color.
SemaphoreHandle_t xSemaphoreNeo = xSemaphoreCreateBinary();