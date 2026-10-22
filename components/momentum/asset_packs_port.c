#include <momentum/asset_packs_i.h>
#include <momentum/settings.h>

#include <assets_icon_paths.h>
#include <gui/icon_i.h>
#include <storage/storage.h>
#include <toolbox/asset_file.h>

#define TAG "AssetPacks"
#define ICONS_FMT ASSET_PACKS_PATH "/%s/Icons/%s"
#define FONTS_FMT ASSET_PACKS_PATH "/%s/Fonts/%s.u8f"
#define U8G2_FONT_DATA_STRUCT_SIZE 23
#define ASSET_PACK_MEMORY_BUDGET (512u * 1024u)

AssetPacks* asset_packs = NULL;

typedef struct {
    Icon icon;
    uint8_t* frames[];
} AnimatedIconSwap;

typedef struct {
    int32_t width;
    int32_t height;
    int32_t frame_rate;
    int32_t frame_count;
} __attribute__((packed)) AnimatedIconMetaFile;

typedef struct {
    int32_t width;
    int32_t height;
} __attribute__((packed)) StaticIconBmxHeader;

static void free_icon(const Icon* icon) {
    AnimatedIconSwap* swap = (void*)icon;
    for(size_t i = 0; i < swap->icon.frame_count; ++i) free(swap->frames[i]);
    free(swap);
}

/* Decode once, with real file bounds; the GUI only sees validated raw frames.
 * Fit replacements to their original icon box without rewriting the SD pack. */
static uint8_t* load_frame(File* file, size_t size, uint32_t width, uint32_t height,
                           uint16_t out_width, uint16_t out_height) {
    if(!size || size > ASSET_FILE_MAX_ENCODED) return NULL;
    size_t bitmap_size = asset_file_bitmap_size(width, height);
    size_t out_size = asset_file_bitmap_size(out_width, out_height);
    if(!bitmap_size || !out_size) return NULL;
    uint8_t* scratch = malloc(size + bitmap_size);
    uint8_t* frame = malloc(out_size + 1);
    bool ok = scratch && frame && storage_file_read(file, scratch, size) == size &&
              asset_file_decode_bitmap(scratch, size, width, height, scratch + size, bitmap_size) &&
              asset_file_scale_bitmap(scratch + size, width, height, frame + 1, out_width, out_height);
    free(scratch);
    if(!ok) { free(frame); return NULL; }
    frame[0] = 0;
    return frame;
}

static void load_icon_animated(
    AssetPacks* pack,
    const Icon* original,
    const char* name,
    FuriString* path,
    File* file) {
    furi_string_printf(path, ICONS_FMT "/meta", momentum_settings.asset_pack, name);
    if(!storage_file_open(file, furi_string_get_cstr(path), FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_close(file);
        return;
    }

    AnimatedIconMetaFile meta;
    bool ok = storage_file_size(file) == sizeof(meta) &&
              storage_file_read(file, &meta, sizeof(meta)) == sizeof(meta);
    storage_file_close(file);
    if(!ok || !asset_file_bitmap_size(meta.width, meta.height) || meta.frame_count <= 0 ||
       meta.frame_count > 64 || meta.frame_rate <= 0 || meta.frame_rate > 60) {
        return;
    }

    uint16_t width, height;
    asset_file_fit_dimensions(meta.width, meta.height, original->width, original->height, &width, &height);
    size_t memory = sizeof(AnimatedIconSwap) + sizeof(uint8_t*) * meta.frame_count +
                    (asset_file_bitmap_size(width, height) + 1) * meta.frame_count;
    if(memory > ASSET_PACK_MEMORY_BUDGET - pack->memory_used) return;

    AnimatedIconSwap* swap =
        calloc(1, sizeof(AnimatedIconSwap) + sizeof(uint8_t*) * meta.frame_count);
    if(!swap) return;
    *((uint8_t*)&swap->icon.frame_count) = (uint8_t)meta.frame_count;
    int32_t loaded = 0;
    for(; loaded < meta.frame_count; ++loaded) {
        furi_string_printf(
            path, ICONS_FMT "/frame_%02ld.bm", momentum_settings.asset_pack, name, loaded);
        if(!storage_file_open(file, furi_string_get_cstr(path), FSAM_READ, FSOM_OPEN_EXISTING)) {
            storage_file_close(file);
            break;
        }
        uint64_t frame_size = storage_file_size(file);
        if(frame_size <= ASSET_FILE_MAX_ENCODED)
            swap->frames[loaded] = load_frame(file, (size_t)frame_size, meta.width, meta.height, width, height);
        ok = swap->frames[loaded] != NULL;
        storage_file_close(file);
        if(!ok) break;
    }

    if(loaded == meta.frame_count) {
        *((uint16_t*)&swap->icon.width) = width;
        *((uint16_t*)&swap->icon.height) = height;
        *((uint8_t*)&swap->icon.frame_count) = (uint8_t)meta.frame_count;
        *((uint8_t*)&swap->icon.frame_rate) = (uint8_t)meta.frame_rate;
        *((const uint8_t* const**)&swap->icon.frames) =
            (const uint8_t* const*)swap->frames;
        IconSwapList_push_back(
            pack->icons, (IconSwap){.original = original, .replaced = &swap->icon});
        pack->memory_used += memory;
    } else {
        free_icon(&swap->icon);
    }
}

static void load_icon_static(
    AssetPacks* pack,
    const Icon* original,
    const char* name,
    FuriString* path,
    File* file) {
    furi_string_printf(path, ICONS_FMT ".bmx", momentum_settings.asset_pack, name);
    if(!storage_file_open(file, furi_string_get_cstr(path), FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_close(file);
        return;
    }

    uint64_t file_size = storage_file_size(file);
    if(file_size <= sizeof(StaticIconBmxHeader) ||
       file_size > sizeof(StaticIconBmxHeader) + ASSET_FILE_MAX_ENCODED) {
        storage_file_close(file);
        return;
    }

    StaticIconBmxHeader header;
    size_t frame_size = (size_t)(file_size - sizeof(header));
    bool ok = storage_file_read(file, &header, sizeof(header)) == sizeof(header) &&
              asset_file_bitmap_size(header.width, header.height);
    uint16_t width = 0, height = 0;
    if(ok) asset_file_fit_dimensions(header.width, header.height, original->width, original->height, &width, &height);
    size_t memory = sizeof(AnimatedIconSwap) + sizeof(uint8_t*) +
                    asset_file_bitmap_size(width, height) + 1;
    AnimatedIconSwap* swap = ok && memory <= ASSET_PACK_MEMORY_BUDGET - pack->memory_used ?
                                 calloc(1, sizeof(AnimatedIconSwap) + sizeof(uint8_t*)) : NULL;
    if(swap) {
        *((uint8_t*)&swap->icon.frame_count) = 1;
        swap->frames[0] = load_frame(file, frame_size, header.width, header.height, width, height);
    }
    ok = swap && swap->frames[0];
    storage_file_close(file);

    if(ok) {
        *((uint16_t*)&swap->icon.width) = width;
        *((uint16_t*)&swap->icon.height) = height;
        *((uint8_t*)&swap->icon.frame_count) = 1;
        *((uint8_t*)&swap->icon.frame_rate) = 0;
        *((const uint8_t* const**)&swap->icon.frames) =
            (const uint8_t* const*)swap->frames;
        IconSwapList_push_back(
            pack->icons, (IconSwap){.original = original, .replaced = &swap->icon});
        pack->memory_used += memory;
    } else {
        if(swap) free_icon(&swap->icon);
    }
}

static const char* const font_names[FontTotalNumber] = {
    [FontPrimary] = "Primary",
    [FontSecondary] = "Secondary",
    [FontKeyboard] = "Keyboard",
    [FontBigNumbers] = "BigNumbers",
    [FontBatteryPercent] = "BatteryPercent",
};

static void load_font(AssetPacks* pack, Font font, FuriString* path, File* file) {
    furi_string_printf(
        path, FONTS_FMT, momentum_settings.asset_pack, font_names[font]);
    if(!storage_file_open(file, furi_string_get_cstr(path), FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_close(file);
        return;
    }

    uint64_t file_size = storage_file_size(file);
    size_t size = file_size <= ASSET_FILE_MAX_FONT ? (size_t)file_size : 0;
    uint8_t* replacement = size > U8G2_FONT_DATA_STRUCT_SIZE &&
                           size + sizeof(CanvasFontParameters) <= ASSET_PACK_MEMORY_BUDGET - pack->memory_used ?
                               malloc(size) : NULL;
    bool ok = replacement && storage_file_read(file, replacement, size) == size &&
              asset_file_validate_font(replacement, size);
    storage_file_close(file);
    if(!ok) {
        free(replacement);
        return;
    }

    CanvasFontParameters* params = malloc(sizeof(CanvasFontParameters));
    if(!params) {
        free(replacement);
        return;
    }
    params->leading_default = replacement[10];
    params->leading_min = params->leading_default > 2 ? params->leading_default - 2 : 1;
    params->height = MAX((int8_t)replacement[15], 0);
    params->descender = MAX((int8_t)replacement[16], 0);
    pack->fonts[font] = replacement;
    pack->font_params[font] = params;
    pack->memory_used += size + sizeof(CanvasFontParameters);
}

void asset_packs_init(void) {
    if(asset_packs || !asset_file_valid_name(momentum_settings.asset_pack)) return;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FuriString* path = furi_string_alloc_printf(
        ASSET_PACKS_PATH "/%s", momentum_settings.asset_pack);
    FileInfo info;
    if(storage_common_stat(storage, furi_string_get_cstr(path), &info) == FSE_OK &&
       (info.flags & FSF_DIRECTORY)) {
        AssetPacks* pack = calloc(1, sizeof(AssetPacks));
        if(!pack) {
            furi_string_free(path);
            furi_record_close(RECORD_STORAGE);
            return;
        }
        IconSwapList_init(pack->icons);
        File* file = storage_file_alloc(storage);
        for(size_t i = 0; i < ASSET_ICON_PATHS_COUNT; ++i) {
            const AssetIconPath* entry = &ASSET_ICON_PATHS[i];
            /* Alias fallback rows must not replace an already loaded primary
               icon or allocate duplicate replacements for the same original. */
            bool loaded = false;
            for M_EACH(swap, pack->icons, IconSwapList_t) {
                if(swap->original == entry->icon) { loaded = true; break; }
            }
            if(loaded) continue;
            if(entry->icon->frame_count > 1) {
                load_icon_animated(pack, entry->icon, entry->path, path, file);
                for M_EACH(swap, pack->icons, IconSwapList_t) {
                    if(swap->original == entry->icon) { loaded = true; break; }
                }
            }
            if(!loaded) load_icon_static(pack, entry->icon, entry->path, path, file);
        }
        for(Font font = 0; font < FontTotalNumber; ++font) {
            load_font(pack, font, path, file);
        }
        storage_file_free(file);
        /* Publish only the complete immutable set. Never mutate it under GUI readers. */
        asset_packs = pack;
        FURI_LOG_I(
            TAG,
            "Loaded '%s' (%u icons)",
            momentum_settings.asset_pack,
            (unsigned)IconSwapList_size(asset_packs->icons));
    }
    furi_string_free(path);
    furi_record_close(RECORD_STORAGE);
}

void asset_packs_free(void) {
    if(!asset_packs) return;
    for M_EACH(icon_swap, asset_packs->icons, IconSwapList_t) {
        free_icon(icon_swap->replaced);
    }
    IconSwapList_clear(asset_packs->icons);
    for(Font font = 0; font < FontTotalNumber; ++font) {
        free(asset_packs->fonts[font]);
        free(asset_packs->font_params[font]);
    }
    free(asset_packs);
    asset_packs = NULL;
}

const Icon* asset_packs_swap_icon(const Icon* requested) {
    if(!asset_packs || !requested) return requested;
    for M_EACH(icon_swap, asset_packs->icons, IconSwapList_t) {
        if(icon_swap->original == requested) return icon_swap->replaced;
    }
    return requested;
}

const uint8_t* asset_packs_get_font(Font font) {
    return asset_packs && (unsigned)font < FontTotalNumber ? asset_packs->fonts[font] : NULL;
}

const CanvasFontParameters* asset_packs_get_font_params(Font font) {
    return asset_packs && (unsigned)font < FontTotalNumber ? asset_packs->font_params[font] : NULL;
}
