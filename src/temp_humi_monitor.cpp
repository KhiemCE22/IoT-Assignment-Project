#include "temp_humi_monitor.h"
#include "global.h"
#include "task_webserver.h"
DHT20 dht20;
LiquidCrystal_I2C lcd(0x21,16,2);


const int I2C_SDA_PIN = 11;
const int I2C_SCL_PIN = 12;

enum class EnvState {
    NORMAL,
    WARNING,
    CRITICAL
};

const float TEMP_SAFE_MIN = 20.0f;
const float TEMP_SAFE_MAX = 30.0f;
const float TEMP_CRITICAL_HIGH = 40.0f;

const float HUMI_SAFE_MIN = 40.0f;
const float HUMI_SAFE_MAX = 70.0f;
const float HUMI_CRITICAL_HIGH = 80.0f;




bool isTempNormal(float temperature) {
    return temperature >= TEMP_SAFE_MIN && temperature < TEMP_SAFE_MAX;
}
bool isTempWarning(float temperature) {
    return (temperature >= TEMP_SAFE_MAX && temperature < TEMP_CRITICAL_HIGH) || (temperature < TEMP_SAFE_MIN);
}
bool isTempCritical(float temperature) {
    return temperature >= TEMP_CRITICAL_HIGH;
}

bool isHumiNormal(float humidity) {
    return humidity >= HUMI_SAFE_MIN && humidity < HUMI_SAFE_MAX;
}
bool isHumiWarning(float humidity) {
    return (humidity >= HUMI_SAFE_MAX && humidity < HUMI_CRITICAL_HIGH) || (humidity < HUMI_SAFE_MIN);
}
bool isHumiCritical(float humidity) {
    return humidity >= HUMI_CRITICAL_HIGH;
}




EnvState evaluateState(float temperature, float humidity) {
    if (isTempCritical(temperature) || isHumiCritical(humidity)) {
        return EnvState::CRITICAL;
    } else if (isTempWarning(temperature) || isHumiWarning(humidity)) {
        return EnvState::WARNING;
    } else {
        return EnvState::NORMAL;
    }
}

void showNormal(float temperature, float humidity) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("T:");
    lcd.print(temperature, 1);
    lcd.print("C H:");
    lcd.print(humidity, 0);
    lcd.print("%");

    lcd.setCursor(0, 1);
    lcd.print("SYSTEM: NORMAL");
}

void showWarning(float temperature, float humidity) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("T:");
    lcd.print(temperature, 1);
    lcd.print("C H:");
    lcd.print(humidity, 0);
    lcd.print("%");

    lcd.setCursor(0, 1);
    lcd.print("STATE: WARNING");
}

void showCritical() {
    const bool blinkOn = (millis() / 500) % 2 == 0; // Blink every 500ms
    lcd.clear();
    lcd.setCursor(0, 0);
    if (blinkOn) {
        lcd.print("!!! CRITICAL !!");
    }
}


void updateLcdFsm(EnvState state, float temperature, float humidity) {
    switch (state) {
        case EnvState::NORMAL:
            showNormal(temperature, humidity);
            
            break;
        case EnvState::WARNING:
            showWarning(temperature, humidity);
            break;
        case EnvState::CRITICAL:
            showCritical();
            break;
    }
}


void sendLatestSensorData(QueueHandle_t queue, const SensorData_t &data) {
    if (!queue) {
        return;
    }

    if (xQueueSend(queue, &data, 0) == pdTRUE) {
        return;
    }

    SensorData_t droppedData;
    xQueueReceive(queue, &droppedData, 0);
    xQueueSend(queue, &data, 0);
}



void temp_humi_monitor(void *pvParameters){
    SensorData_t data;

    // Sensor / I2C initialization
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    // Serial is usually started in main.setup(); duplicate start is safe.
    Serial.begin(115200);
    dht20.begin();

    // Initialize LCD (if present)
    lcd.begin();
    lcd.backlight();
    // setupLcdIcons();
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Temp/Humi Ready");
    lcd.setCursor(0,1);
    lcd.print("Initializing...");

    const TickType_t SAMPLE_INTERVAL = pdMS_TO_TICKS(5000); // 5s
    const TickType_t LCD_BLINK_INTERVAL = pdMS_TO_TICKS(500); // 500ms
    const TickType_t LOOP_INTERVAL = pdMS_TO_TICKS(50);
    const int READ_RETRIES = 3;
    TickType_t lastSampleTick = xTaskGetTickCount() - SAMPLE_INTERVAL;
    TickType_t lastLcdBlinkTick = 0;
    EnvState envState = EnvState::NORMAL;
    bool hasSensorData = false;

    while (1){
        TickType_t now = xTaskGetTickCount();

        if ((now - lastSampleTick) >= SAMPLE_INTERVAL) {
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
            Serial.printf("Read sensor: Temp=%.1f C, Humi=%.0f%%\n", temperature, humidity);
            // Push sensor data directly to all consumer queues
            data.temperature = temperature;
            data.humidity = humidity;
            hasSensorData = true;


            sendLatestSensorData(ledQueue, data);
            sendLatestSensorData(neoQueue, data);
            sendLatestSensorData(tinyQueue, data);
            sendLatestSensorData(gatewayQueue, data);
            // Log to serial and web
            {
                String line = "Humidity: " + String(humidity) + "%  Temperature: " + String(temperature) + " C";
                Webserver_log(line);
            }

            // Send JSON data to connected WebSocket clients (if any)
            {
                String payload = "{";
                payload += "\"temperature\":" + String(temperature) +  ",";
                payload += "\"humidity\":" + String(humidity);
                payload += "}";
                Webserver_sendata(payload);
            }

            envState = evaluateState(temperature, humidity);
            updateLcdFsm(envState, temperature, humidity);
            lastSampleTick = xTaskGetTickCount();
            lastLcdBlinkTick = lastSampleTick;
            now = lastSampleTick;
        }

        if (hasSensorData && envState == EnvState::CRITICAL && (now - lastLcdBlinkTick) >= LCD_BLINK_INTERVAL) {
            showCritical();
            lastLcdBlinkTick = now;
        }

        vTaskDelay(LOOP_INTERVAL);
    }

}
