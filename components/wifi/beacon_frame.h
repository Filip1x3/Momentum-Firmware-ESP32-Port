#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Edit an existing ordinary 802.11 beacon in memory. No radio operations.
 * Resize the SSID IE and preserve following IEs instead of leaving padding
 * inside the advertised IE stream. Invalid/truncated input is not modified. */
bool wlan_beacon_frame_set_ssid(
    uint8_t* frame, size_t* length, size_t capacity, const char* ssid, uint8_t channel);
