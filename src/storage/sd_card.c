#include "sd_card.h"

#include <stddef.h>

#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"

#define SD_SPI spi0
#define SD_PIN_SCK 18u
#define SD_PIN_MOSI 19u
#define SD_PIN_MISO 20u
#define SD_PIN_DAT1 21u
#define SD_PIN_DAT2 22u
#define SD_PIN_CS 23u
#define SD_INIT_TIMEOUT_MS 1500u
#define SD_READY_TIMEOUT_MS 100u
#define SD_DATA_TIMEOUT_MS 250u

static bool card_present;
static bool card_ready;
static bool block_addressing;

static uint8_t transfer(uint8_t value) {
    uint8_t result = 0xffu;
    (void)spi_write_read_blocking(SD_SPI, &value, &result, 1u);
    return result;
}

static void deselect(void) {
    gpio_put(SD_PIN_CS, 1);
    (void)transfer(0xffu);
}

static bool select_card(void) {
    gpio_put(SD_PIN_CS, 0);
    const absolute_time_t deadline = make_timeout_time_ms(SD_READY_TIMEOUT_MS);
    do {
        if (transfer(0xffu) == 0xffu)
            return true;
    } while (!time_reached(deadline));
    deselect();
    return false;
}

static uint8_t send_command_selected(uint8_t command, uint32_t argument, uint8_t crc) {
    (void)transfer((uint8_t)(0x40u | command));
    (void)transfer((uint8_t)(argument >> 24u));
    (void)transfer((uint8_t)(argument >> 16u));
    (void)transfer((uint8_t)(argument >> 8u));
    (void)transfer((uint8_t)argument);
    (void)transfer(crc);
    if (command == 12u)
        (void)transfer(0xffu);
    for (uint32_t i = 0u; i < 10u; ++i) {
        const uint8_t response = transfer(0xffu);
        if ((response & 0x80u) == 0u)
            return response;
    }
    return 0xffu;
}

static uint8_t command(uint8_t number, uint32_t argument, uint8_t crc, uint8_t *extra,
                       size_t extra_size) {
    if (!select_card())
        return 0xffu;
    const uint8_t response = send_command_selected(number, argument, crc);
    for (size_t i = 0u; i < extra_size; ++i)
        extra[i] = transfer(0xffu);
    deselect();
    return response;
}

sd_card_status_t sd_card_init(void) {
    card_present = false;
    card_ready = false;
    block_addressing = false;

    (void)spi_init(SD_SPI, 400000u);
    gpio_set_function(SD_PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(SD_PIN_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(SD_PIN_MISO, GPIO_FUNC_SPI);
    gpio_pull_up(SD_PIN_MISO);
    gpio_init(SD_PIN_DAT1);
    gpio_init(SD_PIN_DAT2);
    gpio_pull_up(SD_PIN_DAT1);
    gpio_pull_up(SD_PIN_DAT2);
    gpio_init(SD_PIN_CS);
    gpio_set_dir(SD_PIN_CS, GPIO_OUT);
    gpio_put(SD_PIN_CS, 1);
    for (uint32_t i = 0u; i < 10u; ++i)
        (void)transfer(0xffu);

    uint8_t idle = 0xffu;
    bool responded = false;
    for (uint32_t attempt = 0u; attempt < 4u && idle != 0x01u; ++attempt) {
        idle = command(0u, 0u, 0x95u, NULL, 0u);
        responded = responded || idle != 0xffu;
    }
    card_present = responded;
    if (idle != 0x01u) {
        return responded ? SD_CARD_INIT_ERROR : SD_CARD_ABSENT;
    }

    uint8_t r7[4] = {0};
    const uint8_t cmd8 = command(8u, 0x1aau, 0x87u, r7, sizeof(r7));
    const bool version2 = cmd8 == 0x01u && r7[2] == 0x01u && r7[3] == 0xaau;
    if (!version2 && (cmd8 & 0x04u) == 0u)
        return SD_CARD_INIT_ERROR;

    const absolute_time_t deadline = make_timeout_time_ms(SD_INIT_TIMEOUT_MS);
    uint8_t response = 0xffu;
    do {
        const uint8_t app = command(55u, 0u, 0x01u, NULL, 0u);
        if (app > 0x01u)
            return SD_CARD_INIT_ERROR;
        response = command(41u, version2 ? 0x40000000u : 0u, 0x01u, NULL, 0u);
        if (response == 0u)
            break;
        sleep_ms(5u);
    } while (!time_reached(deadline));
    if (response != 0u)
        return SD_CARD_INIT_ERROR;

    uint8_t ocr[4] = {0};
    if (command(58u, 0u, 0x01u, ocr, sizeof(ocr)) != 0u) {
        return SD_CARD_INIT_ERROR;
    }
    block_addressing = version2 && (ocr[0] & 0x40u) != 0u;
    if (!block_addressing && command(16u, 512u, 0x01u, NULL, 0u) != 0u) {
        return SD_CARD_INIT_ERROR;
    }
    (void)spi_set_baudrate(SD_SPI, 12000000u);
    card_ready = true;
    return SD_CARD_READY;
}

bool sd_card_present(void) {
    return card_present;
}
bool sd_card_ready(void) {
    return card_ready;
}

static bool read_failure(void) {
    card_ready = false;
    return false;
}

bool sd_card_read_blocks(uint32_t first_lba, uint8_t *destination, uint32_t block_count) {
    if (!card_ready || destination == NULL || block_count == 0u)
        return false;
    for (uint32_t block = 0u; block < block_count; ++block) {
        const uint32_t lba = first_lba + block;
        if (!select_card())
            return read_failure();
        const uint32_t argument = block_addressing ? lba : lba * 512u;
        if (send_command_selected(17u, argument, 0x01u) != 0u) {
            deselect();
            return read_failure();
        }
        const absolute_time_t deadline = make_timeout_time_ms(SD_DATA_TIMEOUT_MS);
        uint8_t token = 0xffu;
        do {
            token = transfer(0xffu);
            if (token == 0xfeu)
                break;
        } while (!time_reached(deadline));
        if (token != 0xfeu) {
            deselect();
            return read_failure();
        }
        (void)spi_read_blocking(SD_SPI, 0xffu, destination + block * 512u, 512u);
        (void)transfer(0xffu);
        (void)transfer(0xffu);
        deselect();
    }
    return true;
}
