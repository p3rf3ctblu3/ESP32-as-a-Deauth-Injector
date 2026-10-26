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

#define MAX_TARGETS 100

typedef struct {
    uint8_t source_mac[6];
    uint8_t dest_mac[6]; 
//  uint8_t bssid[6]; 
} mac_pair_t;

// wrapper struct for inject buffer 
typedef struct {
    uint32_t   id;       // The unique ID from Node-RED
    mac_pair_t macs;     // The untouched mac_pair_t struct
} inject_t;

extern mac_pair_t inject_array[MAX_TARGETS];
extern int inject_array_count;
extern SemaphoreHandle_t inject_sem;

void process_payload(char *payload);
void print_inject_array(void); 

