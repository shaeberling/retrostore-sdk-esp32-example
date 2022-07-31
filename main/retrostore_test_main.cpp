/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_spi_flash.h"

#include "retrostore.h"
#include "wifi.h"

static const char *TAG = "retrostore-tester";
ESP_EVENT_DEFINE_BASE(WINSTON_EVENT);

using namespace std;
using namespace retrostore;


RetroStore rs;


void testUploadDownloadSystemImage() {
  ESP_LOGI(TAG, "testUploadDownloadSystemImage()...");
  // TODO: Upload a system image for testing ...
  // TODO. Replace this hardcoded token with the one we got from the upload.
  int token = 646;

  RsSystemState state;
  rs.downloadState(token, &state);
}

void initWifi() {
  ESP_LOGI(TAG, "Connecting to Wifi...");
  auto* wifi = new Wifi();
  wifi->connect(CONFIG_RS_TEST_WIFI_SSID, CONFIG_RS_TEST_WIFI_PASSWORD);
}

void runAllTests() {
  auto initialFreeHeapKb = esp_get_free_heap_size() / 1024;
  ESP_LOGI(TAG, "RetroStore API tests running... Initial free heap: %d", initialFreeHeapKb);

  for (int i = 0; i < 10; ++i) {
    testUploadDownloadSystemImage();
    auto newFreeHeapKb = esp_get_free_heap_size() / 1024;
    auto diffHeapKb =  initialFreeHeapKb - newFreeHeapKb;
    ESP_LOGI(TAG, "After run [%d], free heap is %d, diff: %d", i, newFreeHeapKb, diffHeapKb);
  }

  ESP_LOGI(TAG, "DONE. All tests run.");
}

void event_handler(void* arg, esp_event_base_t event_base,
                   int32_t event_id, void* event_data) {
  if (event_base == WINSTON_EVENT && event_id == WIFI_CONNECTED) {
    runAllTests();
  } else {
    ESP_LOGW(TAG, "Received unknown event.");
  }
}

// Initialize NVS
void initNvs() {
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);
}


extern "C" {

void app_main(void)
{
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  ESP_ERROR_CHECK(esp_event_handler_register(WINSTON_EVENT, WIFI_CONNECTED,
                                             &event_handler, NULL));

  initNvs();
  initWifi();

  /* Print chip information */
  esp_chip_info_t chip_info;
  esp_chip_info(&chip_info);
  printf("This is %s chip with %d CPU core(s), WiFi%s%s, ",
          CONFIG_IDF_TARGET,
          chip_info.cores,
          (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
          (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

  printf("silicon revision %d, ", chip_info.revision);

  printf("%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
          (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

  printf("Minimum free heap size: %d bytes\n", esp_get_minimum_free_heap_size());
}

}
