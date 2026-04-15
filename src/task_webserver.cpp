#include "task_webserver.h"
#include <WiFi.h>
#include "global.h"
#include "task_check_info.h"

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

bool webserver_isrunning = false;
// Cache last status JSON to replay when clients reconnect
String lastStatusJson = "";

void Webserver_sendata(String data)
{
    // If this is a status message, cache it so newly connected clients can get it
    if (data.indexOf("\"type\":\"status\"") >= 0) {
        lastStatusJson = data;
    }

    if (ws.count() > 0)
    {
        ws.textAll(data); // Gửi đến tất cả client đang kết nối
        Serial.println("📤 Đã gửi dữ liệu qua WebSocket: " + data);
    }
    else
    {
        Serial.println("⚠️ Không có client WebSocket nào đang kết nối!");
    }
}

void Webserver_log(const String &msg)
{
    // Print locally
    Serial.println(msg);
    // Broadcast as JSON to clients (type: serial)
    if (ws.count() > 0)
    {
        String j = "{";
        j += "\"type\":\"serial\",";
        j += "\"message\":\"";
        // escape quotes in msg
        for (size_t i = 0; i < msg.length(); ++i) {
            char c = msg.charAt(i);
            if (c == '"') j += "\\\"";
            else if (c == '\n') j += "\\n";
            else j += c;
        }
        j += "\"}";
        ws.textAll(j);
    }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    if (type == WS_EVT_CONNECT)
    {
        String s = "WebSocket client #" + String(client->id()) + " connected from " + client->remoteIP().toString();
        Webserver_log(s);
        // Send current network status (STA and AP IPs) + saved config to the newly connected client
        String ssid, pass, token, server_addr, port;
        get_wifi_credentials(ssid, pass);
        // If in-memory credentials are empty, try loading persisted config as a fallback
        if (ssid.isEmpty() && pass.isEmpty()) {
            Load_info_File();
            get_wifi_credentials(ssid, pass);
            get_core_iot_info(token, server_addr, port);
        } else {
            get_core_iot_info(token, server_addr, port);
        }
        // If we have a cached status (from recent connect attempts), send it first
        if (lastStatusJson.length() > 0) {
            client->text(lastStatusJson);
        } else {
            String status = "{";
            status += "\"type\":\"status\",";
            status += "\"sta\":\"" + WiFi.localIP().toString() + "\",";
            status += "\"ap\":\"" + WiFi.softAPIP().toString() + "\",";
            status += "\"config\":{";
            status += "\"ssid\":\"" + ssid + "\",";
            status += "\"password\":\"" + pass + "\",";
            status += "\"token\":\"" + token + "\",";
            status += "\"server\":\"" + server_addr + "\",";
            status += "\"port\":\"" + port + "\"";
            status += "}}";
            client->text(status);
        }
    }
    else if (type == WS_EVT_DISCONNECT)
    {
        String s = "WebSocket client #" + String(client->id()) + " disconnected";
        Webserver_log(s);
    }
    else if (type == WS_EVT_DATA)
    {
        AwsFrameInfo *info = (AwsFrameInfo *)arg;

        if (info->opcode == WS_TEXT)
        {
            String message;
            message += String((char *)data).substring(0, len);
            // parseJson(message, true);
            handleWebSocketMessage(message);
        }
    }
}

void connnectWSV()
{
    ws.onEvent(onEvent);
    server.addHandler(&ws);
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(LittleFS, "/index.html", "text/html"); });
    server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(LittleFS, "/script.js", "application/javascript"); });
    server.on("/styles.css", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(LittleFS, "/styles.css", "text/css"); });
    server.begin();
    ElegantOTA.begin(&server);
    webserver_isrunning = true;
}

void Webserver_stop()
{
    ws.closeAll();
    server.end();
    webserver_isrunning = false;
}

void Webserver_reconnect()
{
    if (!webserver_isrunning)
    {
        connnectWSV();
    }
    ElegantOTA.loop();
}
