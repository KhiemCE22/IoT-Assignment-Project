#include "coreiot.h"
#include "global.h"
// ----------- CONFIGURE THESE! -----------
const char* coreIOT_Server = "";  
const char* coreIOT_Token = "";   // Device Access Token
const int   mqttPort = 1883;
// ----------------------------------------

WiFiClient espClient;
PubSubClient client(espClient);


void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect (username=token, password=empty)
    //if (client.connect("ESP32Client", coreIOT_Token, NULL)) {
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);

    if (client.connect(clientId.c_str())) {
        
      Serial.println("connected to CoreIOT Server!");
      client.subscribe("v1/devices/me/rpc/request/+");
      Serial.println("Subscribed to v1/devices/me/rpc/request/+");

    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}


void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.println("] ");

  // Allocate a temporary buffer for the message
  char message[length + 1];
  memcpy(message, payload, length);
  message[length] = '\0';
  Serial.print("Payload: ");
  Serial.println(message);

  // Parse JSON
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, message);

  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    return;
  }

  const char* method = doc["method"];
  if (strcmp(method, "setStateLED") == 0) {
    // Check params type (could be boolean, int, or string according to your RPC)
    // Example: {"method": "setValueLED", "params": "ON"}
    const char* params = doc["params"];

    if (strcmp(params, "ON") == 0) {
      Serial.println("Device turned ON.");
      //TODO

    } else {   
      Serial.println("Device turned OFF.");
      //TODO

    }
  } else {
    Serial.print("Unknown method: ");
    Serial.println(method);
  }
}


void setup_coreiot(){

//   Serial.print("Connecting to WiFi...");
//   WiFi.begin(wifi_ssid, wifi_password);
//   while (WiFi.status() != WL_CONNECTED) {
  
//   while (isWifiConnected == false) {
//     delay(500);
//     Serial.print(".");
//   }
// }

  // Wait until internet is available
  while(1){
    if (take_internet_semaphore(portMAX_DELAY) == pdTRUE) {
      break;
    }
    delay(500);
    Serial.print(".");
  }


  Serial.println(" Connected!");

  String token, server, port;
  get_core_iot_info(token, server, port);
  if (!server.isEmpty()) client.setServer(server.c_str(), port.toInt());
  client.setCallback(callback);

}

void coreiot_task(void *pvParameters){

    setup_coreiot();

    while(1){

        if (!client.connected()) {
            reconnect();
        }
        client.loop();

        // Sample payload, publish to 'v1/devices/me/telemetry'
        SensorData_t sd;
        if (!get_last_sensor_data(sd)) {
          sd.temperature = NAN; sd.humidity = NAN;
        }
        String payload = "{\"temperature\":" + String(sd.temperature) +  ",\"humidity\":" + String(sd.humidity) + "}";
        client.publish("v1/devices/me/telemetry", payload.c_str());


        
        Serial.println("Published payload: " + payload);
        vTaskDelay(10000);  // Publish every 10 seconds
    }
}