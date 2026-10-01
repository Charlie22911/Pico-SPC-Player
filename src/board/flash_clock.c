#include "flash_clock.h"

#include <stdint.h>

#include "hardware/structs/qmi.h"
#include "hardware/sync.h"
#include "pico/platform.h"

#if PICO_FLASH_SPI_CLKDIV < 1 || PICO_FLASH_SPI_CLKDIV > 256
#error "Flash clock divider must be in 1..256"
#endif
#if PICO_FLASH_SPI_RXDELAY < 0 || PICO_FLASH_SPI_RXDELAY > 7
#error "Flash RX delay must fit the QMI timing field"
#endif

bool __no_inline_not_in_flash_func(board_flash_clock_init)(void) {
    /* Preserve the ROM's tested read command/format, including whether EBh is
     * prefixed on every transfer. A 03h fallback cannot use fast QSPI clocks. */
    if ((qmi_hw->m[0].rcmd & QMI_M0_RCMD_PREFIX_BITS) == 0x03u &&
        RPD_SYS_CLOCK_KHZ * 1000u / PICO_FLASH_SPI_CLKDIV > 50000000u) {
        return false;
    }

    const uint32_t irq_state = save_and_disable_interrupts();
    const uint32_t original_csr = qmi_hw->direct_csr;
    if (original_csr & QMI_DIRECT_CSR_EN_BITS) {
        restore_interrupts(irq_state);
        return false;
    }
    /* Direct mode blocks new XIP transfers; BUSY waits for an existing transfer.
     * All instructions and data accessed until XIP is restored reside in SRAM. */
    qmi_hw->direct_csr = original_csr | QMI_DIRECT_CSR_EN_BITS;
    uint32_t remaining = 1000000u;
    while ((qmi_hw->direct_csr & QMI_DIRECT_CSR_BUSY_BITS) && --remaining != 0u) {
        __asm volatile("nop");
    }
    if (remaining != 0u) {
        qmi_hw->m[0].timing =
            (qmi_hw->m[0].timing & ~(QMI_M0_TIMING_CLKDIV_BITS | QMI_M0_TIMING_RXDELAY_BITS)) |
            ((PICO_FLASH_SPI_CLKDIV & 0xffu) << QMI_M0_TIMING_CLKDIV_LSB) |
            (PICO_FLASH_SPI_RXDELAY << QMI_M0_TIMING_RXDELAY_LSB);
    }
    qmi_hw->direct_csr = original_csr;
    __dsb();
    /* Latch the new divisor before the subsequent system clock increase. */
    (void)*(volatile uint32_t *)XIP_NOCACHE_NOALLOC_BASE;
    __dsb();
    __isb();
    restore_interrupts(irq_state);
    return remaining != 0u;
}
