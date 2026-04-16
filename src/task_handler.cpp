#include <task_handler.h>
#include "task_webserver.h"
#include <WiFi.h>

void handleWebSocketMessage(String message)
{
    Webserver_log(message);
    StaticJsonDocument<256> doc;

    DeserializationError error = deserializeJson(doc, message);
    if (error)
    {
        Serial.println("Lỗi parse JSON!");
        return;
    }
    JsonObject value = doc["value"];
    if (doc["page"] == "device")
    {
        if (!value.containsKey("gpio") || !value.containsKey("status"))
        {
            Serial.println("JSON thiếu thông tin gpio hoặc status");
            return;
        }

        int gpio = value["gpio"];
        String status = value["status"].as<String>();

        Serial.printf("Điều khiển GPIO %d → %s\n", gpio, status.c_str());
        pinMode(gpio, OUTPUT);
        if (status.equalsIgnoreCase("ON"))
        {
            digitalWrite(gpio, HIGH);
            Serial.printf("GPIO %d ON\n", gpio);
        }
        else if (status.equalsIgnoreCase("OFF"))
        {
            digitalWrite(gpio, LOW);
            Serial.printf("GPIO %d OFF\n", gpio);
        }
    }
    else if (doc["page"] == "setting")
    {
        String WIFI_SSID = doc["value"]["ssid"].as<String>();
        String WIFI_PASS = doc["value"]["password"].as<String>();
        String CORE_IOT_TOKEN = doc["value"]["token"].as<String>();
        String CORE_IOT_SERVER = doc["value"]["server"].as<String>();
        String CORE_IOT_PORT = doc["value"]["port"].as<String>();

        Webserver_log("Nhận cấu hình từ WebSocket:");
        Webserver_log("SSID: " + WIFI_SSID);
        Webserver_log("PASS: " + WIFI_PASS);
        Webserver_log("TOKEN: " + CORE_IOT_TOKEN);
        Webserver_log("SERVER: " + CORE_IOT_SERVER);
        Webserver_log("PORT: " + CORE_IOT_PORT);

        // Gọi hàm lưu cấu hình
        Save_info_File(WIFI_SSID, WIFI_PASS, CORE_IOT_TOKEN, CORE_IOT_SERVER, CORE_IOT_PORT);

        // Phản hồi lại client (tùy chọn)
        String msg = "{\"status\":\"ok\",\"page\":\"setting_saved\"}";
        ws.textAll(msg);
    }
    else if (doc["page"] == "get_config")
    {
        // Return current network status + saved config
        String ssid, pass, token, server, port;
        get_wifi_credentials(ssid, pass);
        get_core_iot_info(token, server, port);

        String status = "{";
        status += "\"type\":\"status\",";
        status += "\"sta\":\"" + WiFi.localIP().toString() + "\",";
        status += "\"ap\":\"" + WiFi.softAPIP().toString() + "\",";
        status += "\"config\":{";
        status += "\"ssid\":\"" + ssid + "\",";
        status += "\"password\":\"" + pass + "\",";
        status += "\"token\":\"" + token + "\",";
        status += "\"server\":\"" + server + "\",";
        status += "\"port\":\"" + port + "\"";
        status += "}}";

        Webserver_sendata(status);
    }
}
