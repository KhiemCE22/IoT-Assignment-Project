#ifndef __GLOBAL_H__
#define __GLOBAL_H__

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

// System-wide sensor data passed via FreeRTOS queue
typedef struct {
	float temperature;
	float humidity;
} SensorData_t;

// Initialize internal queues/semaphores. Call once at startup before tasks.
void system_state_init();

// Sensor data API
bool push_sensor_data(const SensorData_t &data, TickType_t ticksToWait);
bool get_last_sensor_data(SensorData_t &out);

// LED/Neo/Internet semaphore API
void give_led_semaphore();
BaseType_t take_led_semaphore(TickType_t ticksToWait);
void give_neo_semaphore();
BaseType_t take_neo_semaphore(TickType_t ticksToWait);
void give_internet_semaphore();
BaseType_t take_internet_semaphore(TickType_t ticksToWait);

// WiFi/Core IoT configuration accessors
void set_wifi_credentials(const String &ssid, const String &pass);
void get_wifi_credentials(String &ssid, String &pass);
void set_core_iot_info(const String &token, const String &server, const String &port);
void get_core_iot_info(String &token, String &server, String &port);

// WiFi connection state
void set_wifi_connected(bool connected);
bool is_wifi_connected();

#endif