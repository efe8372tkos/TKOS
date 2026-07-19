#pragma once
#include "types.h"

/*
 * TKOS - ATA PIO Disk Surucusu (Header)
 *
 * stage2.asm'deki 28-bit LBA ATA PIO okuma mantigindan
 * (bkz. .sector_loop, port 0x1F0-0x1F7) C tarafina uyarlanmistir.
 *
 * Su an PRIMARY BUS (port 0x1F0-0x1F7) uzerindeki iki disk
 * destekleniyor:
 *   ATA_DRIVE_MASTER (0) - QEMU/Limbo'da genelde ilk -hda/tkos.img
 *   ATA_DRIVE_SLAVE  (1) - ikinci bagli disk (orn. Limbo'da 2. HDD)
 *
 * Secondary bus (port 0x170-0x177) su an desteklenmiyor - ihtiyac
 * olursa ayni desende ATA_DRIVE_* sabitleri genisletilebilir.
 *
 * Bu katman "ham sektor oku/yaz" saglar; FAT16 surucusu (fat16.c)
 * bunun uzerine kurulur.
 */

#define ATA_SECTOR_SIZE    512

#define ATA_DRIVE_MASTER   0
#define ATA_DRIVE_SLAVE    1

/*
 * ata_read_sectors()
 * @drive      : ATA_DRIVE_MASTER veya ATA_DRIVE_SLAVE
 * @lba        : Baslangic LBA (28-bit, yani lba < 0x0FFFFFFF olmali)
 * @sector_cnt : Okunacak sektor sayisi (1-255)
 * @buffer     : sector_cnt * 512 byte yer olan hedef arabellek
 *
 * Donus: 0 basari, -1 hata (timeout/ATA hata biti/disk yok).
 */
int ata_read_sectors(uint8_t drive, uint32_t lba, uint8_t sector_cnt, void *buffer);

/*
 * ata_write_sectors()
 * Ayni parametreler, yazma yonunde.
 */
int ata_write_sectors(uint8_t drive, uint32_t lba, uint8_t sector_cnt, const void *buffer);

/*
 * ata_drive_present() - Belirtilen surucude gercekten bir disk olup
 * olmadigini kontrol eder (sektor 0'i okumayi deneyerek). Ikinci disk
 * bagli degilse fat16_init() bunun yerine timeout'a dusup uzun
 * sure beklemesin diye once bu hizli kontrol yapilabilir.
 *
 * Donus: 1 disk var, 0 disk yok/erisilemiyor.
 */
int ata_drive_present(uint8_t drive);

