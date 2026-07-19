#pragma once
#include "types.h"



















#define ATA_SECTOR_SIZE    512

#define ATA_DRIVE_MASTER   0
#define ATA_DRIVE_SLAVE    1










int ata_read_sectors(uint8_t drive, uint32_t lba, uint8_t sector_cnt, void *buffer);





int ata_write_sectors(uint8_t drive, uint32_t lba, uint8_t sector_cnt, const void *buffer);









int ata_drive_present(uint8_t drive);

