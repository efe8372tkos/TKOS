#include "ata.h"

/*
 * TKOS - ATA PIO Disk Surucusu (Implementasyon)
 *
 * stage2.asm .sector_loop ile BIREBIR AYNI port sirasi ve bit
 * mantigi kullanilir; tek fark burada LBA ve sektor sayisi
 * parametrik ve donguyu tek bir sektorluk yardimci fonksiyon
 * (ata_read_one) etrafinda kuruyoruz.
 *
 * Port haritasi (Primary ATA Bus, Master disk):
 *   0x1F0  Data (16-bit read/write)
 *   0x1F2  Sector count
 *   0x1F3  LBA low  (bit 0-7)
 *   0x1F4  LBA mid  (bit 8-15)
 *   0x1F5  LBA high (bit 16-23)
 *   0x1F6  Drive/head select (ust 4 bit LBA + 0xE0 = LBA modu, master)
 *   0x1F7  Command (yazma) / Status (okuma)
 *
 * Status biti anlamlari (0x1F7 okurken):
 *   bit 7 (0x80) BSY  - is mesgul, cikana kadar bekle
 *   bit 3 (0x08) DRQ  - veri transferine hazir
 *   bit 0 (0x01) ERR  - hata olustu
 */

#define ATA_REG_DATA        0x1F0
#define ATA_REG_SECCOUNT0   0x1F2
#define ATA_REG_LBA0        0x1F3
#define ATA_REG_LBA1        0x1F4
#define ATA_REG_LBA2        0x1F5
#define ATA_REG_DRIVE_SEL   0x1F6
#define ATA_REG_COMMAND     0x1F7
#define ATA_REG_STATUS      0x1F7

#define ATA_CMD_READ_PIO    0x20
#define ATA_CMD_WRITE_PIO   0x30

#define ATA_SR_BSY          0x80
#define ATA_SR_DRQ          0x08
#define ATA_SR_ERR          0x01

/* Guvenlik siniri: sonsuz donguye girmeyelim, cok yuksek bir deger
 * yeterli (gercek donanimda/QEMU'da BSY/DRQ genelde mikrosaniyeler
 * icinde temizlenir). */
#define ATA_POLL_TIMEOUT    100000000UL

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

/* BSY biti dusene kadar bekle. Timeout'ta -1 doner. */
static int ata_wait_bsy_clear(void) {
    uint64_t spins = 0;
    while (inb(ATA_REG_STATUS) & ATA_SR_BSY) {
        if (++spins > ATA_POLL_TIMEOUT) return -1;
    }
    return 0;
}

/* DRQ biti gelene kadar bekle (veri transferine hazir). Timeout/ERR'de -1. */
static int ata_wait_drq_set(void) {
    uint64_t spins = 0;
    for (;;) {
        uint8_t status = inb(ATA_REG_STATUS);
        if (status & ATA_SR_ERR) return -1;
        if (status & ATA_SR_DRQ) return 0;
        if (++spins > ATA_POLL_TIMEOUT) return -1;
    }
}

/*
 * ata_setup_lba() - stage2.asm'deki port yazma sirasinin birebir
 * karsiligi: drive/head select, sector count, LBA low/mid/high.
 *
 * @drive: ATA_DRIVE_MASTER (0xE0 taban biti) veya ATA_DRIVE_SLAVE
 *         (0xF0 taban biti). Ust 4 bit LBA'nin 24-27. bitleriyle
 *         OR'lanir (0xE0 | ..., 0xF0 | ...).
 */
static void ata_setup_lba(uint8_t drive, uint32_t lba, uint8_t sector_cnt) {
    /* 0x1F6: (master=0xE0 | slave=0xF0) + LBA'nin 24-27. bitleri */
    uint8_t drive_base = (drive == ATA_DRIVE_SLAVE) ? 0xF0 : 0xE0;
    outb(ATA_REG_DRIVE_SEL, (uint8_t)(drive_base | ((lba >> 24) & 0x0F)));

    /* 0x1F2: sektor sayisi */
    outb(ATA_REG_SECCOUNT0, sector_cnt);

    /* 0x1F3-0x1F5: LBA low/mid/high */
    outb(ATA_REG_LBA0, (uint8_t)(lba & 0xFF));
    outb(ATA_REG_LBA1, (uint8_t)((lba >> 8) & 0xFF));
    outb(ATA_REG_LBA2, (uint8_t)((lba >> 16) & 0xFF));
}

int ata_read_sectors(uint8_t drive, uint32_t lba, uint8_t sector_cnt, void *buffer) {
    if (!buffer || sector_cnt == 0) return -1;

    uint16_t *buf16 = (uint16_t *)buffer;

    ata_setup_lba(drive, lba, sector_cnt);
    outb(ATA_REG_COMMAND, ATA_CMD_READ_PIO);

    for (uint16_t s = 0; s < sector_cnt; s++) {
        if (ata_wait_bsy_clear() != 0) return -1;
        if (ata_wait_drq_set()   != 0) return -1;

        /* 256 word (512 byte) oku - stage2.asm'deki 'rep insw' karsiligi */
        for (int i = 0; i < 256; i++) {
            buf16[s * 256 + i] = inw(ATA_REG_DATA);
        }
    }

    return 0;
}

int ata_write_sectors(uint8_t drive, uint32_t lba, uint8_t sector_cnt, const void *buffer) {
    if (!buffer || sector_cnt == 0) return -1;

    const uint16_t *buf16 = (const uint16_t *)buffer;

    ata_setup_lba(drive, lba, sector_cnt);
    outb(ATA_REG_COMMAND, ATA_CMD_WRITE_PIO);

    for (uint16_t s = 0; s < sector_cnt; s++) {
        if (ata_wait_bsy_clear() != 0) return -1;
        if (ata_wait_drq_set()   != 0) return -1;

        for (int i = 0; i < 256; i++) {
            outw(ATA_REG_DATA, buf16[s * 256 + i]);
        }
    }

    /* Yazma sonrasi cache flush (ATA komutu 0xE7) - guvenli olmasi icin.
     * Bazi QEMU/gercek donanim kombinasyonlarinda gerekmeyebilir ama
     * zarari yok. */
    outb(ATA_REG_COMMAND, 0xE7);
    ata_wait_bsy_clear();

    return 0;
}

int ata_drive_present(uint8_t drive) {
    /* Sektor 0'i gecici bir arabellege okumayi dene. Disk yoksa
     * ata_wait_bsy_clear/ata_wait_drq_set timeout'a dusup -1 doner;
     * bu, ATA_POLL_TIMEOUT kadar surer ama sonsuza dek asili kalmaz. */
    static uint8_t probe_buf[ATA_SECTOR_SIZE];
    return (ata_read_sectors(drive, 0, 1, probe_buf) == 0) ? 1 : 0;
}
