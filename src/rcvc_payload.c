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

mac_pair_t inject_array[MAX_TARGETS];
int inject_array_count;
uint32_t last_processed_id = 0; 

void process_payload(char *payload) {
    if (payload == NULL) return;

    // Log the exact JSON payload received
    ESP_LOGI("PROCESS", "Processing JSON: %s", payload);

    cJSON *parsed_json = cJSON_Parse(payload);
    if (parsed_json != NULL) {
        
        cJSON *id_item = cJSON_GetObjectItem(parsed_json, "id");
        
        if (cJSON_IsNumber(id_item)) {
            uint32_t current_id = id_item->valueint;
            
            // Deduplication Check
            if (current_id > last_processed_id) {
                last_processed_id = current_id; // Update our memory
                
                cJSON *src_item = cJSON_GetObjectItem(parsed_json, "src");
                cJSON *dst_item = cJSON_GetObjectItem(parsed_json, "dst");

                if (cJSON_IsString(src_item) && cJSON_IsString(dst_item)) {
                    uint8_t temp_src[6], temp_dst[6]; 

                    if (sscanf(src_item->valuestring, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", 
                        &temp_src[0], &temp_src[1], &temp_src[2], &temp_src[3], &temp_src[4], &temp_src[5]) == 6 &&
                        sscanf(dst_item->valuestring, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", 
                        &temp_dst[0], &temp_dst[1], &temp_dst[2], &temp_dst[3], &temp_dst[4], &temp_dst[5]) == 6) {
                
                        // Save directly to the array if space permits
                        if (inject_array_count < MAX_TARGETS) {
                            memcpy(inject_array[inject_array_count].source_mac, temp_src, 6);
                            memcpy(inject_array[inject_array_count].dest_mac, temp_dst, 6);
                            inject_array_count++;
                        } else {
                            ESP_LOGW("PROCESS", "Target array is full! Cannot store more MAC pairs.");
                        }
                    }
                }
            } else {
                ESP_LOGW("PROCESS", "Dropped duplicate message (ID: %u <= %u)", current_id, last_processed_id);
            }
        }
        cJSON_Delete(parsed_json);
    }
    
    // Always free the text payload
    free(payload); 
}

void print_inject_array(void) {
    if (xSemaphoreTake(inject_sem, portMAX_DELAY) == pdTRUE) {
        ESP_LOGI("ARRAY", "=== CURRENT INJECT ARRAY (%d items) ===", inject_array_count);
        
        for (int i = 0; i < inject_array_count; i++) {
            ESP_LOGI("ARRAY", "[%d] %02x:%02x:%02x:%02x:%02x:%02x -> %02x:%02x:%02x:%02x:%02x:%02x",
                i,
                inject_array[i].source_mac[0], inject_array[i].source_mac[1], 
                inject_array[i].source_mac[2], inject_array[i].source_mac[3], 
                inject_array[i].source_mac[4], inject_array[i].source_mac[5],
                
                inject_array[i].dest_mac[0], inject_array[i].dest_mac[1], 
                inject_array[i].dest_mac[2], inject_array[i].dest_mac[3], 
                inject_array[i].dest_mac[4], inject_array[i].dest_mac[5]
            );
        }
        
        ESP_LOGI("ARRAY", "========================================");
        xSemaphoreGive(inject_sem);
    }
}