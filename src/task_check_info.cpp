#include "task_check_info.h"
#include "global.h"

void Load_info_File()
{
  // Ensure LittleFS is mounted before attempting to read
  if (!LittleFS.begin(true))
  {
    Serial.println("LittleFS begin failed in Load_info_File");
    return;
  }

  File file = LittleFS.open("/info.dat", "r");
  if (!file)
  {
    return;
  }
  DynamicJsonDocument doc(4096);
  DeserializationError error = deserializeJson(doc, file);
  if (error)
  {
    Serial.print(F("deserializeJson() failed: "));
  }
  else
  {
    String ssid = String((const char*)doc["WIFI_SSID"]);
    String pass = String((const char*)doc["WIFI_PASS"]);
    String token = String((const char*)doc["CORE_IOT_TOKEN"]);
    String server = String((const char*)doc["CORE_IOT_SERVER"]);
    String port = String((const char*)doc["CORE_IOT_PORT"]);
    set_wifi_credentials(ssid, pass);
    set_core_iot_info(token, server, port);
  }
  file.close();
}

void Delete_info_File()
{
  if (LittleFS.exists("/info.dat"))
  {
    LittleFS.remove("/info.dat");
  }
  ESP.restart();
}

void Save_info_File(String wifi_ssid, String wifi_pass, String CORE_IOT_TOKEN, String CORE_IOT_SERVER, String CORE_IOT_PORT)
{
  Serial.println(wifi_ssid);
  Serial.println(wifi_pass);

  DynamicJsonDocument doc(4096);
  doc["WIFI_SSID"] = wifi_ssid;
  doc["WIFI_PASS"] = wifi_pass;
  doc["CORE_IOT_TOKEN"] = CORE_IOT_TOKEN;
  doc["CORE_IOT_SERVER"] = CORE_IOT_SERVER;
  doc["CORE_IOT_PORT"] = CORE_IOT_PORT;

  // Ensure LittleFS is mounted before attempting to write
  if (!LittleFS.begin(true))
  {
    Serial.println("LittleFS begin failed in Save_info_File");
  }

  File configFile = LittleFS.open("/info.dat", "w");
  if (configFile)
  {
    serializeJson(doc, configFile);
    configFile.close();
  }
  else
  {
    Serial.println("Unable to save the configuration.");
  }
  // Apply credentials in-memory so background STA attempt can read them
  set_wifi_credentials(wifi_ssid, wifi_pass);
  set_core_iot_info(CORE_IOT_TOKEN, CORE_IOT_SERVER, CORE_IOT_PORT);

  // Notify clients we're starting connection attempt and include saved config
  String status = "{";
  status += "\"type\":\"status\",";
  status += "\"state\":\"connecting\",";
  status += "\"sta\":\"0.0.0.0\",";
  status += "\"ap\":\"" + WiFi.softAPIP().toString() + "\",";
  status += "\"config\":{";
  status += "\"ssid\":\"" + wifi_ssid + "\",";
  status += "\"password\":\"" + wifi_pass + "\",";
  status += "\"token\":\"" + CORE_IOT_TOKEN + "\",";
  status += "\"server\":\"" + CORE_IOT_SERVER + "\",";
  status += "\"port\":\"" + CORE_IOT_PORT + "\"";
  status += "}}";
  Webserver_sendata(status);

  // Start STA connection asynchronously while keeping AP/webserver alive
  startSTAAsync();
};

bool check_info_File(bool check)
{
  if (!check)
  {
    if (!LittleFS.begin(true))
    {
      Serial.println("Lỗi khởi động LittleFS!");
      return false;
    }
    Load_info_File();
  }
  
  String ssid, pass;
  get_wifi_credentials(ssid, pass);
  if (ssid.isEmpty() && pass.isEmpty())
  {
    if (!check)
    {
      startAP();
    }
    return false;
  }
  return true;
}