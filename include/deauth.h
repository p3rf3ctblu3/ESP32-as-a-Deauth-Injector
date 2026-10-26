#pragma once

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

void send_deauth_frame(const uint8_t *ap_bssid, const uint8_t *client_mac);
void DEAUTHENTICATE(void); 

