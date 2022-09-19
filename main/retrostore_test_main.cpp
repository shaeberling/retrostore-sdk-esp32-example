/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <cstdlib>
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
  // Create a random state to upload.
  RsSystemState state1;
  state1.model = RsTrs80Model_MODEL_4;
  state1.registers.af = 12;
  state1.registers.sp = 23;
  state1.registers.de = 34;
  state1.registers.hl_prime = 45;
  state1.registers.i = 56;
  state1.registers.r_2 = 67;

  for (int i = 0; i < 1; ++i) {
    RsMemoryRegion region;
    region.start = rand() % 1000 + 1;
    region.length = 1024;
    std::unique_ptr<uint8_t> data(static_cast<uint8_t*>(malloc(sizeof(uint8_t) * 1024)));
    for (int d = 0; d < region.length; ++d) {
      data.get()[d] = rand() % 256;
    }
    region.data = std::move(data);
    state1.regions.push_back(std::move(region));
  }

  int token = rs.UploadState(state1);
  if (token < 100 || token > 999) {
    ESP_LOGE(TAG, "FAILED: Non-valid token: %d", token);
    return;
  }
  ESP_LOGI(TAG, "Got token: %d", token);

  RsSystemState state2;
  if (!rs.DownloadState(token, &state2)) {
    ESP_LOGE(TAG, "FAILED: Downloading state");
    return;
  }

  // Compare the two states, they should be the same.
  if (state1.model != state2.model) ESP_LOGE(TAG, "FAILED: 'model' is different");
  if (state1.registers.af != state2.registers.af) ESP_LOGE(TAG, "FAILED: 'af' is different");
  if (state1.registers.sp != state2.registers.sp) ESP_LOGE(TAG, "FAILED: 'sp' is different");
  if (state1.registers.de != state2.registers.de) ESP_LOGE(TAG, "FAILED: 'de' is different");
  if (state1.registers.hl_prime != state2.registers.hl_prime) ESP_LOGE(TAG, "FAILED: 'hl_prime' is different");
  if (state1.registers.i != state2.registers.i) ESP_LOGE(TAG, "FAILED: 'i' is different");
  if (state1.registers.r_2 != state2.registers.r_2) ESP_LOGE(TAG, "FAILED: 'r_2' is different");

  bool success = true;
  for (int i = 0; i < state2.regions.size(); ++i) {
    if (state1.regions[i].start != state2.regions[i].start) {
      ESP_LOGE(TAG, "Start of memory region %d does not match.", i);
      success = false;
    }
    if (state1.regions[i].length != state2.regions[i].length) {
      ESP_LOGE(TAG, "Start of memory region %d does not match.", i);
      success = false;
    }

    for (int d = 0; d < state1.regions[i].length; ++d) {
      if (state1.regions[i].data.get()[d] != state2.regions[i].data.get()[d]) {
        ESP_LOGE(TAG, "Memory region %d differs first at %d", i, d);
        success = false;
        break;
      }
    }
  }
  if (!success) return;

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

void testFetchMultipleApps() {
  ESP_LOGI(TAG, "testFetchMultipleApps()...");

  std::vector<RsApp> apps;
  auto success = rs.FetchApps(0, 5, &apps);
  if (!success) {
    ESP_LOGE(TAG, "Downloading apps failed.");
    return;
  }
  if (apps.size() != 5) {
    ESP_LOGE(TAG, "Expected 5 apps, only got %d", apps.size());
    return;
  }
  for (const auto& app : apps) {
    ESP_LOGI(TAG, "Got app: [%s] - %s ", app.id.c_str(), app.name.c_str());
  }

  ESP_LOGI(TAG, "testFetchMultipleApps()... SUCCESS");
}

void testFetchMultipleAppsNano() {
  ESP_LOGI(TAG, "testFetchMultipleAppsNano()...");

  std::vector<RsAppNano> apps;
  auto success = rs.FetchAppsNano(0, 5, &apps);
  if (!success) {
    ESP_LOGE(TAG, "Downloading apps (nano) failed.");
    return;
  }
  if (apps.size() != 5) {
    ESP_LOGE(TAG, "Expected 5 apps (nano), only got %d", apps.size());
    return;
  }
  for (const auto& app : apps) {
    ESP_LOGI(TAG, "Got app: [%s] - %s ", app.id.c_str(), app.name.c_str());
  }

  ESP_LOGI(TAG, "testFetchMultipleAppsNano()... SUCCESS");
}

void testQueryApps() {
  ESP_LOGI(TAG, "testQueryApps()...");

  std::vector<RsApp> apps;
  auto success = rs.FetchApps(0, 1, "Weerd", &apps);
  if (!success) {
    ESP_LOGE(TAG, "Downloading apps failed.");
    return;
  }
  if (apps.size() != 1) {
    ESP_LOGE(TAG, "Expected one app, but got %d", apps.size());
    return;
  }

  if (apps[0].name != "Weerd") {
    ESP_LOGE(TAG, "Queried app name not as expected: %s", apps[0].name.c_str());
    return;
  }

  if (apps[0].id != "59a9ea84-e52c-11e8-9abc-ab7e2ee8e918") {
    ESP_LOGE(TAG, "Queried app ID not as expected: %s", apps[0].id.c_str());
    return;
  }
  ESP_LOGI(TAG, "testQueryApps()... SUCCESS");
}

void testQueryAppsNano() {
  ESP_LOGI(TAG, "testQueryAppsNano()...");

  std::vector<RsAppNano> apps;
  auto success = rs.FetchAppsNano(0, 1, "Weerd", &apps);
  if (!success) {
    ESP_LOGE(TAG, "Downloading apps (nano) failed.");
    return;
  }
  if (apps.size() != 1) {
    ESP_LOGE(TAG, "Expected one app (nano), but got %d", apps.size());
    return;
  }

  if (apps[0].name != "Weerd") {
    ESP_LOGE(TAG, "Queried app name not as expected: %s", apps[0].name.c_str());
    return;
  }

  if (apps[0].id != "59a9ea84-e52c-11e8-9abc-ab7e2ee8e918") {
    ESP_LOGE(TAG, "Queried app ID not as expected: %s", apps[0].id.c_str());
    return;
  }
  ESP_LOGI(TAG, "testQueryAppsNano()... SUCCESS");
}



void initWifi() {
  ESP_LOGI(TAG, "Connecting to Wifi...");
  auto* wifi = new Wifi();
  wifi->connect(CONFIG_RS_TEST_WIFI_SSID, CONFIG_RS_TEST_WIFI_PASSWORD);
}

void runAllTests() {
  auto initialFreeHeapKb = esp_get_free_heap_size() / 1024;
  ESP_LOGI(TAG, "RetroStore API tests running... Initial free heap: %d", initialFreeHeapKb);
  srand(time(nullptr));

  for (int i = 0; i < NUM_TEST_ITERATIONS; ++i) {
    testUploadDownloadSystemImage();
    testFailDownloadSystemImage();
    testFetchSingleApp();
    testFetchSingleAppFail();
    testFetchMultipleApps();
    testQueryApps();
    testFetchMultipleAppsNano();
    testQueryAppsNano();
    auto newFreeHeapKb = esp_get_free_heap_size() / 1024;
    auto diffHeapKb =  initialFreeHeapKb - newFreeHeapKb;
    ESP_LOGI(TAG, "After run [%d], free heap is %d, total diff is %d kb", i, newFreeHeapKb, diffHeapKb);
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
