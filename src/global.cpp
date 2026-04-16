#include "global.h"
// Exported queues for direct use by tasks (no wrapper functions)
QueueHandle_t sensorQueue = NULL;   // Used by polling tasks (tinyml, coreiot)
QueueHandle_t ledQueue = NULL;      // Used by led_blinky task
QueueHandle_t neoQueue = NULL;      // Used by neo_blinky task
static SemaphoreHandle_t semInternet = NULL;

static String wifi_ssid_internal = "";
static String wifi_password_internal = "";
static String wifi_user_ssid = "ESP32-YOUR NETWORK HERE!!!";
static String wifi_user_password = "12345678";

static String core_token_internal = "";
static String core_server_internal = "";
static String core_port_internal = "";

static volatile bool wifi_connected_internal = false;

void system_state_init() {
	if (sensorQueue == NULL) {
		// queue holds last few samples
		sensorQueue = xQueueCreate(5, sizeof(SensorData_t));
	}
	if (semInternet == NULL) semInternet = xSemaphoreCreateBinary();

	// queues for notifying LED and Neo tasks with full SensorData_t copies
	if (ledQueue == NULL) ledQueue = xQueueCreate(5, sizeof(SensorData_t));
	if (neoQueue == NULL) neoQueue = xQueueCreate(5, sizeof(SensorData_t));
}

void give_internet_semaphore(){ if (semInternet) xSemaphoreGive(semInternet); }
BaseType_t take_internet_semaphore(TickType_t ticksToWait){ if (semInternet) return xSemaphoreTake(semInternet, ticksToWait); return pdFALSE; }


// WiFi/Core IoT accessors
void set_wifi_credentials(const String &ssid, const String &pass){ wifi_ssid_internal = ssid; wifi_password_internal = pass; }
void get_wifi_credentials(String &ssid, String &pass){ ssid = wifi_ssid_internal; pass = wifi_password_internal; }
void set_core_iot_info(const String &token, const String &server, const String &port){ core_token_internal = token; core_server_internal = server; core_port_internal = port; }
void get_core_iot_info(String &token, String &server, String &port){ token = core_token_internal; server = core_server_internal; port = core_port_internal; }

// WiFi state
void set_wifi_connected(bool connected){ wifi_connected_internal = connected; }
bool is_wifi_connected(){ return wifi_connected_internal; }