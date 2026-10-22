#include "asset_file.h"

#include <heatshrink_decoder.h>
#include <string.h>

size_t asset_file_bitmap_size(uint32_t width, uint32_t height) {
    if(!width || width > ASSET_FILE_MAX_WIDTH || !height || height > ASSET_FILE_MAX_HEIGHT)
        return 0;
    return ((width + 7u) / 8u) * height;
}

bool asset_file_decode_bitmap(
    const uint8_t* data, size_t size, uint32_t width, uint32_t height,
    uint8_t* output, size_t capacity) {
    size_t expected = asset_file_bitmap_size(width, height);
    if(!data || !output || !expected || capacity < expected || !size ||
       size > ASSET_FILE_MAX_ENCODED) return false;
    if(data[0] == 0) {
        if(size != expected + 1u) return false;
        memcpy(output, data + 1, expected);
        return true;
    }
    if(data[0] != 1 || size < 4 || data[1] != 0) return false;
    size_t encoded = (size_t)data[2] | ((size_t)data[3] << 8);
    if(!encoded || encoded != size - 4u) return false;

    heatshrink_decoder* decoder = heatshrink_decoder_alloc(256, 8, 4);
    if(!decoder) return false;
    heatshrink_decoder_reset(decoder);
    size_t consumed = 4, produced = 0;
    bool ok = true;
    /* One extra byte detects an overlong expansion without writing past output. */
    uint8_t chunk[64];
    for(size_t iterations = 0; ok && iterations < 8192; ++iterations) {
        size_t sunk = 0, polled = 0;
        if(consumed < size) {
            if(heatshrink_decoder_sink(
                   decoder, (uint8_t*)data + consumed, size - consumed, &sunk) < 0) {
                ok = false;
                break;
            }
            consumed += sunk;
        }
        HSD_poll_res result = heatshrink_decoder_poll(decoder, chunk, sizeof(chunk), &polled);
        if(result < 0 || polled > expected - produced) {
            ok = false;
            break;
        }
        memcpy(output + produced, chunk, polled);
        produced += polled;
        if(consumed == size && result == HSDR_POLL_EMPTY &&
           heatshrink_decoder_finish(decoder) == HSDR_FINISH_DONE) {
            ok = produced == expected;
            heatshrink_decoder_free(decoder);
            return ok;
        }
        if(!sunk && !polled && result == HSDR_POLL_EMPTY) break;
    }
    heatshrink_decoder_free(decoder);
    return false;
}

void asset_file_fit_dimensions(
    uint32_t width, uint32_t height, uint32_t max_width, uint32_t max_height,
    uint16_t* result_width, uint16_t* result_height) {
    *result_width = *result_height = 0;
    if(!asset_file_bitmap_size(width, height) || !max_width || !max_height) return;
    if(width > max_width || height > max_height) {
        if(max_width * height <= max_height * width) {
            height = height * max_width / width;
            width = max_width;
        } else {
            width = width * max_height / height;
            height = max_height;
        }
    }
    *result_width = (uint16_t)(width ? width : 1);
    *result_height = (uint16_t)(height ? height : 1);
}

bool asset_file_scale_bitmap(
    const uint8_t* source, uint32_t width, uint32_t height,
    uint8_t* output, uint32_t out_width, uint32_t out_height) {
    size_t size = asset_file_bitmap_size(out_width, out_height);
    if(!source || !output || !size || !asset_file_bitmap_size(width, height)) return false;
    memset(output, 0, size);
    for(uint32_t y = 0; y < out_height; ++y) {
        for(uint32_t x = 0; x < out_width; ++x) {
            uint32_t sx = x * width / out_width, sy = y * height / out_height;
            if(source[sy * ((width + 7) / 8) + sx / 8] & (1u << (sx % 8)))
                output[y * ((out_width + 7) / 8) + x / 8] |= 1u << (x % 8);
        }
    }
    return true;
}

bool asset_file_valid_name(const char* name) {
    if(!name || !name[0] || !strcmp(name, ".") || !strcmp(name, "..")) return false;
    for(size_t i = 0; name[i]; ++i) {
        if(i >= 63 || (uint8_t)name[i] < 32 || name[i] == '/' || name[i] == '\\' ||
           name[i] == ':') return false;
    }
    return true;
}

static uint16_t font_word(const uint8_t* p) {
    return ((uint16_t)p[0] << 8) | p[1];
}

static bool font_bits(
    const uint8_t* data, size_t size, size_t* position, uint8_t count, uint8_t* value) {
    if(!count || count > 8 || *position + count > size * 8) return false;
    *value = 0;
    for(uint8_t i = 0; i < count; ++i, ++*position)
        *value |= ((data[*position / 8] >> (*position % 8)) & 1u) << i;
    return true;
}

static bool font_glyph(const uint8_t* font, const uint8_t* glyph, size_t size) {
    size_t bit = 0;
    uint8_t width, height, unused, a, b, repeat;
    if(!font_bits(glyph, size, &bit, font[4], &width) ||
       !font_bits(glyph, size, &bit, font[5], &height)) return false;
    for(unsigned i = 6; i <= 8; ++i)
        if(!font_bits(glyph, size, &bit, font[i], &unused)) return false;
    if(width > ASSET_FILE_MAX_WIDTH || height > ASSET_FILE_MAX_HEIGHT) return false;
    if(!width || !height) return true; /* space/empty glyph */
    size_t pixels = 0, expected = (size_t)width * height;
    while(pixels < expected) {
        if(!font_bits(glyph, size, &bit, font[2], &a) ||
           !font_bits(glyph, size, &bit, font[3], &b) || (!a && !b)) return false;
        do {
            if((size_t)a + b > expected - pixels) return false;
            pixels += (size_t)a + b;
            if(!font_bits(glyph, size, &bit, 1, &repeat)) return false;
        } while(repeat);
    }
    return true;
}

bool asset_file_validate_font(const uint8_t* data, size_t size) {
    if(!data || size < 25 || size > ASSET_FILE_MAX_FONT || data[1] > 3) return false;
    for(unsigned i = 2; i <= 8; ++i)
        if(!data[i] || data[i] > 8) return false;
    if(!data[9] || data[9] > 128 || !data[10] || data[10] > 64) return false;
    /* Every ASCII jump must land on a validated record, not inside a glyph. */
    size_t upper = 23u + font_word(data + 17), lower = 23u + font_word(data + 19);
    size_t unicode = 23u + font_word(data + 21), pos = 23;
    bool saw_upper = false, saw_lower = false;
    while(pos + 2 <= size) {
        saw_upper |= pos == upper;
        saw_lower |= pos == lower;
        size_t length = data[pos + 1];
        if(!length) break;
        if(length < 3 || length > size - pos ||
           !font_glyph(data, data + pos + 2, length - 2)) return false;
        pos += length;
    }
    if(pos + 2 > size || !saw_upper || !saw_lower || unicode < pos + 2 || unicode > size)
        return false;
    if(unicode == size) return true; /* ASCII-only font without a unicode table. */

    /* u8g2 follows these additive offsets until the last (0xffff) range. */
    size_t table = unicode, start = unicode;
    uint16_t previous = 0;
    bool end = false;
    while(table + 4 <= size) {
        size_t delta = font_word(data + table);
        uint16_t last = font_word(data + table + 2);
        if(!delta || delta > size - start || last < previous) return false;
        start += delta;
        table += 4;
        previous = last;
        if(last == 0xffff) { end = true; break; }
    }
    if(!end || start < table || start + 2 > size) return false;
    /* Validate the complete glyph chain, then check every lookup landing. */
    pos = unicode + font_word(data + unicode);
    if(pos < table) return false;
    size_t lookup = unicode, landing = pos;
    while(pos + 2 <= size) {
        if(pos == landing && lookup < table) {
            lookup += 4;
            if(lookup < table) landing += font_word(data + lookup);
        }
        if(!font_word(data + pos)) return lookup == table;
        if(pos + 3 > size) return false;
        size_t length = data[pos + 2];
        if(length < 4 || length > size - pos ||
           !font_glyph(data, data + pos + 3, length - 3)) return false;
        pos += length;
    }
    return false;
}
