#include "icon_animation_i.h"
#include "icon_i.h" // IWYU pragma: keep

#include <furi.h>

const Icon* asset_packs_swap_icon(const Icon* requested);

IconAnimation* icon_animation_alloc(const Icon* icon) {
    furi_check(icon);
    icon = asset_packs_swap_icon(icon);

    IconAnimation* instance = malloc(sizeof(IconAnimation));
    instance->icon = icon;
    instance->timer =
        furi_timer_alloc(icon_animation_timer_callback, FuriTimerTypePeriodic, instance);

    return instance;
}

void icon_animation_free(IconAnimation* instance) {
    furi_check(instance);

    icon_animation_stop(instance);
    furi_timer_free(instance->timer);

    free(instance);
}

void icon_animation_set_update_callback(
    IconAnimation* instance,
    IconAnimationCallback callback,
    void* context) {
    furi_check(instance);

    instance->callback = callback;
    instance->callback_context = context;
}

const uint8_t* icon_animation_get_data(const IconAnimation* instance) {
    return instance->icon->frames[instance->frame];
}

void icon_animation_next_frame(IconAnimation* instance) {
    furi_assert(instance);
    instance->frame = (instance->frame + 1) % instance->icon->frame_count;
}

void icon_animation_timer_callback(void* context) {
    furi_assert(context);

    IconAnimation* instance = context;

    if(!instance->animating) return;

    icon_animation_next_frame(instance);
    if(instance->callback) {
        instance->callback(instance, instance->callback_context);
    }
}

uint8_t icon_animation_get_width(const IconAnimation* instance) {
    furi_check(instance);

    return instance->icon->width;
}

uint8_t icon_animation_get_height(const IconAnimation* instance) {
    furi_check(instance);

    return instance->icon->height;
}

void icon_animation_start(IconAnimation* instance) {
    furi_check(instance);

    if(!instance->animating) {
        /* FAP menu icons and static pack replacements have no animation clock. */
        if(instance->icon->frame_count <= 1 || instance->icon->frame_rate == 0) return;
        instance->animating = true;
        furi_check(furi_timer_start(
                       instance->timer,
                       MAX(1u, furi_kernel_get_tick_frequency() / instance->icon->frame_rate)) ==
                   FuriStatusOk);
    }
}

void icon_animation_stop(IconAnimation* instance) {
    furi_check(instance);

    if(instance->animating) {
        instance->animating = false;
        furi_timer_stop(instance->timer);
        instance->frame = 0;
    }
}

bool icon_animation_is_last_frame(const IconAnimation* instance) {
    furi_check(instance);

    return instance->icon->frame_count - instance->frame <= 1;
}
