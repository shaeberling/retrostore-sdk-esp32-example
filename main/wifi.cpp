#include "wifi.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "esp_event.h"
#include "esp_log.h"

#include "lwip/err.h"
#include "lwip/sys.h"

namespace {

ESP_EVENT_DEFINE_BASE(WINSTON_EVENT);

#define MAXIMUM_RETRY  10

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about one event
 * - are we connected to the AP with an IP? */
static const int WIFI_CONNECTED_BIT = BIT0;
static const char *TAG = "winston-wifi";
static int s_retry_num = 0;

static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data) {
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    ESP_LOGI(TAG, "WIFI: Connection Start");
    esp_wifi_connect();
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    ESP_LOGI(TAG, "WIFI: Disconnected");
    if (s_retry_num < MAXIMUM_RETRY) {
      esp_wifi_connect();
      xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
      s_retry_num++;
      auto delay_millis = 5 * 1000;
      vTaskDelay(delay_millis / portTICK_PERIOD_MS);
      ESP_LOGI(TAG, "Retrying to connect to the AP, %d of %d tries",
                    s_retry_num, MAXIMUM_RETRY);
    } else {
      ESP_LOGI(TAG, "Connection to the AP failed. Rebooting...");
      esp_restart();
    }
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
    ESP_LOGI(TAG, "got ip: " IPSTR, IP2STR(&event->ip_info.ip));
    s_retry_num = 0;
    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    // Fire off an event to let other event listener know that WIFI is
    // now connected.
    esp_event_post(WINSTON_EVENT, WIFI_CONNECTED, NULL, 0, portMAX_DELAY);
  } else {
    ESP_LOGI(TAG, "WIFI: Unknown event.");
  }
}
}  // namespace

// public
void Wifi::connect(const std::string& ssid, const std::string& password) {
  s_wifi_event_group = xEventGroupCreate();

  ESP_ERROR_CHECK(esp_netif_init());
  esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                             &event_handler, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                             &event_handler, NULL));

  wifi_config_t wifi_config = { };
  strcpy((char*) wifi_config.sta.ssid, ssid.c_str());
  strcpy((char*) wifi_config.sta.password, password.c_str());

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
  ESP_ERROR_CHECK(esp_wifi_start() );

  ESP_LOGI(TAG, "wifi_init_sta finished.");
  ESP_LOGI(TAG, "Connecting to Wifi SSID: %s", ssid.c_str());
}
