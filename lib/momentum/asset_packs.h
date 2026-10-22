#pragma once

#include <gui/canvas.h>
#include <stdint.h>

#define ASSET_PACKS_PATH EXT_PATH("asset_packs")

void asset_packs_init(void);
/* Only after all GUI/animation readers have stopped. Changing packs at runtime
 * is applied by saving settings and rebooting; it must not call this function. */
void asset_packs_free(void);
