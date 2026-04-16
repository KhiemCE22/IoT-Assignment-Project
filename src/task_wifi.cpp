#include "task_wifi.h"
#include "global.h"
#include "task_webserver.h"

void startAP()
{
    WiFi.mode(WIFI_AP);
    WiFi.softAP(String(SSID_AP), String(PASS_AP));
    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());
}

void startSTA()
{
    String ssid, pass;
    get_wifi_credentials(ssid, pass);
    if (ssid.isEmpty()) {
        vTaskDelete(NULL);
    }
    WiFi.mode(WIFI_STA);
    if (pass.isEmpty()) {
        WiFi.begin(ssid.c_str());
    } else {
        WiFi.begin(ssid.c_str(), pass.c_str());
    }

    while (WiFi.status() != WL_CONNECTED)
    {
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    // Notify internet-ready via API
    // Log and broadcast STA IP for easy discovery
    {
        String ipmsg = "STA IP address: " + WiFi.localIP().toString();
        Webserver_log(ipmsg);
    }
    give_internet_semaphore();
}

// Non-blocking STA connect that keeps AP available (AP+STA)
void sta_connect_task(void *pvParameters)
{
    (void)pvParameters;
    String ssid, pass;
    get_wifi_credentials(ssid, pass);
    if (ssid.isEmpty())
    {
        vTaskDelete(NULL);
        return;
    }

    Webserver_log("Bắt đầu kết nối STA (nền) đến: " + ssid);

    // Set AP+STA mode so softAP stays available while trying STA
    WiFi.mode(WIFI_AP_STA);
    if (pass.isEmpty())
    {
        WiFi.begin(ssid.c_str());
    }
    else
    {
        WiFi.begin(ssid.c_str(), pass.c_str());
    }

    const int maxSecs = 30; // total timeout
    int elapsed = 0;
    int tryCount = 0;
    while (elapsed < maxSecs)
    {
        if (WiFi.status() == WL_CONNECTED)
        {
            String status = "{";
            status += "\"type\":\"status\",";
            status += "\"state\":\"connected\",";
            status += "\"sta\":\"" + WiFi.localIP().toString() + "\",";
            status += "\"ap\":\"" + WiFi.softAPIP().toString() + "\"";
            status += "}";
            Webserver_sendata(status);
            Webserver_log("STA connected: " + WiFi.localIP().toString());
            give_internet_semaphore();
            vTaskDelete(NULL);
            return;
        }

        // periodically broadcast connecting status
        if (elapsed % 3 == 0)
        {
            tryCount++;
            String status = "{";
            status += "\"type\":\"status\",";
            status += "\"state\":\"connecting\",";
            status += "\"try\":" + String(tryCount) + ",";
            status += "\"sta\":\"0.0.0.0\",";
            status += "\"ap\":\"" + WiFi.softAPIP().toString() + "\"";
            status += "}";
            Webserver_sendata(status);
        }

        vTaskDelay(1000 / portTICK_PERIOD_MS);
        elapsed++;
    }

    // failed
    String fail = "{";
    fail += "\"type\":\"status\",";
    fail += "\"state\":\"failed\",";
    fail += "\"reason\":\"timeout\",";
    fail += "\"ap\":\"" + WiFi.softAPIP().toString() + "\"";
    fail += "}";
    Webserver_sendata(fail);
    Webserver_log("STA connection failed (timeout)");
    vTaskDelete(NULL);
}

void startSTAAsync()
{
    xTaskCreate(sta_connect_task, "sta_connect", 4096, NULL, 1, NULL);
}

bool Wifi_reconnect()
{
    const wl_status_t status = WiFi.status();
    if (status == WL_CONNECTED)
    {
        return true;
    }
    startSTA();
    return false;
}
