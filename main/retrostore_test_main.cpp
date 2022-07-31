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

#define NUM_TEST_ITERATIONS 10

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
  auto success = rs.DownloadState(token, &state);
  if (!success) {
    ESP_LOGE(TAG, "FAILED: Downloading state");
    return;
  }
  ESP_LOGI(TAG, "testUploadDownloadSystemImage()...SUCCESS");
}

void testFailDownloadSystemImage() {
  ESP_LOGI(TAG, "testFailDownloadSystemImage()...");

  RsSystemState state;
  auto success = rs.DownloadState(12345, &state);  // non-existent token.
  if (success) {
    ESP_LOGE(TAG, "ERROR: Downloading state should have failed but did not.");
    return;
  }
  ESP_LOGI(TAG, "testFailDownloadSystemImage()...SUCCESS");
}

void testFetchSingleApp() {
  ESP_LOGI(TAG, "testFetchSingleApp()...");
  const auto DONKEY_KONG_ID = "a2729dec-96b3-11e7-9539-e7341c560175";

  RsApp app;
  auto success = rs.FetchApp(DONKEY_KONG_ID, &app);
  if (!success) {
    ESP_LOGE(TAG, "FAILED: Downloading app.");
    return;
  }

  if (app.id != DONKEY_KONG_ID) {
    ESP_LOGE(TAG, "FAILED: Returned app has the wrong key. Was: %s", app.id.c_str());
  }
  if (app.name != "Donkey Kong") {
    ESP_LOGE(TAG, "FAILED: App's name does not match. Was: %s", app.name.c_str());
  }
  if (app.release_year != 1981) {
    ESP_LOGE(TAG, "FAILED: Release year does not match. Was: %d", app.release_year);
  }
  if (app.author != "Wayne Westmoreland and Terry Gilman") {
    ESP_LOGE(TAG, "FAILED: Author does not match. Was: %s", app.author.c_str());
  }
  if (app.model != RsTrs80Model_MODEL_III) {
    ESP_LOGE(TAG, "FAILED: Model does not match. Was: %d", app.model);
  }
  if (app.screenshot_urls.size() == 0) {
    ESP_LOGE(TAG, "FAILED: App has no screenshots.");
  }
  for (auto& url : app.screenshot_urls) {
    if (url.rfind("https://", 0) != 0) {
      ESP_LOGE(TAG, "Screenshot URL invalid: %s", url.c_str());
    }
  }
  ESP_LOGI(TAG, "testFetchSingleApp()...SUCCESS");
}

void testFetchSingleAppFail() {
  ESP_LOGI(TAG, "testFetchSingleAppFail()...");
  const auto NON_EXISTENT_ID = "a2729dec_XXXX_11e7-9539-e7341c560175";

  RsApp app;
  auto success = rs.FetchApp(NON_EXISTENT_ID, &app);
  if (success) {
    ESP_LOGE(TAG, "Downloading app should have failed.");
    return;
  }
  ESP_LOGI(TAG, "testFetchSingleAppFail()... SUCCESS");
}

void initWifi() {
  ESP_LOGI(TAG, "Connecting to Wifi...");
  auto* wifi = new Wifi();
  wifi->connect(CONFIG_RS_TEST_WIFI_SSID, CONFIG_RS_TEST_WIFI_PASSWORD);
}

void runAllTests() {
  auto initialFreeHeapKb = esp_get_free_heap_size() / 1024;
  ESP_LOGI(TAG, "RetroStore API tests running... Initial free heap: %d", initialFreeHeapKb);

  for (int i = 0; i < NUM_TEST_ITERATIONS; ++i) {
    testUploadDownloadSystemImage();
    testFailDownloadSystemImage();
    testFetchSingleApp();
    testFetchSingleAppFail();
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
