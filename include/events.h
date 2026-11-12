/*
 * events.h
 * ------------------------------------------------------------------
 * Declares the shared synchronization state used to coordinate the
 * sniff -> connect -> publish -> inject state machine in main.c:
 *
 *  - s_evt            : event group whose bits signal when each
 *                        stage completes (IP assigned, MAC buffer
 *                        full, MQTT connected/subscribed/published).
 *  - inject_queue      : queue of raw JSON strings waiting to be
 *                         parsed by process_payload().
 *  - inject_sem        : mutex protecting inject_array from
 *                         concurrent access.
 *  - allow_connection   : flag toggled between sniffer mode
 *                          (promiscuous, Wi-Fi disconnected) and
 *                          station mode (Wi-Fi connected to AP).
 *  - sntp_initialized  : tracks whether time has been synced yet,
 *                        so it's only attempted once per boot.
 *
 * Also declares the Wi-Fi and IP event handlers implemented in
 * events.c.
 */

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