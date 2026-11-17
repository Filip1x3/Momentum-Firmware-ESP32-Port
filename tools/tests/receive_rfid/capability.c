#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "../../../components/furi_hal/boards/board_lilygo_t_embed_cc1101.h"
#include "production_capability.h"
#ifndef EXPECT_EXTERNAL
#define EXPECT_EXTERNAL 0
#endif
int main(void) {
    assert(furi_hal_rfid_is_supported() == (EXPECT_EXTERNAL != 0));
    assert(BOARD_PIN_RFID_RX == BOARD_PIN_NRF24_CSN);
    assert(BOARD_PIN_RFID_TX == BOARD_PIN_NRF24_CE);
    puts(EXPECT_EXTERNAL ? "RFID explicit external build capability PASS" :
                           "RFID stock board reports no 125 kHz reader PASS");
    return 0;
}
