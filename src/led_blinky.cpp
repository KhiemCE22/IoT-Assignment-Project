#include "led_blinky.h"
#include "global.h"
/*
  led_blinky task

  Behavior summary (temperature-driven):
  - Temp < 20°C   : Slow blink 2s ON / 2s OFF
  - 20°C - 30°C   : Normal blink 1s ON / 1s OFF
  - 30°C - 40°C   : Fast blink 200ms ON / 200ms OFF
  - >= 40°C       : Alert pattern: 100ms ON / 100ms OFF (very fast)

  Synchronization:
  - `temp_humi_monitor` gives `xSemaphoreLED` each time it updates
    `glob_temperature` (every ~5s). The LED task checks the semaphore
    (non-blocking) and updates its blink parameters when a new sample
    is available. This decouples sensor sampling from LED timing and
    uses a binary semaphore to notify the LED task about condition changes.

  Implementation notes:
  - The blink loop uses small delays (50ms) and tick-based timing so
    the task can remain responsive to semaphore notifications while
    performing blink timing.
*/

void led_blinky(void *pvParameters){
  pinMode(LED_GPIO, OUTPUT);

  // default blink timings (in ms)
  TickType_t blink_on_ticks = pdMS_TO_TICKS(1000);
  TickType_t blink_off_ticks = pdMS_TO_TICKS(1000);

  bool ledState = false; // false = OFF, true = ON
  TickType_t lastToggle = xTaskGetTickCount();

  while(1) {
    // If a new temperature sample is available, update the blink pattern.
    if (take_led_semaphore(0) == pdTRUE) {
      SensorData_t sd;
      float t = NAN;
      if (get_last_sensor_data(sd)) t = sd.temperature;

      // Choose blink pattern based on temperature ranges
      if (t < 0) {
        // sensor error/uninitialized: very slow heartbeat
        blink_on_ticks = pdMS_TO_TICKS(200);
        blink_off_ticks = pdMS_TO_TICKS(1800);
      } else if (t < 20.0) {
        blink_on_ticks = pdMS_TO_TICKS(2000);
        blink_off_ticks = pdMS_TO_TICKS(2000);
      } else if (t < 30.0) {
        blink_on_ticks = pdMS_TO_TICKS(1000);
        blink_off_ticks = pdMS_TO_TICKS(1000);
      } else if (t < 40.0) {
        blink_on_ticks = pdMS_TO_TICKS(200);
        blink_off_ticks = pdMS_TO_TICKS(200);
      } else {
        // critical high temperature: very fast blink (alert)
        blink_on_ticks = pdMS_TO_TICKS(100);
        blink_off_ticks = pdMS_TO_TICKS(100);
      }
    }

    // Determine if it's time to toggle the LED
    TickType_t now = xTaskGetTickCount();
    TickType_t elapsed = now - lastToggle;

    if (!ledState) {
      // currently OFF: check if off interval elapsed
      if (elapsed >= blink_off_ticks) {
        digitalWrite(LED_GPIO, HIGH);
        ledState = true;
        lastToggle = now;
      }
    } else {
      // currently ON: check if on interval elapsed
      if (elapsed >= blink_on_ticks) {
        digitalWrite(LED_GPIO, LOW);
        ledState = false;
        lastToggle = now;
      }
    }

    
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}