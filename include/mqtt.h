/*
 * mqtt.h
 * ------------------------------------------------------------------
 * Public interface for the MQTT/TLS connection to the NanoMQ broker.
 *
 *  - client             : handle for the single global MQTT client
 *                          instance, shared with main.c.
 *  - mqtt_init()         : configures and creates the client (TLS
 *                          cert, broker URI, client ID) but does not
 *                          start it — main.c calls
 *                          esp_mqtt_client_start() once Wi-Fi/IP is
 *                          up.
 *  - mqtt_event_handler() : registered as the client's event
 *                           callback; implemented in mqtt.c.
 */

#pragma once 

#include <stdint.h>
#include "esp_event.h"     // esp_event_base_t
#include "mqtt_client.h"   // esp_mqtt_client_handle_t

extern esp_mqtt_client_handle_t client; 

void mqtt_init(void);
void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
