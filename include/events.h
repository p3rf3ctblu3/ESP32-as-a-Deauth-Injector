#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_event.h"

#include "events.h"

#define ASSIGNED_IP_BIT      BIT0
#define MAC_BUFFER_FULL_BIT  BIT1
#define MQTT_PUBLISHED_BIT   BIT2
#define MQTT_CONNECTED_BIT   BIT3
#define MQTT_SUBSCRIBED_BIT  BIT4

extern EventGroupHandle_t s_evt; 

extern QueueHandle_t inject_queue;
extern SemaphoreHandle_t inject_sem; 

extern volatile bool allow_connection;
extern volatile bool sntp_initialized;

bool sync_time(void);
extern void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data);

extern void ip_event_handler(void* arg, esp_event_base_t event_base,
                            int32_t event_id, void* event_data);