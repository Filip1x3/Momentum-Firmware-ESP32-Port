#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* External graphics use the logical Flipper canvas, not the 320x170 LCD. */
#define ASSET_FILE_MAX_WIDTH 128u
#define ASSET_FILE_MAX_HEIGHT 64u
#define ASSET_FILE_MAX_BITMAP 1024u
#define ASSET_FILE_MAX_ENCODED 1156u /* Heatshrink literal worst case + header. */
#define ASSET_FILE_MAX_FONT 65536u

size_t asset_file_bitmap_size(uint32_t width, uint32_t height);
bool asset_file_decode_bitmap(
    const uint8_t* data, size_t size, uint32_t width, uint32_t height,
    uint8_t* output, size_t capacity);
void asset_file_fit_dimensions(
    uint32_t width, uint32_t height, uint32_t max_width, uint32_t max_height,
    uint16_t* result_width, uint16_t* result_height);
bool asset_file_scale_bitmap(
    const uint8_t* source, uint32_t width, uint32_t height,
    uint8_t* output, uint32_t out_width, uint32_t out_height);
/* Validate the tables AND glyph bit streams before exposing a font to u8g2. */
bool asset_file_validate_font(const uint8_t* data, size_t size);
bool asset_file_valid_name(const char* name);
