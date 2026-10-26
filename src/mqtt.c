/* 
    adapted from https://github.com/espressif/esp-mqtt/blob/master/examples/ssl/main/app_main.c
*/

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_system.h"
#include <inttypes.h>
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "mqtt.h"
#include "mqtt_client.h"
#include "esp_crt_bundle.h"
#include "cert.h"
#include "events.h"
#include "freertos/event_groups.h"
#include "credentials.h"

// set URI to params n a config file somehere along with the ssid/password, .gitignore 

static const char *TAG = "MQTT";
int inj_queue_size = 0;

/*
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {

    //ESP_LOGI(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32, base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    //int msg_id;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        if (esp_mqtt_client_subscribe(client, MQTT_INJECT_TOPIC, 1) == pdTRUE) {
            ESP_LOGI(TAG, "MQTT SUBSCRIBED TO sn1ff3r/1nj3ct"); 
        } else {
            ESP_LOGI(TAG, "FAILED TO SUBSCRIBE");
        }
        xEventGroupSetBits(s_evt, MQTT_CONNECTED_BIT);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d, return code=0x%02x ", event->msg_id, (uint8_t)*event->data);
        xEventGroupSetBits(s_evt, MQTT_SUBSCRIBED_BIT);
        break;

    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        xEventGroupSetBits(s_evt, MQTT_PUBLISHED_BIT);
        break;

    // when esp receives a msg from the broker, this event is triggered and the msg is stored in event->data
    
    case MQTT_EVENT_DATA: 
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");

        // mqtt event data triggered when subscribed
        // save the batch to the inject queue! (make sure at most 100 messages from node-red dashboard)
        if (strncmp(event->topic, MQTT_INJECT_TOPIC, event->topic_len) == 0) {
            char *payload = malloc(event->data_len + 1); // +1 for termination operator
            if (payload == NULL) 
                break;
            memcpy(payload, event->data, event->data_len);
            payload[event->data_len] = '\0';
            
            // APPLY BACKPRESSURE -> BLOCK PACKETS UNTIL MEMORY IS FREED, NANOMQ HOLDS THE MESSAGES. DELAY SLOWS DONE ACKS (WITH QOS 1)
            // NANOMQ SEES ITS INTERNAL QUEUE IS GROWING SO IT STOPS SENDING PACKETS UNTIL INTERNAL QUEUE STARTS EMPTYING 
            if (xQueueSend(inject_queue, &payload, 0) != pdPASS) { 
                free(payload);  
            } 

            // track if reading the rcvd messages empties the queue for the next batch
            int actual_size = uxQueueMessagesWaiting(inject_queue);
            ESP_LOGI(TAG, "INJECT QUEUE ACTUAL SIZE: %d", actual_size);
            
        }

        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");

        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            ESP_LOGI(TAG, "Last error code reported from esp-tls: 0x%x", event->error_handle->esp_tls_last_esp_err);
            ESP_LOGI(TAG, "Last tls stack error number: 0x%x", event->error_handle->esp_tls_stack_err);
            ESP_LOGI(TAG, "Last captured errno : %d (%s)",  event->error_handle->esp_transport_sock_errno,
                     strerror(event->error_handle->esp_transport_sock_errno));
        } else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
            ESP_LOGI(TAG, "Connection refused error: 0x%x", event->error_handle->connect_return_code);
        } else {
            ESP_LOGW(TAG, "Unknown error type: 0x%x", event->error_handle->error_type);
        }

        break;

    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

void mqtt_init(void) {
    esp_mqtt_client_config_t mqtt_cfg = {
        .uri             = "mqtts://192.168.1.69:8883",
        .client_id       = "l0n3_sn1ff3r",
        .cert_pem = ca_cert_pem,
        .task_stack      = 8192, // increase if qi get stack smashing
        .disable_clean_session = true,
        .keepalive = 120 
    };
    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    ESP_LOGI(TAG, "Successful Init.");
}
