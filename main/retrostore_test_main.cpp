/* CLI that uses the retrostore to test functionality.
 *
 * This serves both as a test and as documentation on
 * how to use the API.
 */
#include <cstdlib>
#include <set>
#include <stdio.h>
#include <vector>

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

#define NUM_TEST_ITERATIONS 1

static const char *TAG = "retrostore-tester";
ESP_EVENT_DEFINE_BASE(WINSTON_EVENT);

using namespace std;
using namespace retrostore;


RetroStore rs;


void createRandomTestState(RsSystemState* state) {
  state->model = RsTrs80Model_MODEL_4;
  state->registers.af = 12;
  state->registers.sp = 23;
  state->registers.de = 34;
  state->registers.hl_prime = 45;
  state->registers.i = 56;
  state->registers.r_2 = 67;

  for (int i = 0; i < 1; ++i) {
    RsMemoryRegion region;
    region.start = rand() % 1000 + 1;
    region.length = 1024;
    std::unique_ptr<uint8_t> data(static_cast<uint8_t*>(malloc(sizeof(uint8_t) * 1024)));
    for (int d = 0; d < region.length; ++d) {
      data.get()[d] = rand() % 256;
    }
    region.data = std::move(data);
    state->regions.push_back(std::move(region));
  }
}

void testUploadDownloadSystemState() {
  ESP_LOGI(TAG, "testUploadDownloadSystemState()...");
  // Create a random state to upload.
  RsSystemState state1;
  createRandomTestState(&state1);

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


  RsSystemState state3;
  if (!rs.DownloadState(token, true /* exclude_memory_region_data */, &state3)) {
    ESP_LOGE(TAG, "FAILED: Downloading state");
    return;
  }

  // Make sure the same number of regions are returned.
  if (state3.regions.size() != state1.regions.size()) {
    ESP_LOGE(TAG, "FAILED: Downloaded state3 does not have expected number of memory regions.");
    return;
  }

  // Make sure that none of the regions have data.
  for (int i = 0; i < state3.regions.size(); ++i) {
    if (state3.regions[i].data.get() != NULL) {
      ESP_LOGE(TAG, "FAILED: Downloaded state3 should not contain memory region data.");
      return;
    }
  }

  ESP_LOGI(TAG, "testUploadDownloadSystemState()...SUCCESS");
}

bool helper_downloadAndCheckRegion(int n, int token, int start, int length, const uint8_t* want) {
  RsMemoryRegion region;
  if (!rs.DownloadStateMemoryRange(token, start, length, &region)) {
    ESP_LOGE(TAG, "n=%d Downloading memory regions failed.", n);
    return false;
  }
  if (region.length != length) {
    ESP_LOGE(TAG, "n=%d Received data length does not match request: %d vs %d",
             n, region.length, length);
    return false;
  }
  bool success = true;
  for (int i = 0; i < length; ++i) {
    if (region.data.get()[i] != want[i]) {
      ESP_LOGE(TAG, "n=%d Recv data at idx=%d does not match. (%d vs %d)",
              n, i, region.data.get()[i], want[i]);
      success = false;
    }
  }
  return success;
}

void testDownloadStateMemoryRegions() {
  ESP_LOGI(TAG, "testDownloadStateMemoryRegions()...");
  RsSystemState state;
  state.model = RsTrs80Model_MODEL_III;

  {
    RsMemoryRegion region;
    region.start = 1000;
    region.length = 4;
    std::unique_ptr<uint8_t> data(new uint8_t[4]{42, 43, 44, 45});
    region.data = std::move(data);
    state.regions.push_back(std::move(region));
  }
  {
    RsMemoryRegion region;
    region.start = 1100;
    region.length = 8;
    std::unique_ptr<uint8_t> data(new uint8_t[8]{1, 2, 3, 4, 5, 6, 7, 8});
    region.data = std::move(data);
    state.regions.push_back(std::move(region));
  }
  {
    RsMemoryRegion region;
    region.start = 1108;
    region.length = 6;
    std::unique_ptr<uint8_t> data(new uint8_t[6]{11, 22, 33, 44, 55, 66});
    region.data = std::move(data);
    state.regions.push_back(std::move(region));
  }
  {
    RsMemoryRegion region;
    region.start = 1120;
    region.length = 5;
    std::unique_ptr<uint8_t> data(new uint8_t[8]{101, 102, 103, 104, 105});
    region.data = std::move(data);
    state.regions.push_back(std::move(region));
  }

  // Upload the state so we can download it again and check the API.
  int token = rs.UploadState(state);
  if (token < 100 || token > 999) {
    ESP_LOGE(TAG, "FAILED: Non-valid token: %d", token);
    return;
  }
  ESP_LOGI(TAG, "Got token: %d", token);

  // Exact match of uploads
  uint8_t want1[] = {42, 43, 44, 45};
  if (!helper_downloadAndCheckRegion(1, token, 1000, 4, want1)) return;
  uint8_t want2[] = {11, 22, 33, 44, 55, 66};
  if (!helper_downloadAndCheckRegion(2, token, 1108, 6, want2)) return;

  // Requesting two connected regions at once..
  uint8_t want3[] = {1, 2, 3, 4, 5, 6, 7, 8, 11, 22, 33, 44, 55, 66};
  if (!helper_downloadAndCheckRegion(3, token, 1100, 14, want3)) return;

  // Requesting more (padding) should result in '0'.
  uint8_t want4[] = {0, 0, 42, 43, 44, 45, 0, 0};
  if (!helper_downloadAndCheckRegion(4, token, 998, 8, want4)) return;
  
  // Request half into one.
  uint8_t want5[] = {44, 45, 0, 0};
  if (!helper_downloadAndCheckRegion(5, token, 1002, 4, want5)) return;
  
  // Request half into one across and half into another region.
  uint8_t want6[] = {44, 55, 66, 0, 0, 0, 0, 0, 0, 101, 102, 103};
  if (!helper_downloadAndCheckRegion(6, token, 1111, 12, want6)) return;

  ESP_LOGI(TAG, "testDownloadStateMemoryRegions()...SUCCESS");
}

void testFailDownloadSystemState() {
  ESP_LOGI(TAG, "testFailDownloadSystemState()...");

  RsSystemState state;
  auto success = rs.DownloadState(12345, &state);  // non-existent token.
  if (success) {
    ESP_LOGE(TAG, "ERROR: Downloading state should have failed but did not.");
    return;
  }
  ESP_LOGI(TAG, "testFailDownloadSystemState()...SUCCESS");
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
    ESP_LOGI(TAG, "Descriptions\n\"%s\" ", app.description.c_str());
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

void testQueryAppsWithMediaTypes() {
  ESP_LOGI(TAG, "testQueryAppsWithMediaTypes()...");

  {
    std::vector<RsAppNano> appsNoFilter;
    std::vector<RsMediaType> hasTypes;  // empty.
    auto success = rs.FetchAppsNano(0, 10, "ldos OR donkey", hasTypes, &appsNoFilter);
    if (!success) {
      ESP_LOGE(TAG, "Downloading apps failed.");
      return;
    }
    std::set<std::string> appNamesNoFilter;
    for (int i = 0; i < appsNoFilter.size(); ++i) {
      ESP_LOGI(TAG, "Found app '%s'", appsNoFilter[i].name.c_str());
      appNamesNoFilter.insert(appsNoFilter[i].name);
    }

    if (appNamesNoFilter.find("Donkey Kong") == appNamesNoFilter.end()) {
      ESP_LOGE(TAG, "Donkey Kong not found.");
      return;
    }
    if (appNamesNoFilter.find("LDOS - Model I") == appNamesNoFilter.end()) {
      ESP_LOGE(TAG, "LDOS - Model I not found.");
      return;
    }
    if (appNamesNoFilter.find("LDOS - Model III") == appNamesNoFilter.end()) {
      ESP_LOGE(TAG, "LDOS - Model III not found.");
      return;
    }
  }

  // Next we use the same query, but add the need to have a CMD, which
  // the LDOS entries do not have, but Donkey Kong does.
  {
    std::vector<RsAppNano> appsWithFilter;
    std::vector<RsMediaType> hasTypes;
    hasTypes.push_back(RsMediaType_COMMAND);
    auto success = rs.FetchAppsNano(0, 10, "ldos OR donkey", hasTypes, &appsWithFilter);
    if (!success) {
      ESP_LOGE(TAG, "Downloading apps failed.");
      return;
    }
    std::set<std::string> appNamesWithFilter;
    for (int i = 0; i < appsWithFilter.size(); ++i) {
      ESP_LOGI(TAG, "Found app '%s'", appsWithFilter[i].name.c_str());
      appNamesWithFilter.insert(appsWithFilter[i].name);
    }

    if (appNamesWithFilter.find("Donkey Kong") == appNamesWithFilter.end()) {
      ESP_LOGE(TAG, "Donkey Kong not found.");
      return;
    }
    if (appNamesWithFilter.find("LDOS - Model I") != appNamesWithFilter.end()) {
      ESP_LOGE(TAG, "LDOS - Model I found, but should NOT be found.");
      return;
    }
    if (appNamesWithFilter.find("LDOS - Model III") != appNamesWithFilter.end()) {
      ESP_LOGE(TAG, "LDOS - Model III found, but should NOT be found.");
      return;
    }
  }

  ESP_LOGI(TAG, "testQueryAppsWithMediaTypes()... SUCCESS");
}

void testQueryAppsNano() {
  ESP_LOGI(TAG, "testQueryAppsNano()...");

  std::vector<RsAppNano> apps;
  std::vector<RsMediaType> hasType;  // empty
  auto success = rs.FetchAppsNano(0, 1, "Weerd", hasType, &apps);
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

void testFetchMediaImages() {
  ESP_LOGI(TAG, "testFetchMediaImages()...");

  const std::string BREAKDOWN_ID("29b20252-680f-11e8-b4a9-1f10b5491ef5");
  std::vector<RsMediaType> types;
  types.push_back(RsMediaType_COMMAND);

  std::vector<RsMediaImage> images;
  auto success = rs.FetchMediaImages(BREAKDOWN_ID, types, &images);

  if (!success) {
    ESP_LOGE(TAG, "Downloading apps (nano) failed.");
    return;
  }

  if (images.size() != 1) {
    ESP_LOGE(TAG, "Expected 1 media image of type COMMAND, but got %d", images.size());
    return;
  }
  if (images[0].filename != "command.CMD") {
    ESP_LOGE(TAG, "Queries media image filename not as expected: %s", images[0].filename.c_str());
    return;
  }

  ESP_LOGI(TAG, "Media image size is: %d", images[0].data_size);
  if (images[0].data_size <= 0) {
    ESP_LOGE(TAG, "Data size of media image is zero: %d", images[0].data_size);
    return;
  }
  ESP_LOGI(TAG, "testFetchMediaImages()... SUCCESS");
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
    testUploadDownloadSystemState();
    testDownloadStateMemoryRegions();
    testFailDownloadSystemState();
    testFetchSingleApp();
    testFetchSingleAppFail();
    testFetchMultipleApps();
    testQueryApps();
    testQueryAppsWithMediaTypes();
    testFetchMultipleAppsNano();
    testQueryAppsNano();
    testFetchMediaImages();
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
