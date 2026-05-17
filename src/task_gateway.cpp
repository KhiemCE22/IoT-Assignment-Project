#include "task_gateway.h"

// ----------- CONFIGURE THESE! -----------
const char* gateway_Server = "192.168.1.200";  
const char* gateway_Token = "";   // Device Access Token
const int   mqttPort = 1883;
// ----------------------------------------

WiFiClient espClient_pi;
PubSubClient client_pi(espClient_pi);

void reconnect_gateway() {
  // Loop until we're reconnected
  while (!client_pi.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect (username=token, password=empty)
    //if (client_pi.connect("ESP32Client", gateway_Token, NULL)) {
    // String clientId = "ESP32Client-";
    // clientId += String(random(0xffff), HEX);
    String clientId = "ESP32_Node_01";
    if (client_pi.connect(clientId.c_str())) {
        
      Serial.println("connected to Gateway Server!");
    //   client_pi.subscribe("v1/devices/me/rpc/request/+");
    //   Serial.println("Subscribed to v1/devices/me/rpc/request/+");

    } else {
      Serial.print("failed, rc=");
      Serial.print(client_pi.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void setup_gateway(){

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

  client_pi.setServer(gateway_Server, mqttPort);

}

void gateway_task(void *pvParameters){
    setup_gateway();

    while(1){
        if (!client_pi.connected()) {
            reconnect_gateway();
        }
        client_pi.loop();

        RawSensorData sd = {NAN, NAN};
        // Get latest sensor data from queue (non-blocking)
        if (xQueueReceive(gatewayQueue, &sd, 0) == pdTRUE) {
            String payload = "{\"temperature\":" + String(sd.temperature) + ",\"humidity\":" + String(sd.humidity) + "}";
            
            // Topic: nodes/ESP32_001
            client_pi.publish("nodes/ESP32_001", payload.c_str());
            Serial.println("Sent to Local Broker: " + payload);
        }
        
        vTaskDelay(pdMS_TO_TICKS(10000)); // Publish every 10 seconds
    }
}
