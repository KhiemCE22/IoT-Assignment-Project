#include "neo_blinky.h"
#include "global.h"
/*
  neo_blinky task

  Humidity -> Color mapping (example):
  - humidity < 30%   : Blue   (0, 0, 150)   -- dry
  - 30% <= h < 60%   : Green  (0, 150, 0)   -- normal
  - 60% <= h < 80%   : Yellow (150, 150, 0) -- humid
  - h >= 80%         : Red    (200, 0, 0)   -- very humid / alert
  - invalid (<0)     : Purple (80, 0, 80)   -- sensor error

  Synchronization:
  - `temp_humi_monitor` gives `xSemaphoreNeo` after each humidity update.
  - `neo_blinky` checks the semaphore (non-blocking) and when signaled
    reads `glob_humidity` and updates the NeoPixel color accordingly.
  - This decouples sampling frequency from display updates and ensures
    color updates happen only when new data is available.
*/

void neo_blinky(void *pvParameters){

    Adafruit_NeoPixel strip(LED_COUNT, NEO_PIN, NEO_GRB + NEO_KHZ800);
    strip.begin();
    strip.clear();
    strip.show();

    uint32_t currentColor = 0;

    while(1) {
        // If a new humidity sample is available, update the color mapping
        if (xSemaphoreTake(xSemaphoreNeo, 0) == pdTRUE) {
            float h = glob_humidity;

            if (h < 0) {
                currentColor = strip.Color(80, 0, 80); // purple = error
            } else if (h < 30.0) {
                currentColor = strip.Color(0, 0, 150); // blue = dry
            } else if (h < 60.0) {
                currentColor = strip.Color(0, 150, 0); // green = normal
            } else if (h < 80.0) {
                currentColor = strip.Color(150, 150, 0); // yellow = humid
            } else {
                currentColor = strip.Color(200, 0, 0); // red = very humid
            }

            // Update the NeoPixel immediately when color changes
            strip.setPixelColor(0, currentColor);
            strip.show();
        }

        // Keep task responsive; sleep briefly before checking semaphore again.
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}