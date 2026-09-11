#include "board.h"

#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "pico/multicore.h"

#include "AMOLED_2in41.h"
#include "DEV_Config.h"
#include "FT6336U.h"
#include "psram.h"
#include "qspi_pio.h"

static volatile board_status_t status;

board_result_t board_init(void) {
    if (DEV_Module_Init() != 0) {
        status.last_error = BOARD_ERROR_CLOCK;
        return status.last_error;
    }
    status.sys_clock_hz = clock_get_hz(clk_sys);
    status.peri_clock_hz = clock_get_hz(clk_peri);
    status.clock_ready = status.sys_clock_hz == RPD_SYS_CLOCK_KHZ * 1000u &&
                         status.peri_clock_hz == status.sys_clock_hz;
    if (!status.clock_ready) {
        status.last_error = BOARD_ERROR_CLOCK;
        return status.last_error;
    }

    status.psram_capacity_bytes = board_psram_init();
    status.psram_ready = status.psram_capacity_bytes != 0;
    status.last_error = status.psram_ready ? BOARD_OK : BOARD_ERROR_PSRAM;
    return status.last_error;
}

board_result_t board_ui_init(void) {
    if (get_core_num() != 1u)
        return BOARD_ERROR_WRONG_CORE;
    if (pio_sm_is_claimed(g_qspi.pio, g_qspi.sm_4wire) ||
        !pio_can_add_program(g_qspi.pio, &qspi_4wire_data_program)) {
        status.last_error = BOARD_ERROR_RESOURCE;
        return status.last_error;
    }
    pio_sm_claim(g_qspi.pio, g_qspi.sm_4wire);
    QSPI_GPIO_Init(g_qspi);
    QSPI_PIO_Init(g_qspi);
    QSPI_4Wrie_Mode(&g_qspi);
    AMOLED_2IN41_Init();
    AMOLED_2IN41_SetBrightness(60u);
    status.display_ready = true;
    status.touch_ready = FT6336U_Init(FT6336U_Point_Mode);
    status.last_error = status.touch_ready ? BOARD_OK : BOARD_ERROR_NOT_INITIALIZED;
    return status.last_error;
}

board_status_t board_get_status(void) {
    board_status_t copy;
    copy.sys_clock_hz = status.sys_clock_hz;
    copy.peri_clock_hz = status.peri_clock_hz;
    copy.psram_capacity_bytes = status.psram_capacity_bytes;
    copy.clock_ready = status.clock_ready;
    copy.psram_ready = status.psram_ready;
    copy.display_ready = status.display_ready;
    copy.touch_ready = status.touch_ready;
    copy.last_error = status.last_error;
    return copy;
}

bool board_touch_read(uint16_t *x, uint16_t *y, bool *pressed) {
    if (x == NULL || y == NULL || pressed == NULL || !status.touch_ready || get_core_num() != 1u ||
        !FT6336U_Get_Point()) {
        return false;
    }
    *pressed = FT6336U.touch_num != 0u;
    *x = FT6336U.touch1_x;
    *y = FT6336U.touch1_y;
    return true;
}
