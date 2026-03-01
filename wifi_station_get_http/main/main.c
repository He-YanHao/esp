#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "esp_private/esp_clk.h"
#include "esp_pm.h"
#include "esp_flash.h"
#include "esp_flash_spi_init.h"
#include "esp_flash.h"
#include "nvs_flash.h"
#include "station.h"

void app_main(void)
{
    // 初始化NVS 用来存储WiFi配置等数据
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
        
    // 获取芯片信息
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    printf("This is %s chip with %d CPU core(s), %s%s%s%s, ",
           CONFIG_IDF_TARGET,
           chip_info.cores,
           (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "WiFi/" : "",
           (chip_info.features & CHIP_FEATURE_BT) ? "BT" : "",
           (chip_info.features & CHIP_FEATURE_BLE) ? "BLE" : "",
           (chip_info.features & CHIP_FEATURE_IEEE802154) ? ", 802.15.4 (Zigbee/Thread)" : "");
    unsigned major_rev = chip_info.revision / 100;
    unsigned minor_rev = chip_info.revision % 100;
    printf("silicon revision v%d.%d, ", major_rev, minor_rev);

    // 动态频率配置
    esp_pm_config_t pm_cfg = {
        .max_freq_mhz = 240,
        .min_freq_mhz = 80,
        .light_sleep_enable = false
    };
    esp_pm_configure(&pm_cfg);
    // 输出时钟配置
    printf("CPU frequency: %d MHz\n",
           esp_clk_cpu_freq() / 1000000);
    printf("CPU max freq: %d MHz, min freq: %d MHz, light sleep: %d\n",
            pm_cfg.max_freq_mhz, pm_cfg.min_freq_mhz, pm_cfg.light_sleep_enable);

    // 获取flash大小
    uint32_t flash_size;
    if(esp_flash_get_size(NULL, &flash_size) != ESP_OK) {
        printf("Get flash size failed");
        return;
    }
    printf("%" PRIu32 "MB %s flash\n", flash_size / (uint32_t)(1024 * 1024),
           (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");
    printf("Minimum free heap size: %" PRIu32 " bytes\n", esp_get_minimum_free_heap_size());

    // 初始化WiFi连接
    wifi_init_station();
    // 等待WiFi连接稳定
    vTaskDelay(500 / portTICK_PERIOD_MS);
    // 进行HTTP GET请求测试
    http_get_test();
}
