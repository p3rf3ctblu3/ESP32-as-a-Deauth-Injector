/*
 * deauth.h
 * ------------------------------------------------------------------
 * Public interface for sending raw 802.11 deauthentication frames.
 *
 *  - send_deauth_frame() : builds and transmits one deauth frame for
 *                          a given {AP BSSID, client MAC} pair.
 *  - DEAUTHENTICATE()    : loops over every pair currently stored in
 *                          inject_array and deauths each one.
 */

#pragma once

#include <stdint.h>

void send_deauth_frame(const uint8_t *ap_bssid, const uint8_t *client_mac);
void DEAUTHENTICATE(void); 

