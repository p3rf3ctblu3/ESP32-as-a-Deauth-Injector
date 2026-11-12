/*
 * rcvd_payload.h
 * ------------------------------------------------------------------
 * Defines the shared data structures used to store and inject MAC
 * address pairs captured by the sniffer, plus the target array that
 * Node-RED commands populate for the deauth stage.
 *
 *  - mac_pair_t   : a single {source_mac, dest_mac} pair, used both
 *                    for sniffed packets and injection targets.
 *  - inject_t     : wraps a mac_pair_t with Node-RED's message ID,
 *                    used for de-duplicating incoming inject commands.
 *  - inject_array : fixed-size buffer (MAX_TARGETS) of validated
 *                    targets, filled by process_payload() and
 *                    consumed by DEAUTHENTICATE() in deauth.c.
 *
 * Declares the functions implemented in rcvd_payload.c (see below).
 */

#pragma once

#include <stdint.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

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

