#include <furi.h>
#include <icon_animation_i.h>
#include <icon_i.h>
#include <compress.h>
#include <stdio.h>

struct FuriTimer { unsigned starts; uint32_t period; };
FuriTimer* furi_timer_alloc(FuriTimerCallback cb, FuriTimerType type, void* context) {
    (void)cb; (void)type; (void)context; return malloc(sizeof(FuriTimer));
}
void furi_timer_free(FuriTimer* timer) { free(timer); }
FuriStatus furi_timer_start(FuriTimer* timer, uint32_t ticks) {
    assert(ticks); timer->starts++; timer->period = ticks; return FuriStatusOk;
}
FuriStatus furi_timer_stop(FuriTimer* timer) { (void)timer; return FuriStatusOk; }
uint32_t furi_kernel_get_tick_frequency(void) { return 100; }
const Icon* asset_packs_swap_icon(const Icon* icon) { return icon; }

int main(void) {
    const uint8_t raw[] = {0, 0xAA, 0x55};
    const uint8_t* frames[] = {raw, raw};
    const Icon still = {.width=8, .height=2, .frame_count=1, .frame_rate=0, .frames=frames};
    IconAnimation* animation = icon_animation_alloc(&still);
    for(unsigned i = 0; i < 1000; ++i) {
        icon_animation_start(animation);
        icon_animation_stop(animation);
    }
    assert(animation->timer->starts == 0);
    assert(icon_animation_get_data(animation) == raw);
    icon_animation_free(animation);
    const Icon animated = {.width=8, .height=2, .frame_count=2, .frame_rate=255, .frames=frames};
    animation = icon_animation_alloc(&animated);
    icon_animation_start(animation);
    assert(animation->timer->starts == 1 && animation->timer->period == 1);
    icon_animation_next_frame(animation);
    assert(animation->frame == 1);
    icon_animation_stop(animation);
    assert(animation->frame == 0);
    icon_animation_free(animation);

    Compress* decoder = compress_alloc(CompressTypeHeatshrink, &compress_config_heatshrink_default);
    uint8_t output[] = {0xCC, 0xCC, 0xCC, 0xCC};
    size_t written = 99;
    assert(compress_decode(decoder, (uint8_t*)raw, sizeof(raw), output + 1, 2, &written));
    assert(written == 2 && output[0] == 0xCC && output[3] == 0xCC);
    assert(output[1] == 0xAA && output[2] == 0x55);
    assert(!compress_decode(decoder, (uint8_t*)raw, 0, output + 1, 2, &written));
    assert(written == 0);
    uint8_t truncated[] = {1, 0, 255, 255};
    assert(!compress_decode(decoder, truncated, sizeof(truncated), output + 1, 2, &written));
    assert(output[0] == 0xCC && output[3] == 0xCC);
    compress_free(decoder);
    puts("PASS: 1000 static-icon start/stop cycles, animated timer bounds, exact-size raw decompression and truncated header");
    return 0;
}
