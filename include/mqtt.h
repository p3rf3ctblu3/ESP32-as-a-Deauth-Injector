#pragma once 

#include <stdint.h>
#include "esp_event.h"     // esp_event_base_t
#include "mqtt_client.h"   // esp_mqtt_client_handle_t

extern esp_mqtt_client_handle_t client; 

void mqtt_init(void);
void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
