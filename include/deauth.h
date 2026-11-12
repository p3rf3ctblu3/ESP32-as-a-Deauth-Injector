#pragma once

#include <stdint.h>

void send_deauth_frame(const uint8_t *ap_bssid, const uint8_t *client_mac);
void DEAUTHENTICATE(void); 

