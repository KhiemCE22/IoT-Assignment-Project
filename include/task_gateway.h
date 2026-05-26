#ifndef __GATEWAY_H__
#define __GATEWAY_H__

#include <Arduino.h>
#include <WiFi.h>
#include "global.h"
#include <PubSubClient.h>
#include <ArduinoJson.h>


void gateway_task(void *pvParameters);

#endif