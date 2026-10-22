
#include <stdint.h>
#include <flipper_format/flipper_format.h>
#include <furi.h>
#include <core/dangerous_defines.h>
#include <storage/storage.h>
#include <gui/icon_i.h>

#include "animation_manager.h"
#include "animation_storage.h"
#include <assets_dolphin_internal.h>
#include <assets_dolphin_blocking.h>
#include <momentum/momentum.h>
#include <toolbox/asset_file.h>

#define ANIMATION_META_FILE     "meta.txt"
#define BASE_ANIMATION_DIR      EXT_PATH("dolphin")
static char ANIMATION_DIR[23 + ASSET_PACKS_NAME_LEN + 1];
static char ANIMATION_MANIFEST_FILE[sizeof(ANIMATION_DIR) + 13];
#define TAG                     "AnimationStorage"

static void animation_storage_free_bubbles(BubbleAnimation* animation);
static void animation_storage_free_frames(BubbleAnimation* animation);
static void animation_storage_free_animation(BubbleAnimation** storage_animation);
static BubbleAnimation* animation_storage_load_animation(const char* name);

/* Momentum's manifest selection: packs can contain animations without Icons. */
static void animation_handler_select_manifest(void) {
    FuriString* anim_dir = furi_string_alloc();
    FuriString* manifest = furi_string_alloc();
    bool use_asset_pack = asset_file_valid_name(momentum_settings.asset_pack);
    if(use_asset_pack) {
        furi_string_printf(
            anim_dir, "%s/%s/Anims", ASSET_PACKS_PATH, momentum_settings.asset_pack);
        furi_string_printf(manifest, "%s/manifest.txt", furi_string_get_cstr(anim_dir));
        Storage* storage = furi_record_open(RECORD_STORAGE);
        if(storage_common_stat(storage, furi_string_get_cstr(manifest), NULL) != FSE_OK) {
            use_asset_pack = false;
        }
        furi_record_close(RECORD_STORAGE);
    }
    if(!use_asset_pack) {
        furi_string_set(anim_dir, BASE_ANIMATION_DIR);
        furi_string_printf(manifest, "%s/manifest.txt", furi_string_get_cstr(anim_dir));
    }
    strlcpy(ANIMATION_DIR, furi_string_get_cstr(anim_dir), sizeof(ANIMATION_DIR));
    strlcpy(
        ANIMATION_MANIFEST_FILE, furi_string_get_cstr(manifest), sizeof(ANIMATION_MANIFEST_FILE));
    FURI_LOG_I(TAG, "Animation manifest: %s", ANIMATION_MANIFEST_FILE);
    furi_string_free(manifest);
    furi_string_free(anim_dir);
}

static bool animation_storage_load_single_manifest_info(
    StorageAnimationManifestInfo* manifest_info,
    const char* name) {
    furi_assert(manifest_info);
    animation_handler_select_manifest();
    manifest_info->name = NULL;

    bool result = false;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* file = flipper_format_file_alloc(storage);
    flipper_format_set_strict_mode(file, true);
    FuriString* read_string;
    read_string = furi_string_alloc();

    do {
        uint32_t u32value;
        if(FSE_OK != storage_sd_status(storage)) break;
        if(!flipper_format_file_open_existing(file, ANIMATION_MANIFEST_FILE)) break;

        if(!flipper_format_read_header(file, read_string, &u32value)) break;
        if(furi_string_cmp_str(read_string, "Flipper Animation Manifest")) break;

        manifest_info->name = NULL;

        /* skip other animation names */
        flipper_format_set_strict_mode(file, false);
        while(flipper_format_read_string(file, "Name", read_string) &&
              furi_string_cmp_str(read_string, name))
            ;
        if(furi_string_cmp_str(read_string, name)) break;
        flipper_format_set_strict_mode(file, true);

        if(!asset_file_valid_name(furi_string_get_cstr(read_string))) break;
        manifest_info->name = strdup(furi_string_get_cstr(read_string));
        if(!manifest_info->name) break;

        if(!flipper_format_read_uint32(file, "Min butthurt", &u32value, 1)) break;
        manifest_info->min_butthurt = u32value;
        if(!flipper_format_read_uint32(file, "Max butthurt", &u32value, 1)) break;
        manifest_info->max_butthurt = u32value;
        if(!flipper_format_read_uint32(file, "Min level", &u32value, 1)) break;
        manifest_info->min_level = u32value;
        if(!flipper_format_read_uint32(file, "Max level", &u32value, 1)) break;
        manifest_info->max_level = u32value;
        if(!flipper_format_read_uint32(file, "Weight", &u32value, 1)) break;
        if(u32value > 1000000) break;
        manifest_info->weight = u32value;
        result = true;
    } while(0);

    if(!result && manifest_info->name) {
        free((void*)manifest_info->name);
        manifest_info->name = NULL;
    }
    furi_string_free(read_string);
    flipper_format_free(file);

    furi_record_close(RECORD_STORAGE);

    return result;
}

void animation_storage_fill_animation_list(StorageAnimationList_t* animation_list) {
    furi_assert(sizeof(StorageAnimationList_t) == sizeof(void*));
    furi_assert(!StorageAnimationList_size(*animation_list));
    animation_handler_select_manifest();

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* file = flipper_format_file_alloc(storage);
    /* Forbid skipping fields */
    flipper_format_set_strict_mode(file, true);
    FuriString* read_string;
    read_string = furi_string_alloc();

    do {
        uint32_t u32value;
        StorageAnimation* storage_animation = NULL;

        if(FSE_OK != storage_sd_status(storage)) break;
        if(!flipper_format_file_open_existing(file, ANIMATION_MANIFEST_FILE)) break;
        if(!flipper_format_read_header(file, read_string, &u32value)) break;
        if(furi_string_cmp_str(read_string, "Flipper Animation Manifest")) break;
        for(size_t entries = 0; entries < 512; ++entries) {
            storage_animation = malloc(sizeof(StorageAnimation));
            if(!storage_animation) break;
            storage_animation->external = true;
            storage_animation->animation = NULL;
            storage_animation->manifest_info.name = NULL;

            if(!flipper_format_read_string(file, "Name", read_string)) break;
            if(!asset_file_valid_name(furi_string_get_cstr(read_string))) break;
            storage_animation->manifest_info.name = strdup(furi_string_get_cstr(read_string));
            if(!storage_animation->manifest_info.name) break;

            if(!flipper_format_read_uint32(file, "Min butthurt", &u32value, 1)) break;
            storage_animation->manifest_info.min_butthurt = u32value;
            if(!flipper_format_read_uint32(file, "Max butthurt", &u32value, 1)) break;
            storage_animation->manifest_info.max_butthurt = u32value;
            if(!flipper_format_read_uint32(file, "Min level", &u32value, 1)) break;
            storage_animation->manifest_info.min_level = u32value;
            if(!flipper_format_read_uint32(file, "Max level", &u32value, 1)) break;
            storage_animation->manifest_info.max_level = u32value;
            if(!flipper_format_read_uint32(file, "Weight", &u32value, 1)) break;
            if(u32value > 1000000) break;
            storage_animation->manifest_info.weight = u32value;

            StorageAnimationList_push_back(*animation_list, storage_animation);
            storage_animation = NULL; /* List owns successful entries. */
        }

        if(storage_animation) animation_storage_free_storage_animation(&storage_animation);
    } while(0);

    furi_string_free(read_string);
    flipper_format_free(file);

    // add hard-coded animations
    for(size_t i = 0; i < dolphin_internal_size; ++i) {
        StorageAnimationList_push_back(*animation_list, (StorageAnimation*)&dolphin_internal[i]);
    }

    furi_record_close(RECORD_STORAGE);
}

StorageAnimation* animation_storage_find_animation(const char* name) {
    furi_assert(name);
    furi_assert(strlen(name));
    StorageAnimation* storage_animation = NULL;

    for(size_t i = 0; i < dolphin_blocking_size; ++i) {
        if(!strcmp(dolphin_blocking[i].manifest_info.name, name)) {
            storage_animation = (StorageAnimation*)&dolphin_blocking[i];
            break;
        }
    }

    if(!storage_animation) {
        for(size_t i = 0; i < dolphin_internal_size; ++i) {
            if(!strcmp(dolphin_internal[i].manifest_info.name, name)) {
                storage_animation = (StorageAnimation*)&dolphin_internal[i];
                break;
            }
        }
    }

    /* look through external animations */
    if(!storage_animation) {
        storage_animation = calloc(1, sizeof(StorageAnimation));
        if(!storage_animation) return NULL;
        storage_animation->external = true;

        bool result = false;
        result =
            animation_storage_load_single_manifest_info(&storage_animation->manifest_info, name);
        if(result) {
            storage_animation->animation = animation_storage_load_animation(name);
            result = !!storage_animation->animation;
        }
        if(!result) {
            animation_storage_free_storage_animation(&storage_animation);
        }
    }

    return storage_animation;
}

StorageAnimationManifestInfo* animation_storage_get_meta(StorageAnimation* storage_animation) {
    furi_assert(storage_animation);
    return &storage_animation->manifest_info;
}

const BubbleAnimation*
    animation_storage_get_bubble_animation(StorageAnimation* storage_animation) {
    furi_assert(storage_animation);
    animation_storage_cache_animation(storage_animation);
    return storage_animation->animation;
}

void animation_storage_cache_animation(StorageAnimation* storage_animation) {
    furi_assert(storage_animation);

    if(storage_animation->external) {
        if(!storage_animation->animation) {
            storage_animation->animation =
                animation_storage_load_animation(storage_animation->manifest_info.name);
        }
    }
}

static void animation_storage_free_animation(BubbleAnimation** animation) {
    furi_assert(animation);

    if(*animation) {
        animation_storage_free_bubbles(*animation);
        animation_storage_free_frames(*animation);
        if((*animation)->frame_order) {
            free((void*)(*animation)->frame_order);
        }
        free(*animation);
        *animation = NULL;
    }
}

void animation_storage_free_storage_animation(StorageAnimation** storage_animation) {
    furi_assert(storage_animation);
    furi_assert(*storage_animation);

    if((*storage_animation)->external) {
        animation_storage_free_animation((BubbleAnimation**)&(*storage_animation)->animation);

        if((*storage_animation)->manifest_info.name) {
            free((void*)(*storage_animation)->manifest_info.name);
        }
        free(*storage_animation);
    }

    *storage_animation = NULL;
}

static bool animation_storage_cast_align(FuriString* align_str, Align* align) {
    if(!furi_string_cmp(align_str, "Bottom")) {
        *align = AlignBottom;
    } else if(!furi_string_cmp(align_str, "Top")) {
        *align = AlignTop;
    } else if(!furi_string_cmp(align_str, "Left")) {
        *align = AlignLeft;
    } else if(!furi_string_cmp(align_str, "Right")) {
        *align = AlignRight;
    } else if(!furi_string_cmp(align_str, "Center")) {
        *align = AlignCenter;
    } else {
        return false;
    }

    return true;
}

static void animation_storage_free_frames(BubbleAnimation* animation) {
    furi_assert(animation);

    const Icon* icon = &animation->icon_animation;
    for(int i = 0; icon->frames && i < icon->frame_count; ++i) {
        if(icon->frames[i]) {
            free((void*)icon->frames[i]);
        }
    }

    free((void*)icon->frames);
    FURI_CONST_ASSIGN_PTR(animation->icon_animation.frames, NULL);
    FURI_CONST_ASSIGN(animation->icon_animation.frame_count, 0);
}

static bool animation_storage_load_frames(
    Storage* storage,
    const char* name,
    BubbleAnimation* animation,
    uint32_t* frame_order,
    uint32_t width,
    uint32_t height) {
    if(!asset_file_bitmap_size(width, height)) return false;
    uint16_t frame_order_count = animation->passive_frames + animation->active_frames;

    /* The frames should go in order (0...N), without omissions */
    size_t max_frame_count = 0;
    for(int i = 0; i < frame_order_count; ++i) {
        max_frame_count = MAX(max_frame_count, frame_order[i]);
    }

    if((max_frame_count >= frame_order_count) || (max_frame_count >= 256 /* max uint8_t */)) {
        return false;
    }

    Icon* icon = (Icon*)&animation->icon_animation;
    FURI_CONST_ASSIGN(icon->frame_count, max_frame_count + 1);
    FURI_CONST_ASSIGN(icon->frame_rate, 0);
    FURI_CONST_ASSIGN(icon->height, height);
    FURI_CONST_ASSIGN(icon->width, width);
    icon->frames = calloc(icon->frame_count, sizeof(const uint8_t*));
    if(!icon->frames) { FURI_CONST_ASSIGN(icon->frame_count, 0); return false; }

    bool frames_ok = false;
    File* file = storage_file_alloc(storage);
    FileInfo file_info = {0};
    FuriString* filename;
    filename = furi_string_alloc();
    size_t bitmap_size = asset_file_bitmap_size(width, height);
    uint8_t* encoded = malloc(ASSET_FILE_MAX_ENCODED);

    for(int i = 0; i < icon->frame_count; ++i) {
        frames_ok = false;
        furi_string_printf(filename, "%s/%s/frame_%d.bm", ANIMATION_DIR, name, i);

        if(storage_common_stat(storage, furi_string_get_cstr(filename), &file_info) != FSE_OK)
            break;
        if(!encoded || !file_info.size || file_info.size > ASSET_FILE_MAX_ENCODED ||
           (file_info.flags & FSF_DIRECTORY)) {
            FURI_LOG_E(
                TAG,
                "Filesize %llu, max: %zu (width %u, height %u)",
                file_info.size,
                (size_t)ASSET_FILE_MAX_ENCODED,
                (unsigned)width,
                (unsigned)height);
            break;
        }
        if(!storage_file_open(
               file, furi_string_get_cstr(filename), FSAM_READ, FSOM_OPEN_EXISTING)) {
            FURI_LOG_E(TAG, "Can't open file \'%s\'", furi_string_get_cstr(filename));
            break;
        }

        FURI_CONST_ASSIGN_PTR(icon->frames[i], malloc(bitmap_size + 1));
        if(!icon->frames[i] || storage_file_size(file) != file_info.size ||
           storage_file_read(file, encoded, file_info.size) != file_info.size ||
           !asset_file_decode_bitmap(encoded, file_info.size, width, height,
                                     (uint8_t*)icon->frames[i] + 1, bitmap_size)) {
            FURI_LOG_E(TAG, "Read failed: \'%s\'", furi_string_get_cstr(filename));
            break;
        }
        ((uint8_t*)icon->frames[i])[0] = 0;
        storage_file_close(file);
        frames_ok = true;
    }

    if(!frames_ok) {
        FURI_LOG_E(
            TAG,
            "Load \'%s\' failed, %ux%u, size: %llu",
            furi_string_get_cstr(filename),
            (unsigned)width,
            (unsigned)height,
            file_info.size);
        animation_storage_free_frames(animation);
    } else {
        furi_check(animation->icon_animation.frames);
        for(int i = 0; i < animation->icon_animation.frame_count; ++i) {
            furi_check(animation->icon_animation.frames[i]);
        }
    }

    storage_file_free(file);
    free(encoded);
    furi_string_free(filename);

    return frames_ok;
}

static bool animation_storage_load_bubbles(BubbleAnimation* animation, FlipperFormat* ff) {
    uint32_t u32value;
    FuriString* str;
    str = furi_string_alloc();
    bool success = false;
    furi_assert(!animation->frame_bubble_sequences);

    do {
        if(!flipper_format_read_uint32(ff, "Bubble slots", &u32value, 1)) break;
        if(u32value > 20) break;
        animation->frame_bubble_sequences_count = u32value;
        if(animation->frame_bubble_sequences_count == 0) {
            success = true;
            break;
        }
        animation->frame_bubble_sequences =
            calloc(animation->frame_bubble_sequences_count, sizeof(FrameBubble*));
        if(!animation->frame_bubble_sequences) break;

        int32_t current_slot = 0;
        for(int i = 0; i < animation->frame_bubble_sequences_count; ++i) {
            FURI_CONST_ASSIGN_PTR(
                animation->frame_bubble_sequences[i], calloc(1, sizeof(FrameBubble)));
            if(!animation->frame_bubble_sequences[i]) goto bubbles_done;
        }

        const FrameBubble* bubble = animation->frame_bubble_sequences[0];
        int8_t index = -1;
        size_t bubble_count = 0;
        for(;;) {
            if(!flipper_format_read_int32(ff, "Slot", &current_slot, 1)) break;
            if(++bubble_count > 128) goto bubbles_done;
            if((current_slot != 0) && (index == -1)) goto bubbles_done;

            if(current_slot == index) {
                FURI_CONST_ASSIGN_PTR(bubble->next_bubble, calloc(1, sizeof(FrameBubble)));
                if(!bubble->next_bubble) goto bubbles_done;
                bubble = bubble->next_bubble;
            } else if(current_slot == index + 1) {
                ++index;
                if(index >= animation->frame_bubble_sequences_count) goto bubbles_done;
                bubble = animation->frame_bubble_sequences[index];
            } else {
                /* slots have to start from 0, be ascending sorted, and
                 * have exact number of slots as specified in "Bubble slots" */
                goto bubbles_done;
            }
            if(index >= animation->frame_bubble_sequences_count) goto bubbles_done;

            if(!flipper_format_read_uint32(ff, "X", &u32value, 1)) goto bubbles_done;
            if(u32value >= 128) goto bubbles_done;
            FURI_CONST_ASSIGN(bubble->bubble.x, u32value);
            if(!flipper_format_read_uint32(ff, "Y", &u32value, 1)) goto bubbles_done;
            if(u32value >= 64) goto bubbles_done;
            FURI_CONST_ASSIGN(bubble->bubble.y, u32value);

            if(!flipper_format_read_string(ff, "Text", str)) goto bubbles_done;
            if(furi_string_size(str) > 100) goto bubbles_done;

            furi_string_replace_all(str, "\\n", "\n");

            FURI_CONST_ASSIGN_PTR(bubble->bubble.text, strdup(furi_string_get_cstr(str)));
            if(!bubble->bubble.text) goto bubbles_done;

            if(!flipper_format_read_string(ff, "AlignH", str)) goto bubbles_done;
            if(!animation_storage_cast_align(str, (Align*)&bubble->bubble.align_h)) goto bubbles_done;
            if(!flipper_format_read_string(ff, "AlignV", str)) goto bubbles_done;
            if(!animation_storage_cast_align(str, (Align*)&bubble->bubble.align_v)) goto bubbles_done;

            if(!flipper_format_read_uint32(ff, "StartFrame", &u32value, 1)) goto bubbles_done;
            if(u32value >= 255) goto bubbles_done;
            FURI_CONST_ASSIGN(bubble->start_frame, u32value);
            if(!flipper_format_read_uint32(ff, "EndFrame", &u32value, 1)) goto bubbles_done;
            if(u32value < bubble->start_frame || u32value >= 255) goto bubbles_done;
            FURI_CONST_ASSIGN(bubble->end_frame, u32value);
        }
        success = (index + 1) == animation->frame_bubble_sequences_count;
    } while(0);

bubbles_done:
    if(!success) {
        if(animation->frame_bubble_sequences) {
            FURI_LOG_E(TAG, "Failed to load animation bubbles");
            animation_storage_free_bubbles(animation);
        }
    }

    furi_string_free(str);
    return success;
}

static BubbleAnimation* animation_storage_load_animation(const char* name) {
    furi_assert(name);
    if(!asset_file_valid_name(name)) return NULL;
    animation_handler_select_manifest();
    BubbleAnimation* animation = calloc(1, sizeof(BubbleAnimation));
    if(!animation) return NULL;

    uint32_t height = 0;
    uint32_t width = 0;
    uint32_t* u32array = NULL;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_file_alloc(storage);
    /* Forbid skipping fields */
    flipper_format_set_strict_mode(ff, true);
    FuriString* str;
    str = furi_string_alloc();
    animation->frame_bubble_sequences = NULL;

    bool success = false;
    do {
        uint32_t u32value;

        if(FSE_OK != storage_sd_status(storage)) break;

        furi_string_printf(str, "%s/%s/" ANIMATION_META_FILE, ANIMATION_DIR, name);
        if(!flipper_format_file_open_existing(ff, furi_string_get_cstr(str))) break;
        if(!flipper_format_read_header(ff, str, &u32value)) break;
        if(furi_string_cmp_str(str, "Flipper Animation")) break;

        if(!flipper_format_read_uint32(ff, "Width", &width, 1)) break;
        if(!flipper_format_read_uint32(ff, "Height", &height, 1)) break;
        if(!asset_file_bitmap_size(width, height)) break;

        if(!flipper_format_read_uint32(ff, "Passive frames", &u32value, 1)) break;
        if(!u32value || u32value > 255) break;
        animation->passive_frames = u32value;
        if(!flipper_format_read_uint32(ff, "Active frames", &u32value, 1)) break;
        if(u32value > 255u - animation->passive_frames) break;
        animation->active_frames = u32value;

        uint8_t frames = animation->passive_frames + animation->active_frames;
        uint32_t count = 0;
        if(!flipper_format_get_value_count(ff, "Frames order", &count)) break;
        if(count != frames) {
            FURI_LOG_E(TAG, "Error loading animation: frames order");
            break;
        }
        u32array = malloc(sizeof(uint32_t) * frames);
        if(!u32array) break;
        if(!flipper_format_read_uint32(ff, "Frames order", u32array, frames)) break;
        animation->frame_order = malloc(sizeof(uint8_t) * frames);
        if(!animation->frame_order) break;
        for(int i = 0; i < frames; ++i) {
            FURI_CONST_ASSIGN(animation->frame_order[i], u32array[i]);
        }

        /* passive and active frames must be loaded up to this point */
        if(!animation_storage_load_frames(storage, name, animation, u32array, width, height))
            break;

        if(!flipper_format_read_uint32(ff, "Active cycles", &u32value, 1)) break; //-V779
        if(u32value > 255) break;
        animation->active_cycles = u32value;
        if(!flipper_format_read_uint32(ff, "Frame rate", &u32value, 1)) break;
        if(!u32value || u32value > 60) break;
        FURI_CONST_ASSIGN(animation->icon_animation.frame_rate, u32value);
        if(!flipper_format_read_uint32(ff, "Duration", &u32value, 1)) break;
        if(!u32value || u32value > UINT16_MAX) break;
        animation->duration = u32value;
        if(!flipper_format_read_uint32(ff, "Active cooldown", &u32value, 1)) break;
        if(u32value > UINT16_MAX) break;
        animation->active_cooldown = u32value;

        if(!animation_storage_load_bubbles(animation, ff)) break;
        success = true;
    } while(0);

    furi_string_free(str);
    flipper_format_free(ff);
    if(u32array) {
        free(u32array);
    }

    if(!success) { //-V547
        /* Includes frames loaded before a later metadata/bubble failure. */
        animation_storage_free_animation(&animation);
    }

    return animation;
}

static void animation_storage_free_bubbles(BubbleAnimation* animation) {
    if(!animation->frame_bubble_sequences) return;

    for(int i = 0; i < animation->frame_bubble_sequences_count;) {
        const FrameBubble* const* bubble = &animation->frame_bubble_sequences[i];

        if((*bubble) == NULL) { ++i; continue; }

        while((*bubble)->next_bubble != NULL) {
            bubble = &(*bubble)->next_bubble;
        }

        if((*bubble)->bubble.text) {
            free((void*)(*bubble)->bubble.text);
        }
        if((*bubble) == animation->frame_bubble_sequences[i]) {
            ++i;
        }
        free((void*)*bubble);
        FURI_CONST_ASSIGN_PTR(*bubble, NULL);
    }
    free((void*)animation->frame_bubble_sequences);
    animation->frame_bubble_sequences = NULL;
}
