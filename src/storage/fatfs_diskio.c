#include "ff.h"
#include "diskio.h"

#include <stddef.h>

#include "sd_card.h"

DSTATUS disk_initialize(BYTE pdrv) {
    if (pdrv != 0u)
        return STA_NOINIT;
    return sd_card_init() == SD_CARD_READY ? 0u : STA_NOINIT;
}

DSTATUS disk_status(BYTE pdrv) {
    if (pdrv != 0u)
        return STA_NOINIT;
    return sd_card_ready() ? 0u : STA_NOINIT;
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count) {
    if (pdrv != 0u || buff == NULL || count == 0u)
        return RES_PARERR;
    return sd_card_read_blocks((uint32_t)sector, buff, (uint32_t)count) ? RES_OK : RES_ERROR;
}

#if FF_FS_READONLY == 0
DRESULT disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count) {
    (void)pdrv;
    (void)buff;
    (void)sector;
    (void)count;
    return RES_WRPRT;
}
#endif

DRESULT disk_ioctl(BYTE pdrv, BYTE command, void *buff) {
    if (pdrv != 0u)
        return RES_PARERR;
    if (!sd_card_ready())
        return RES_NOTRDY;
    switch (command) {
    case CTRL_SYNC:
        return RES_OK;
    case GET_SECTOR_SIZE:
        if (buff == NULL)
            return RES_PARERR;
        *(WORD *)buff = 512u;
        return RES_OK;
    case GET_BLOCK_SIZE:
        if (buff == NULL)
            return RES_PARERR;
        *(DWORD *)buff = 1u;
        return RES_OK;
    default:
        return RES_PARERR;
    }
}
