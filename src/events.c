#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "string.h"
#include "cJSON.h"
#include "mqtt.h"
#include "esp_sntp.h"
#include <time.h>
#include <sys/time.h>
#include "esp_system.h"
#include "lwip/ip_addr.h"
#include "freertos/event_groups.h"
#include "mqtt_client.h"
#include "credentials.h"
#include "events.h"
#include "rcvd_payload.h"
#include "deauth.h"


//------------------------------------ WIFI TIME SYNC AND EVENT HANDLERS --------------------------------------

// only if i needed to store timestamps i would also need an external clock connected to the esp. 
// since i only care about tls certificate timeout and my mode that formats and sends data is responsible for that, it doesnt matter. 
// this isnt a critical application

bool sync_time(void) {

    ESP_LOGI("TIME", "Initializing SNTP...");
    
    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_setservername(1, "time.nist.gov");
    esp_sntp_init();

    sntp_sync_status_t status = SNTP_SYNC_STATUS_RESET;
    for (int retry = 0; retry < 15; retry++) {
        status = sntp_get_sync_status();      // read once per iteration
        if (status == SNTP_SYNC_STATUS_COMPLETED) break;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    time_t now = 0;
    time(&now);
    bool ok = (status == SNTP_SYNC_STATUS_COMPLETED) || (now > 1700000000);
    ESP_LOGI("TIME", "status=%d epoch=%ld", (int)status, (long)now);
    return ok;
}

// triggers once wifi is connected
void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (allow_connection) {
            esp_wifi_connect();
            ESP_LOGI("WIFI", "Retrying connection to AP...");  // NEED TO GET ASSIGNED WITH AN IP ADDRESS, SEE BELOW
        } else {
            ESP_LOGI("WIFI", "Disconnected safely for sniffer mode.");
        }
    }
}

void ip_event_handler(void* arg, esp_event_base_t event_base,
                            int32_t event_id, void* event_data) {
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        // default esp time at 1970. sync for tls handshake and valid certificate timeout
        if (!sntp_initialized) {
            if (sync_time()) {
                sntp_initialized = true;
            } else {
                ESP_LOGW("STNP_SYNC:", "failed on first attempt");
            }
        }
        xEventGroupSetBits(s_evt, ASSIGNED_IP_BIT);
    }
}