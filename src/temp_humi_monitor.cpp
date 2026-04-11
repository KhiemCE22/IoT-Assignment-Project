#include "temp_humi_monitor.h"
#include "global.h"
DHT20 dht20;
LiquidCrystal_I2C lcd(33,16,2);


void temp_humi_monitor(void *pvParameters){

    // Sensor / I2C initialization
    Wire.begin(11, 12);
    // Serial is usually started in main.setup(); duplicate start is safe.
    Serial.begin(115200);
    dht20.begin();

    // Initialize LCD (if present)
    lcd.begin();
    lcd.backlight();
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Temp/Humi Monitor");
    lcd.setCursor(0,1);
    lcd.print("Initializing...");

    const TickType_t SAMPLE_INTERVAL = pdMS_TO_TICKS(5000); // 5s
    const int READ_RETRIES = 3;

    while (1){
        float temperature = NAN;
        float humidity = NAN;

        // Attempt sensor reads with retries to reduce transient errors
        for (int attempt = 0; attempt < READ_RETRIES; ++attempt) {
            dht20.read();
            temperature = dht20.getTemperature();
            humidity = dht20.getHumidity();
            if (!isnan(temperature) && !isnan(humidity)) break;
            vTaskDelay(pdMS_TO_TICKS(200));
        }

        if (isnan(temperature) || isnan(humidity)) {
            Serial.println("DHT read failed after retries");
            temperature = -1;
            humidity = -1;
        }

        // Push sensor data to queue and update last-known value.
        SensorData_t d;
        d.temperature = temperature;
        d.humidity = humidity;
        push_sensor_data(d, 0);

        // Notify display and LED tasks that fresh data is available.
        give_led_semaphore();
        give_neo_semaphore();

        // Log to serial
        Serial.print("Humidity: ");
        Serial.print(humidity);
        Serial.print("%  Temperature: ");
        Serial.print(temperature);
        Serial.println(" C");

        // Update LCD (two-line)
        lcd.clear();
        lcd.setCursor(0,0);
        if (temperature < -0.5) {
            lcd.print("Temp: -- C");
        } else {
            lcd.print("Temp: ");
            lcd.print(temperature);
            lcd.print(" C");
        }
        lcd.setCursor(0,1);
        if (humidity < -0.5) {
            lcd.print("Humi: -- %");
        } else {
            lcd.print("Humi: ");
            lcd.print(humidity);
            lcd.print(" %");
        }

        vTaskDelay(SAMPLE_INTERVAL);
    }

}