#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#define EXAMPLE_ESP_WIFI_SSID               "CMCC-Y5D3"
#define EXAMPLE_ESP_WIFI_PASS               "12345678"
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD   WIFI_AUTH_WPA2_PSK  // 允许的最低认证模式
#define ESP_WIFI_SAE_MODE                   WPA3_SAE_PWE_BOTH   // SAE 模式，支持两种模式：HUNT_AND_PECK 和 HASH_TO_ELEMENT
#define EXAMPLE_H2E_IDENTIFIER              ""                  // H2E 标识符，只有在使用 HASH_TO_ELEMENT 模式时才需要设置
#define EXAMPLE_ESP_MAXIMUM_RETRY           CONFIG_ESP_MAXIMUM_RETRY
#define CONFIG_ESP_MAXIMUM_RETRY            5                   // 最大重试次数

/* 事件组（event group）允许为每个事件使用多个 bit（位），但我们这里只关心两个事件：
 * 我们已经成功连接到 AP（接入点），并且获取到了 IP 地址
 * 在达到最大重试次数后，连接失败 */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

/* FreeRTOS event group to signal when we are connected*/
// 创建一个事件组来标志连接状态，事件组允许多个事件位。
static EventGroupHandle_t s_wifi_event_group;

static int s_retry_num = 0;

static const char *TAG = "wifi station";

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

esp_err_t wifi_init_station(void)
{
    // 只有当编译允许更高日志时，才尝试提升 wifi 模块日志等级。
    if (CONFIG_LOG_MAXIMUM_LEVEL > CONFIG_LOG_DEFAULT_LEVEL) {
        /* If you only want to open more logs in the wifi module, you need to make the max level greater than the default level,
         * and call esp_log_level_set() before esp_wifi_init() to improve the log level of the wifi module. */
        esp_log_level_set("wifi", CONFIG_LOG_MAXIMUM_LEVEL);
    }

    // 创建一个事件组来标志连接状态，事件组允许多个事件位，但我们只关心两个事件：
    // - 我们已经连接到AP并获得IP地址
    // - 连接失败并达到最大重试次数
    s_wifi_event_group = xEventGroupCreate();

    // 初始化 ESP-IDF 的网络接口抽象层（TCP/IP 子系统）
    ESP_ERROR_CHECK(esp_netif_init());

    // 创建系统默认事件循环
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    // 创建一个默认的 WiFi Station 网络接口对象
    esp_netif_create_default_wifi_sta();

    // 内存分配策略 缓冲区大小 任务优先级 WiFi 内部参数
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    // 真正初始化 WiFi 驱动
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));
    
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS,
            /*
            如果密码符合 WPA2 标准（密码长度 ≥ 8），则认证模式阈值（authmode threshold）默认会重置为 WPA2。
            这意味着如果你设置了一个符合 WPA2 标准的密码，但路由器使用的是过时的 WEP/WPA 协议，设备将无法连接。
            如果你希望设备连接到已经过时的 WEP/WPA 网络，请将 threshold 的值设置为 WIFI_AUTH_WEP 或 WIFI_AUTH_WPA_PSK，
            并且将密码的长度和格式设置为符合 WIFI_AUTH_WEP / WIFI_AUTH_WPA_PSK 标准的要求。
            */
            .threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
            .sae_pwe_h2e = ESP_WIFI_SAE_MODE,
            .sae_h2e_identifier = EXAMPLE_H2E_IDENTIFIER,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
    return ESP_OK;
}
