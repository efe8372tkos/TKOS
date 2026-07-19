#pragma once
#include "types.h"
#include "ata.h"

/*
 * TKOS - FAT16 Dosya Sistemi Surucusu (Header)
 *
 * Diskin FAT16'nin basladigi LBA'dan itibaren standart Microsoft
 * FAT16 formatini okur. Bu, host tarafinda (Termux/Linux)
 * 'mkfs.fat -F 16 --offset=...' ile olusturulan bolumle BIREBIR
 * uyumludur - yani zArchiver, Windows Gezgini, Linux dosya
 * yoneticileri de ayni veriyi okuyup yazabilir.
 *
 * COKLU DISK DESTEGI:
 * Her disk (ATA_DRIVE_MASTER / ATA_DRIVE_SLAVE) kendi bagimsiz
 * fat16_volume_t durumuna sahiptir. Boylece ayni oturumda hem
 * birincil hem ikincil diskteki FAT16 bolumlerini es zamanli
 * mount edip kullanabilirsin (bkz. kernel_main.c: ls/cat ->
 * ATA_DRIVE_MASTER, ls2/cat2 -> ATA_DRIVE_SLAVE).
 *
 * Su an SADECE OKUMA destekleniyor:
 *   - Root dizini listeleme
 *   - Dosya icerigini okuma (cluster zincirini takip ederek)
 *
 * Yazma (dosya olusturma/silme) ayri bir asamada eklenecek.
 */

/* FAT16 bolumunun disk uzerindeki baslangic LBA'si.
 * tkos.img duzeni: sektor 0-2047 bootloader+kernel icin ayrilmis,
 * FAT16 sektor 2048'den (1 MiB) itibaren basliyor.
 * Bu deger host tarafindaki 'mkfs.fat --offset=2048' ile
 * BIREBIR eslesmelidir - biri degisirse digeri de guncellenmeli.
 *
 * NOT: ikinci disk (ATA_DRIVE_SLAVE) icin de ayni offset varsayimi
 * kullaniliyor - yani 2. diski hazirlarken de FAT16'yi sektor
 * 2048'den baslatarak formatlaman gerekir (mkfs.fat ... --offset=2048).
 * Farkli bir ikinci disk duzeni kullanmak istersen fat16_init()'e
 * ayri bir 'partition_lba' parametresi eklemek gerekir. */
#define FAT16_PARTITION_LBA   2048

#define FAT16_MAX_NAME        13   /* "DOSYAADI.UZN" + '\0' (8.3 format) */

/* Dizin girisi oznitelik bitleri (DIR_Attr alani) */
#define FAT_ATTR_READ_ONLY  0x01
#define FAT_ATTR_HIDDEN     0x02
#define FAT_ATTR_SYSTEM     0x04
#define FAT_ATTR_VOLUME_ID  0x08
#define FAT_ATTR_DIRECTORY  0x10
#define FAT_ATTR_ARCHIVE    0x20
#define FAT_ATTR_LFN        0x0F  /* READ_ONLY|HIDDEN|SYSTEM|VOLUME_ID birlikte = uzun isim girisi */

/*
 * fat16_dirent_t - kullanicidan/shell'den erisim icin sadelestirilmis
 * dizin girisi. Ham disk formatindan (fat_raw_dirent_t, .c dosyasinda
 * static/internal) okunup buraya cevrilir.
 */
typedef struct {
    char     name[FAT16_MAX_NAME];  /* "DOSYA.TXT" seklinde, normalize edilmis */
    uint8_t  attr;                  /* FAT_ATTR_* bitleri */
    uint32_t size;                  /* Byte cinsinden dosya boyutu */
    uint16_t first_cluster;         /* Ilk cluster numarasi */
} fat16_dirent_t;

/*
 * fat16_volume_t - bir FAT16 bolumunun mount edilmis durumu.
 * Her disk (master/slave) icin ayri bir instance tutulur.
 * Alanlar fat16.c icinde doldurulur; disaridan sadece opak
 * bir pointer olarak tasinir (kernel_main.c ic yapisini bilmek
 * zorunda degil).
 */
typedef struct {
    uint8_t  drive;              /* ATA_DRIVE_MASTER / ATA_DRIVE_SLAVE */
    int      mounted;            /* 1: basariyla mount edildi */

    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint8_t  num_fats;
    uint16_t root_entry_count;
    uint16_t sectors_per_fat;

    uint32_t fat_start_lba;
    uint32_t root_start_lba;
    uint32_t root_sector_count;
    uint32_t data_start_lba;

    /* YAZMA DESTEGI icin: toplam sektor/cluster sayisi.
     * fat16_find_free_cluster() taramanin nerede bitecegini
     * bilmek icin buna ihtiyac duyar. */
    uint32_t total_sectors;
    uint32_t total_clusters;
} fat16_volume_t;

/*
 * fat16_init() - Belirtilen surucudeki (drive) BPB'yi okur, temel
 * parametreleri cikarip vol icindeki durumu hazirlar.
 *
 * @drive : ATA_DRIVE_MASTER veya ATA_DRIVE_SLAVE
 * @vol   : cagiran tarafin ayirdigi (statik/global olabilir)
 *          fat16_volume_t yapisi; basarili donuste doldurulur.
 *
 * Donus: 0 basari, -1 hata (disk yok, ATA okuma hatasi ya da
 * gecersiz/taninmayan BPB imzasi).
 */
int fat16_init(uint8_t drive, fat16_volume_t *vol);

/*
 * fat16_list_root() - Root dizindeki gecerli (silinmemis, LFN/VOLUME_ID
 * olmayan) girisleri sirayla out_entries dizisine yazar.
 *
 * @vol         : fat16_init() ile mount edilmis volume
 * @out_entries : cagiran tarafin ayirdigi dizi
 * @max_entries : out_entries dizisinin kapasitesi
 *
 * Donus: yazilan giris sayisi (0..max_entries).
 */
int fat16_list_root(fat16_volume_t *vol, fat16_dirent_t *out_entries, int max_entries);

/*
 * fat16_find_in_root() - Root dizinde isimle (case-insensitive, 8.3
 * formatinda) dosya arar.
 *
 * Donus: 1 bulundu (out doldurulur), 0 bulunamadi.
 */
int fat16_find_in_root(fat16_volume_t *vol, const char *name, fat16_dirent_t *out);

/*
 * fat16_read_file() - Bir dirent'in isaret ettigi dosyanin tum
 * icerigini, cluster zincirini takip ederek buffer'a okur.
 *
 * @buffer     : cagiran tarafin ayirdigi arabellek
 * @buffer_cap : arabellegin kapasitesi (byte)
 *
 * Donus: okunan byte sayisi (dosya boyutu buffer_cap'ten buyukse
 * kesilir, buffer_cap kadar okunur), hata durumunda -1.
 */
int fat16_read_file(fat16_volume_t *vol, const fat16_dirent_t *entry,
                     void *buffer, uint32_t buffer_cap);

/*
 * fat16_write_file() - Dosyayi olusturur (yoksa) veya ustune yazar
 * (varsa). Eski cluster zinciri serbest birakilir, veri boyutuna
 * gore YENIDEN tahsis edilir (basit ama garantili yaklasim -
 * fragmentasyon optimizasyonu yapilmaz, alloc.c felsefesiyle ayni).
 *
 * @filename : 8.3 formatinda isim (ornek: "TEST.TXT"), LFN YOK
 * @data     : yazilacak veri (size==0 ise NULL olabilir)
 * @size     : byte cinsinden veri boyutu
 *
 * Donus: 0 basari, -1 hata (gecersiz isim, disk dolu, I/O hatasi,
 * root dizini dolu).
 */
int fat16_write_file(fat16_volume_t *vol, const char *filename,
                      const void *data, uint32_t size);

/*
 * fat16_delete_file() - Dosyayi siler: cluster zincirini FAT'ta
 * serbest birakir, dizin girisini 0xE5 (silinmis) isaretler.
 * Dizinleri (FAT_ATTR_DIRECTORY) SILMEZ.
 *
 * Donus: 0 basari, -1 hata (dosya yok/dizin/I/O hatasi).
 */
int fat16_delete_file(fat16_volume_t *vol, const char *filename);
