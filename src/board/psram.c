#include "psram.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/structs/qmi.h"
#include "hardware/structs/xip_ctrl.h"
#include "hardware/sync.h"
#include "hardware/xip_cache.h"
#include "pico/platform.h"

#include "psram_validation.h"

#define PSRAM_CS_PIN 47u
#define PSRAM_WAIT_ITERATIONS 1000000u
#define PSRAM_CMD_QUAD_END 0xf5u
#define PSRAM_CMD_QUAD_ENABLE 0x35u
#define PSRAM_CMD_READ_ID 0x9fu
#define PSRAM_CMD_RESET_ENABLE 0x66u
#define PSRAM_CMD_RESET 0x99u
#define PSRAM_CMD_QUAD_READ 0xebu
#define PSRAM_CMD_QUAD_WRITE 0x38u
#define PSRAM_CMD_NOOP 0xffu
#define PSRAM_UNCACHED_BASE (XIP_NOCACHE_NOALLOC_BASE + 0x01000000u)

static bool __no_inline_not_in_flash_func(qmi_wait)(uint32_t mask, uint32_t expected) {
    for (uint32_t i = 0; i < PSRAM_WAIT_ITERATIONS; ++i) {
        if ((qmi_hw->direct_csr & mask) == expected)
            return true;
        __asm volatile("nop");
    }
    return false;
}

static size_t __no_inline_not_in_flash_func(psram_qmi_setup)(void) {
    gpio_set_function(PSRAM_CS_PIN, GPIO_FUNC_XIP_CS1);

    const int sys_hz = (int)clock_get_hz(clk_sys);
    if (sys_hz <= 0)
        return 0;
    const int divider = BOARD_PSRAM_CLKDIV;
    const int rx_delay = BOARD_PSRAM_RXDELAY;
    const int clock_period_fs = (int)(1000000000000000ll / sys_hz);
    /* MAX_SELECT uses 64 system-clock units. Leave room below the 3 us
       extended-grade limit for an in-progress cache fill to finish. */
    const int max_select = (2000 * 1000000ll) / (64 * clock_period_fs);
    int min_deselect = (50 * 1000000 + clock_period_fs - 1) / clock_period_fs - (divider + 1) / 2;
    if (min_deselect < 0)
        min_deselect = 0;

    uint8_t manufacturer = 0;
    uint8_t extended = 0;
    uint32_t irq_state = save_and_disable_interrupts();
    qmi_hw->direct_csr = (30u << QMI_DIRECT_CSR_CLKDIV_LSB) | QMI_DIRECT_CSR_EN_BITS;
    if (!qmi_wait(QMI_DIRECT_CSR_BUSY_BITS, 0))
        goto timeout;

    qmi_hw->direct_csr |= QMI_DIRECT_CSR_ASSERT_CS1N_BITS;
    qmi_hw->direct_tx = QMI_DIRECT_TX_OE_BITS |
                        (QMI_DIRECT_TX_IWIDTH_VALUE_Q << QMI_DIRECT_TX_IWIDTH_LSB) |
                        PSRAM_CMD_QUAD_END;
    if (!qmi_wait(QMI_DIRECT_CSR_TXEMPTY_BITS, QMI_DIRECT_CSR_TXEMPTY_BITS) ||
        !qmi_wait(QMI_DIRECT_CSR_BUSY_BITS, 0))
        goto timeout;
    (void)qmi_hw->direct_rx;
    qmi_hw->direct_csr &= ~QMI_DIRECT_CSR_ASSERT_CS1N_BITS;

    /* ID reads must follow reset; reset also restores the linear burst default. */
    const uint8_t reset_commands[] = {PSRAM_CMD_RESET_ENABLE, PSRAM_CMD_RESET};
    for (uint i = 0; i < sizeof(reset_commands); ++i) {
        for (uint j = 0; j < 20; ++j)
            __asm volatile("nop");
        qmi_hw->direct_csr |= QMI_DIRECT_CSR_ASSERT_CS1N_BITS;
        qmi_hw->direct_tx = reset_commands[i];
        if (!qmi_wait(QMI_DIRECT_CSR_TXEMPTY_BITS, QMI_DIRECT_CSR_TXEMPTY_BITS) ||
            !qmi_wait(QMI_DIRECT_CSR_BUSY_BITS, 0))
            goto timeout;
        qmi_hw->direct_csr &= ~QMI_DIRECT_CSR_ASSERT_CS1N_BITS;
        (void)qmi_hw->direct_rx;
    }
    for (uint j = 0; j < 20; ++j)
        __asm volatile("nop");

    qmi_hw->direct_csr |= QMI_DIRECT_CSR_ASSERT_CS1N_BITS;
    for (uint i = 0; i < 7; ++i) {
        qmi_hw->direct_tx = i == 0 ? PSRAM_CMD_READ_ID : PSRAM_CMD_NOOP;
        if (!qmi_wait(QMI_DIRECT_CSR_TXEMPTY_BITS, QMI_DIRECT_CSR_TXEMPTY_BITS) ||
            !qmi_wait(QMI_DIRECT_CSR_BUSY_BITS, 0))
            goto timeout;
        uint8_t value = (uint8_t)qmi_hw->direct_rx;
        if (i == 5)
            manufacturer = value;
        if (i == 6)
            extended = value;
    }
    qmi_hw->direct_csr &= ~(QMI_DIRECT_CSR_ASSERT_CS1N_BITS | QMI_DIRECT_CSR_EN_BITS);
    restore_interrupts(irq_state);

    size_t capacity = board_psram_capacity_from_id(manufacturer, extended);
    if (capacity == 0) {
        printf("PSRAM ID rejected: %02x %02x\n", manufacturer, extended);
        return 0;
    }

    irq_state = save_and_disable_interrupts();
    qmi_hw->direct_csr = (30u << QMI_DIRECT_CSR_CLKDIV_LSB) | QMI_DIRECT_CSR_EN_BITS;
    if (!qmi_wait(QMI_DIRECT_CSR_BUSY_BITS, 0))
        goto timeout;
    const uint8_t commands[] = {
        /* Reset selected linear mode; entering QPI leaves that mode intact. */
        PSRAM_CMD_QUAD_ENABLE,
    };
    for (uint i = 0; i < sizeof(commands); ++i) {
        qmi_hw->direct_csr |= QMI_DIRECT_CSR_ASSERT_CS1N_BITS;
        qmi_hw->direct_tx = commands[i];
        if (!qmi_wait(QMI_DIRECT_CSR_TXEMPTY_BITS, QMI_DIRECT_CSR_TXEMPTY_BITS) ||
            !qmi_wait(QMI_DIRECT_CSR_BUSY_BITS, 0))
            goto timeout;
        qmi_hw->direct_csr &= ~QMI_DIRECT_CSR_ASSERT_CS1N_BITS;
        for (uint j = 0; j < 20; ++j)
            __asm volatile("nop");
        (void)qmi_hw->direct_rx;
    }
    /* Join sequential transfers in linear mode, breaking at each 1 KiB page
       boundary or MAX_SELECT so the chip has time to refresh. */
    qmi_hw->m[1].timing = (QMI_M1_TIMING_PAGEBREAK_VALUE_1024 << QMI_M1_TIMING_PAGEBREAK_LSB) |
                          (BOARD_PSRAM_COOLDOWN << QMI_M1_TIMING_COOLDOWN_LSB) |
                          ((uint32_t)rx_delay << QMI_M1_TIMING_RXDELAY_LSB) |
                          ((uint32_t)max_select << QMI_M1_TIMING_MAX_SELECT_LSB) |
                          ((uint32_t)min_deselect << QMI_M1_TIMING_MIN_DESELECT_LSB) |
                          ((uint32_t)divider << QMI_M1_TIMING_CLKDIV_LSB);
    qmi_hw->m[1].rfmt = (QMI_M1_RFMT_PREFIX_WIDTH_VALUE_Q << QMI_M1_RFMT_PREFIX_WIDTH_LSB) |
                        (QMI_M1_RFMT_ADDR_WIDTH_VALUE_Q << QMI_M1_RFMT_ADDR_WIDTH_LSB) |
                        (QMI_M1_RFMT_SUFFIX_WIDTH_VALUE_Q << QMI_M1_RFMT_SUFFIX_WIDTH_LSB) |
                        (QMI_M1_RFMT_DUMMY_WIDTH_VALUE_Q << QMI_M1_RFMT_DUMMY_WIDTH_LSB) |
                        (QMI_M1_RFMT_DUMMY_LEN_VALUE_24 << QMI_M1_RFMT_DUMMY_LEN_LSB) |
                        (QMI_M1_RFMT_DATA_WIDTH_VALUE_Q << QMI_M1_RFMT_DATA_WIDTH_LSB) |
                        (QMI_M1_RFMT_PREFIX_LEN_VALUE_8 << QMI_M1_RFMT_PREFIX_LEN_LSB) |
                        (QMI_M1_RFMT_SUFFIX_LEN_VALUE_NONE << QMI_M1_RFMT_SUFFIX_LEN_LSB);
    qmi_hw->m[1].rcmd = PSRAM_CMD_QUAD_READ;
    qmi_hw->m[1].wfmt = (QMI_M1_WFMT_PREFIX_WIDTH_VALUE_Q << QMI_M1_WFMT_PREFIX_WIDTH_LSB) |
                        (QMI_M1_WFMT_ADDR_WIDTH_VALUE_Q << QMI_M1_WFMT_ADDR_WIDTH_LSB) |
                        (QMI_M1_WFMT_SUFFIX_WIDTH_VALUE_Q << QMI_M1_WFMT_SUFFIX_WIDTH_LSB) |
                        (QMI_M1_WFMT_DUMMY_WIDTH_VALUE_Q << QMI_M1_WFMT_DUMMY_WIDTH_LSB) |
                        (QMI_M1_WFMT_DUMMY_LEN_VALUE_NONE << QMI_M1_WFMT_DUMMY_LEN_LSB) |
                        (QMI_M1_WFMT_DATA_WIDTH_VALUE_Q << QMI_M1_WFMT_DATA_WIDTH_LSB) |
                        (QMI_M1_WFMT_PREFIX_LEN_VALUE_8 << QMI_M1_WFMT_PREFIX_LEN_LSB) |
                        (QMI_M1_WFMT_SUFFIX_LEN_VALUE_NONE << QMI_M1_WFMT_SUFFIX_LEN_LSB);
    qmi_hw->m[1].wcmd = PSRAM_CMD_QUAD_WRITE;
    xip_ctrl_hw->ctrl |= XIP_CTRL_WRITABLE_M1_BITS;
    __dsb();
    qmi_hw->direct_csr &= ~(QMI_DIRECT_CSR_ASSERT_CS1N_BITS | QMI_DIRECT_CSR_EN_BITS);
    restore_interrupts(irq_state);
    printf("PSRAM ID %02x %02x, usable %u bytes\n", manufacturer, extended, (unsigned)capacity);
    return capacity;

timeout:
    qmi_hw->direct_csr &= ~(QMI_DIRECT_CSR_ASSERT_CS1N_BITS | QMI_DIRECT_CSR_EN_BITS);
    restore_interrupts(irq_state);
    printf("PSRAM QMI timeout\n");
    return 0;
}

static bool __no_inline_not_in_flash_func(psram_check_linear_block)(size_t offset) {
    uint32_t cached[16];
    const uint32_t irq_state = save_and_disable_interrupts();
    xip_cache_invalidate_range(0x01000000u + offset, sizeof(cached));
    /* SRAM execution prevents flash instruction fetches from interrupting
       these adjacent cache fills, allowing QMI to join the linear reads. */
    const volatile uint32_t *source = (const volatile uint32_t *)((uintptr_t)BOARD_PSRAM_BASE + offset);
    for (size_t i = 0u; i < 16u; ++i)
        cached[i] = source[i];
    bool valid = true;
    const volatile uint32_t *uncached = (const volatile uint32_t *)((uintptr_t)PSRAM_UNCACHED_BASE + offset);
    for (size_t i = 0u; i < 16u; ++i) {
        if (cached[i] != uncached[i])
            valid = false;
    }
    restore_interrupts(irq_state);
    return valid;
}

static bool psram_read(void *context, size_t offset, uint32_t *value) {
    (void)context;
    if ((offset & 63u) == 0u && !psram_check_linear_block(offset))
        return false;
    const uintptr_t line = offset & ~(XIP_CACHE_LINE_SIZE - 1u);
    xip_cache_invalidate_range(0x01000000u + line, XIP_CACHE_LINE_SIZE);
    *value = *(volatile uint32_t *)((uintptr_t)PSRAM_UNCACHED_BASE + offset);
    /* A cold cached read also exercises the QMI's full 8-byte cache fill. */
    return *value == *(volatile uint32_t *)((uintptr_t)BOARD_PSRAM_BASE + offset);
}

static bool psram_write(void *context, size_t offset, uint32_t value) {
    (void)context;
    *(volatile uint32_t *)((uintptr_t)PSRAM_UNCACHED_BASE + offset) = value;
    __dsb();
    return true;
}

size_t board_psram_init(void) {
    const size_t capacity = psram_qmi_setup();
    const uint32_t timing = qmi_hw->m[1].timing;
    const uint32_t divider = (timing & QMI_M1_TIMING_CLKDIV_BITS) >> QMI_M1_TIMING_CLKDIV_LSB;
    if (capacity != 0u &&
        (divider != BOARD_PSRAM_CLKDIV ||
         ((timing & QMI_M1_TIMING_RXDELAY_BITS) >> QMI_M1_TIMING_RXDELAY_LSB) != BOARD_PSRAM_RXDELAY ||
         ((timing & QMI_M1_TIMING_COOLDOWN_BITS) >> QMI_M1_TIMING_COOLDOWN_LSB) != BOARD_PSRAM_COOLDOWN ||
         ((timing & QMI_M1_TIMING_PAGEBREAK_BITS) >> QMI_M1_TIMING_PAGEBREAK_LSB) != QMI_M1_TIMING_PAGEBREAK_VALUE_1024)) {
        printf("PSRAM clock/timing check: FAIL (requested divisor %u)\n", BOARD_PSRAM_CLKDIV);
        return 0u;
    }
    const board_psram_access_t access = {NULL, psram_read, psram_write};
    const bool valid = capacity != 0u && board_psram_validate_memory(&access, capacity);
    /* Discard probe data cached before the original words were restored. */
    xip_cache_invalidate_all();
    if (!valid) {
        printf("PSRAM validation failed\n");
        return 0;
    }
    printf("PSRAM memory check: PASS (3 x 64-byte probes, uncached + cold cache + sequential reads)\n");
    return capacity;
}
