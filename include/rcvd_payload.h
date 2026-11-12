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

