#include "board.h"

#include <stdio.h>

#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "hardware/structs/qmi.h"
#include "hardware/uart.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"

#include "AMOLED_2in41.h"
#include "DEV_Config.h"
#include "FT6336U.h"
#include "clock_validation.h"
#include "flash_clock.h"
#include "psram.h"
#include "qspi_pio.h"

static volatile board_status_t status;
static board_clock_readings_t clocks;

board_result_t board_init(void) {
    const bool flash_clock_ready = board_flash_clock_init();
    bool module_ready = false;
    if (flash_clock_ready) {
        module_ready = DEV_Module_Init() == 0;
    } else {
        /* Keep the ROM's safe clocks if its flash protocol cannot be accelerated. */
        stdio_init_all();
        sleep_ms(100);
        printf("Flash timing setup failed or ROM selected a slow 03h read fallback\n");
    }
    status.sys_clock_hz = clock_get_hz(clk_sys);
    status.peri_clock_hz = clock_get_hz(clk_peri);
    clocks = (board_clock_readings_t){
        .configured_sys_hz = status.sys_clock_hz,
        .configured_peri_hz = status.peri_clock_hz,
        .measured_sys_khz = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_SYS),
        .measured_peri_khz = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_PERI),
        .flash_divider_encoded = (uint8_t)((qmi_hw->m[0].timing & QMI_M0_TIMING_CLKDIV_BITS) >>
                                           QMI_M0_TIMING_CLKDIV_LSB),
    };
    status.clock_ready = flash_clock_ready && module_ready &&
                         board_clocks_match(&clocks, RPD_SYS_CLOCK_KHZ * 1000u,
                                            PICO_FLASH_SPI_CLKDIV);
    if (!status.clock_ready) {
        status.last_error = BOARD_ERROR_CLOCK;
        return status.last_error;
    }

    status.psram_capacity_bytes = board_psram_init();
    status.psram_ready = status.psram_capacity_bytes != 0;
    status.last_error = status.psram_ready ? BOARD_OK : BOARD_ERROR_PSRAM;
    return status.last_error;
}

void board_print_diagnostics(void) {
    const uint32_t flash_timing = qmi_hw->m[0].timing;
    const uint32_t flash_divider = board_qmi_clock_divider(
        (uint8_t)((flash_timing & QMI_M0_TIMING_CLKDIV_BITS) >> QMI_M0_TIMING_CLKDIV_LSB));
    const uint32_t psram_timing = qmi_hw->m[1].timing;
    const uint32_t psram_divider = board_qmi_clock_divider(
        (uint8_t)((psram_timing & QMI_M1_TIMING_CLKDIV_BITS) >> QMI_M1_TIMING_CLKDIV_LSB));
    const uart_hw_t *uart = uart_get_hw(PICO_DEFAULT_UART_INSTANCE());
    const uint32_t baud_divisor = 64u * uart->ibrd + uart->fbrd;
    const uint32_t baud = baud_divisor != 0u
                              ? (uint32_t)((uint64_t)clocks.measured_peri_khz * 4000u / baud_divisor)
                              : 0u;

    printf("\nPico-SPC-Player %s | Arm UART/clock diagnostics\n", PICO_SPC_BUILD_VERSION);
    printf("UART%u TX=GP%u RX=GP%u: requested=%u actual=%lu baud (IBRD=%lu FBRD=%lu)\n",
           PICO_DEFAULT_UART, PICO_DEFAULT_UART_TX_PIN, PICO_DEFAULT_UART_RX_PIN,
           PICO_DEFAULT_UART_BAUD_RATE, (unsigned long)baud,
           (unsigned long)uart->ibrd, (unsigned long)uart->fbrd);
    printf("CLK requested sys/peri=%u kHz; SDK sys=%lu peri=%lu Hz\n", RPD_SYS_CLOCK_KHZ,
           (unsigned long)status.sys_clock_hz, (unsigned long)status.peri_clock_hz);
    printf("CLK measured against clk_ref=%lu Hz: sys=%lu peri=%lu kHz\n",
           (unsigned long)clock_get_hz(clk_ref), (unsigned long)clocks.measured_sys_khz,
           (unsigned long)clocks.measured_peri_khz);
    printf("FLASH M0_TIMING=0x%08lx requested_div=%u actual_div=%lu RXDELAY=%lu SCK=%lu Hz\n",
           (unsigned long)flash_timing, PICO_FLASH_SPI_CLKDIV, (unsigned long)flash_divider,
           (unsigned long)((flash_timing & QMI_M0_TIMING_RXDELAY_BITS) >> QMI_M0_TIMING_RXDELAY_LSB),
           (unsigned long)((uint64_t)clocks.measured_sys_khz * 1000u / flash_divider));
    printf("FLASH ROM read protocol retained: RFMT=0x%08lx RCMD=0x%08lx; requested_RXDELAY=%u\n",
           (unsigned long)qmi_hw->m[0].rfmt, (unsigned long)qmi_hw->m[0].rcmd,
           PICO_FLASH_SPI_RXDELAY);
    printf("PSRAM M1_TIMING=0x%08lx requested_div=%u actual_div=%lu RXDELAY=%lu SCK=%lu Hz; validated=%u bytes\n",
           (unsigned long)psram_timing, BOARD_PSRAM_CLKDIV, (unsigned long)psram_divider,
           (unsigned long)((psram_timing & QMI_M1_TIMING_RXDELAY_BITS) >> QMI_M1_TIMING_RXDELAY_LSB),
           (unsigned long)((uint64_t)clocks.measured_sys_khz * 1000u / psram_divider),
           (unsigned)status.psram_capacity_bytes);
    printf("PSRAM CHECK: %s (%u Hz target, linear, joined transfers, 1 KiB page breaks; uncached + cold cache + sequential probes; optional)\n",
           status.psram_ready ? "PASS" : "FAIL/unavailable", RPD_SYS_CLOCK_KHZ * 1000u / BOARD_PSRAM_CLKDIV);
    printf("CLOCK CHECK: %s (sys/peri measured tolerance 0.1%%; flash divider exact)\n\n",
           status.clock_ready ? "PASS" : "FAIL");
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
    /* Keep the two-instruction QSPI loop's divider fixed as clk_sys changes. */
    const uint32_t sys_hz = clock_get_hz(clk_sys);
    const uint32_t display_divider = 1u;
    pio_sm_set_clkdiv_int_frac8(g_qspi.pio, g_qspi.sm_4wire,
                              display_divider, 0u);
    printf("DISPLAY QSPI requested_SCK=%lu actual_SCK=%lu Hz (PIO divider=%lu)\n",
           (unsigned long)(RPD_SYS_CLOCK_KHZ * 1000u / (2u * display_divider)),
           (unsigned long)(sys_hz / (2u * display_divider)),
           (unsigned long)display_divider);
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
