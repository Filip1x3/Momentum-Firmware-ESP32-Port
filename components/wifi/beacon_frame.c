#include "beacon_frame.h"
#include <string.h>

bool wlan_beacon_frame_set_ssid(
    uint8_t* frame, size_t* length, size_t capacity, const char* ssid, uint8_t channel) {
    const size_t fixed_header = 36;
    if(!frame || !length || !ssid || *length < fixed_header + 2 || *length > capacity ||
       channel < 1 || channel > 14) return false;

    size_t ssid_offset = SIZE_MAX, channel_offset = SIZE_MAX;
    for(size_t offset = fixed_header; offset < *length;) {
        if(*length - offset < 2) return false;
        size_t size = frame[offset + 1];
        if(size > *length - offset - 2) return false;
        if(frame[offset] == 0) {
            if(ssid_offset != SIZE_MAX || size > 32) return false;
            ssid_offset = offset;
        } else if(frame[offset] == 3) {
            if(channel_offset != SIZE_MAX || size != 1) return false;
            channel_offset = offset;
        }
        offset += size + 2;
    }
    if(ssid_offset == SIZE_MAX || channel_offset == SIZE_MAX) return false;
    size_t name_size = 0;
    while(name_size < 32 && ssid[name_size]) ++name_size;
    size_t old_size = frame[ssid_offset + 1];
    size_t new_length = *length - old_size + name_size;
    if(new_length > capacity) return false;
    size_t tail = ssid_offset + 2 + old_size;
    memmove(frame + ssid_offset + 2 + name_size, frame + tail, *length - tail);
    memcpy(frame + ssid_offset + 2, ssid, name_size);
    frame[ssid_offset + 1] = (uint8_t)name_size;
    if(channel_offset > ssid_offset) channel_offset = channel_offset - old_size + name_size;
    frame[channel_offset + 2] = channel;
    *length = new_length;
    return true;
}
