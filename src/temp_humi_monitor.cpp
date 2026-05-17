#include "temp_humi_monitor.h"
#include "TinyML_task.h"

#include "global.h"
#include "task_webserver.h"
DHT20 dht20;
LiquidCrystal_I2C lcd(33,16,2);

namespace {
enum class EnvState {
    NORMAL,
    WARNING,
    CRITICAL
};

const float TEMP_SAFE_MIN = 18.0f;
const float TEMP_SAFE_MAX = 32.0f;
const float TEMP_DANGER_MIN = 10.0f;
const float TEMP_DANGER_MAX = 40.0f;
const float HUMI_SAFE_MIN = 30.0f;
const float HUMI_SAFE_MAX = 70.0f;
const float HUMI_DANGER_MIN = 20.0f;
const float HUMI_DANGER_MAX = 85.0f;
const unsigned long AI_RESULT_TIMEOUT_MS = 15000;

const uint8_t LCD_ICON_TEMP = 0;
const uint8_t LCD_ICON_DROP = 1;
const uint8_t LCD_ICON_SHIELD = 2;
const uint8_t LCD_ICON_WARN = 3;

bool isSensorValid(float temperature, float humidity) {
    return !isnan(temperature) && !isnan(humidity) && temperature >= 0.0f && humidity >= 0.0f && humidity <= 100.0f;
}

bool isTempSafe(float temperature) {
    return temperature >= TEMP_SAFE_MIN && temperature <= TEMP_SAFE_MAX;
}

bool isHumiSafe(float humidity) {
    return humidity >= HUMI_SAFE_MIN && humidity <= HUMI_SAFE_MAX;
}

bool isTempDangerous(float temperature) {
    return temperature < TEMP_DANGER_MIN || temperature > TEMP_DANGER_MAX;
}

bool isHumiDangerous(float humidity) {
    return humidity < HUMI_DANGER_MIN || humidity > HUMI_DANGER_MAX;
}

bool hasFreshAiResult(bool hasAiResult, unsigned long lastAiResultAtMs) {
    return hasAiResult && millis() - lastAiResultAtMs <= AI_RESULT_TIMEOUT_MS;
}

EnvState evaluateState(float temperature, float humidity, const AIAnomalyResult &ai, bool aiFresh) {
    if (!isSensorValid(temperature, humidity)) {
        return EnvState::WARNING;
    }

    const bool anomaly = aiFresh && ai.isAnomaly;
    const bool aiNormal = aiFresh && !ai.isAnomaly;
    const bool tempSafe = isTempSafe(temperature);
    const bool humiSafe = isHumiSafe(humidity);
    const bool tempDanger = isTempDangerous(temperature);
    const bool humiDanger = isHumiDangerous(humidity);

    if (anomaly && tempDanger && humiDanger) {
        return EnvState::CRITICAL;
    }

    if ((aiNormal || !aiFresh) && tempSafe && humiSafe) {
        return EnvState::NORMAL;
    }

    return EnvState::WARNING;
}

const char *stateName(EnvState state) {
    switch (state) {
        case EnvState::NORMAL:
            return "NORMAL";
        case EnvState::WARNING:
            return "WARNING";
        case EnvState::CRITICAL:
            return "CRITICAL";
    }
    return "UNKNOWN";
}

void printPadded(const char *text) {
    lcd.print(text);
    const size_t len = strlen(text);
    for (size_t i = len; i < 16; ++i) {
        lcd.print(' ');
    }
}

void setupLcdIcons() {
    uint8_t tempIcon[8] = {
        B00100,
        B01010,
        B01010,
        B01110,
        B01110,
        B11111,
        B11111,
        B01110
    };
    uint8_t dropIcon[8] = {
        B00100,
        B00100,
        B01010,
        B01010,
        B10001,
        B10001,
        B10001,
        B01110
    };
    uint8_t shieldIcon[8] = {
        B01110,
        B11111,
        B10101,
        B10101,
        B10001,
        B01010,
        B00100,
        B00100
    };
    uint8_t warnIcon[8] = {
        B00100,
        B01110,
        B01110,
        B11111,
        B10101,
        B11111,
        B00100,
        B00100
    };

    lcd.createChar(LCD_ICON_TEMP, tempIcon);
    lcd.createChar(LCD_ICON_DROP, dropIcon);
    lcd.createChar(LCD_ICON_SHIELD, shieldIcon);
    lcd.createChar(LCD_ICON_WARN, warnIcon);
}

void showNormal(float temperature, float humidity) {
    lcd.setCursor(0, 0);
    lcd.write(static_cast<uint8_t>(LCD_ICON_TEMP));
    lcd.print(temperature, 1);
    lcd.write(static_cast<uint8_t>(223));
    lcd.print("C ");
    lcd.write(static_cast<uint8_t>(LCD_ICON_DROP));
    lcd.print(humidity, 0);
    lcd.print("%   ");

    lcd.setCursor(0, 1);
    lcd.print("System:SAFE ");
    lcd.write(static_cast<uint8_t>(LCD_ICON_SHIELD));
    lcd.print("   ");
}

void showWarning(float temperature, float humidity, const AIAnomalyResult &ai, bool aiFresh) {
    const bool blinkOn = (millis() / 500) % 2 == 0; // Blink every 500ms

    lcd.setCursor(0, 0);
    if (blinkOn) {
        lcd.write(static_cast<uint8_t>(LCD_ICON_WARN));
        lcd.print("!!Warning!!    ");
    } else {
        printPadded("                ");
    }

    lcd.setCursor(0, 1);
    if (!isSensorValid(temperature, humidity)) {
        printPadded("Sensor read err");
    } else if (aiFresh && ai.isAnomaly) {
        printPadded("AI:Anomaly     ");
    } else {
        printPadded("RAW:Out of safe");
    }
}

void showCritical(float temperature, float humidity) {
    lcd.setCursor(0, 0);
    printPadded("CRITICAL ERROR!");

    lcd.setCursor(0, 1);
    char line[17];
    snprintf(line, sizeof(line), "T:%4.1f H:%2.0f RISK", temperature, humidity);
    printPadded(line);
}

void updateLcdFsm(EnvState state, float temperature, float humidity, const AIAnomalyResult &ai, bool aiFresh) {
    static EnvState lastState = EnvState::NORMAL;
    static bool firstRender = true;

    if (firstRender || state != lastState) {
        lcd.clear();
        firstRender = false;
        lastState = state;
    }

    switch (state) {
        case EnvState::NORMAL:
            showNormal(temperature, humidity);
            break;
        case EnvState::WARNING:
            showWarning(temperature, humidity, ai, aiFresh);
            break;
        case EnvState::CRITICAL:
            showCritical(temperature, humidity);
            break;
    }
}

void receiveLatestAiResult(AIAnomalyResult &latestAiResult, bool &hasAiResult, unsigned long &lastAiResultAtMs) {
    if (!aiResultQueue) {
        return;
    }

    AIAnomalyResult receivedResult;
    while (xQueueReceive(aiResultQueue, &receivedResult, 0) == pdTRUE) {
        latestAiResult = receivedResult;
        hasAiResult = true;
        lastAiResultAtMs = millis();
    }
}

void waitAndRefreshLcd(
    float temperature,
    float humidity,
    TickType_t durationTicks,
    AIAnomalyResult &latestAiResult,
    bool &hasAiResult,
    unsigned long &lastAiResultAtMs
) {
    const TickType_t refreshInterval = pdMS_TO_TICKS(500);
    TickType_t elapsed = 0;

    while (elapsed < durationTicks) {
        const TickType_t remaining = durationTicks - elapsed;
        const TickType_t delayTicks = remaining < refreshInterval ? remaining : refreshInterval;
        vTaskDelay(delayTicks);
        elapsed += delayTicks;

        receiveLatestAiResult(latestAiResult, hasAiResult, lastAiResultAtMs);
        const bool aiFresh = hasFreshAiResult(hasAiResult, lastAiResultAtMs);
        const EnvState envState = evaluateState(temperature, humidity, latestAiResult, aiFresh);
        updateLcdFsm(envState, temperature, humidity, latestAiResult, aiFresh);
    }
}
}


void temp_humi_monitor(void *pvParameters){
    RawSensorData data;
    AIAnomalyResult latestAiResult = {0.0f, false};
    bool hasAiResult = false;
    unsigned long lastAiResultAtMs = 0;

    // Sensor / I2C initialization
    Wire.begin(11, 12);
    // Serial is usually started in main.setup(); duplicate start is safe.
    Serial.begin(115200);
    dht20.begin();

    // Initialize LCD (if present)
    lcd.begin();
    lcd.backlight();
    setupLcdIcons();
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

        // Push sensor data directly to all consumer queues
        data.temperature = temperature;
        data.humidity = humidity;
        receiveLatestAiResult(latestAiResult, hasAiResult, lastAiResultAtMs);
        const bool aiFresh = hasFreshAiResult(hasAiResult, lastAiResultAtMs);
        const EnvState envState = evaluateState(temperature, humidity, latestAiResult, aiFresh);
        xQueueSend(ledQueue, &data, portMAX_DELAY);
        xQueueSend(neoQueue, &data, portMAX_DELAY);
        xQueueSend(tinyQueue, &data, portMAX_DELAY);
        xQueueSend(gatewayQueue, &data, portMAX_DELAY);
        // Log to serial and web
        {
            String line = "Humidity: " + String(humidity) + "%  Temperature: " + String(temperature) + " C";
            Webserver_log(line);
        }

        // Send JSON data to connected WebSocket clients (if any)
        {
            String payload = "{";
            payload += "\"temperature\":" + String(temperature) +  ",";
            payload += "\"humidity\":" + String(humidity) + ",";
            payload += "\"state\":\"" + String(stateName(envState)) + "\",";
            payload += "\"ai_valid\":" + String(aiFresh ? "true" : "false") + ",";
            payload += "\"ai_anomaly\":" + String((aiFresh && latestAiResult.isAnomaly) ? "true" : "false") + ",";
            payload += "\"ai_score\":" + String(latestAiResult.score, 4);
            payload += "}";
            Webserver_sendata(payload);
        }

        updateLcdFsm(envState, temperature, humidity, latestAiResult, aiFresh);

        waitAndRefreshLcd(
            temperature,
            humidity,
            SAMPLE_INTERVAL,
            latestAiResult,
            hasAiResult,
            lastAiResultAtMs
        );
    }

}
