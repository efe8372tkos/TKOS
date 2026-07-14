#include "fat16.h"
#include "ata.h"
#include "string.h"
typedef struct __attribute__((packed)) {
    uint8_t  jmp[3];
    uint8_t  oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sector_count;
    uint8_t  num_fats;
    uint16_t root_entry_count;
    uint16_t total_sectors_16;
    uint8_t  media_type;
    uint16_t sectors_per_fat;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
} fat16_bpb_t;
typedef struct __attribute__((packed)) {
    uint8_t  name[8];       
    uint8_t  ext[3];        
    uint8_t  attr;
    uint8_t  reserved;
    uint8_t  create_time_tenth;
    uint16_t create_time;
    uint16_t create_date;
    uint16_t access_date;
    uint16_t first_cluster_hi; 
    uint16_t write_time;
    uint16_t write_date;
    uint16_t first_cluster_lo;
    uint32_t file_size;
} fat_raw_dirent_t;
#define FAT16_CLUSTER_EOC_MIN   0xFFF8  
static uint8_t g_sector_buf[512];
static void fat16_name_to_string(const uint8_t *name8, const uint8_t *ext3, char *out) {
    int pos = 0;
    for (int i = 0; i < 8 && name8[i] != ' '; i++)
        out[pos++] = (char)name8[i];
    if (ext3[0] != ' ') {
        out[pos++] = '.';
        for (int i = 0; i < 3 && ext3[i] != ' '; i++)
            out[pos++] = (char)ext3[i];
    }
    out[pos] = '\0';
}
static char fat16_upper(char c) {
    if (c >= 'a' && c <= 'z') return (char)(c - 'a' + 'A');
    return c;
}
static int fat16_streq_ci(const char *a, const char *b) {
    while (*a && *b) {
        if (fat16_upper(*a) != fat16_upper(*b)) return 0;
        a++; b++;
    }
    return *a == '\0' && *b == '\0';
}
int fat16_init(uint8_t drive, fat16_volume_t *vol) {
    if (!vol) return -1;
    vol->mounted = 0;
    vol->drive   = drive;
    if (!ata_drive_present(drive)) return -1;
    if (ata_read_sectors(drive, FAT16_PARTITION_LBA, 1, g_sector_buf) != 0)
        return -1;
    fat16_bpb_t bpb;
    memcpy(&bpb, g_sector_buf, sizeof(fat16_bpb_t));
    if (bpb.bytes_per_sector != 512)      return -1;
    if (bpb.num_fats == 0)                return -1;
    if (bpb.sectors_per_cluster == 0)     return -1;
    if (bpb.sectors_per_fat == 0)         return -1;
    vol->bytes_per_sector    = bpb.bytes_per_sector;
    vol->sectors_per_cluster = bpb.sectors_per_cluster;
    vol->num_fats            = bpb.num_fats;
    vol->root_entry_count    = bpb.root_entry_count;
    vol->sectors_per_fat     = bpb.sectors_per_fat;
    vol->fat_start_lba = FAT16_PARTITION_LBA + bpb.reserved_sector_count;
    uint32_t fat_area_sectors = (uint32_t)bpb.num_fats * bpb.sectors_per_fat;
    vol->root_start_lba = vol->fat_start_lba + fat_area_sectors;
    uint32_t root_bytes = (uint32_t)bpb.root_entry_count * sizeof(fat_raw_dirent_t);
    vol->root_sector_count = (root_bytes + bpb.bytes_per_sector - 1) / bpb.bytes_per_sector;
    vol->data_start_lba = vol->root_start_lba + vol->root_sector_count;
    vol->mounted = 1;
    return 0;
}
static int fat16_get_next_cluster(fat16_volume_t *vol, uint16_t cluster, uint16_t *out_next) {
    if (!vol || !vol->mounted) return -1;
    uint32_t fat_offset       = (uint32_t)cluster * 2; 
    uint32_t fat_sector       = vol->fat_start_lba + (fat_offset / vol->bytes_per_sector);
    uint32_t offset_in_sector = fat_offset % vol->bytes_per_sector;
    if (ata_read_sectors(vol->drive, fat_sector, 1, g_sector_buf) != 0) return -1;
    uint16_t value;
    memcpy(&value, &g_sector_buf[offset_in_sector], sizeof(uint16_t));
    *out_next = value;
    return 0;
}
int fat16_list_root(fat16_volume_t *vol, fat16_dirent_t *out_entries, int max_entries) {
    if (!vol || !vol->mounted || !out_entries || max_entries <= 0) return 0;
    int found = 0;
    for (uint32_t s = 0; s < vol->root_sector_count && found < max_entries; s++) {
        if (ata_read_sectors(vol->drive, vol->root_start_lba + s, 1, g_sector_buf) != 0)
            break;
        fat_raw_dirent_t *raw = (fat_raw_dirent_t *)g_sector_buf;
        int entries_per_sector = vol->bytes_per_sector / sizeof(fat_raw_dirent_t);
        for (int i = 0; i < entries_per_sector && found < max_entries; i++) {
            uint8_t first_byte = raw[i].name[0];
            if (first_byte == 0x00) return found; 
            if (first_byte == 0xE5) continue;      
            if (raw[i].attr == FAT_ATTR_LFN) continue;
            if (raw[i].attr & FAT_ATTR_VOLUME_ID) continue;
            fat16_dirent_t *e = &out_entries[found];
            fat16_name_to_string(raw[i].name, raw[i].ext, e->name);
            e->attr           = raw[i].attr;
            e->size           = raw[i].file_size;
            e->first_cluster  = raw[i].first_cluster_lo;
            found++;
        }
    }
    return found;
}
int fat16_find_in_root(fat16_volume_t *vol, const char *name, fat16_dirent_t *out) {
    if (!vol || !vol->mounted || !name || !out) return 0;
    for (uint32_t s = 0; s < vol->root_sector_count; s++) {
        if (ata_read_sectors(vol->drive, vol->root_start_lba + s, 1, g_sector_buf) != 0)
            break;
        fat_raw_dirent_t *raw = (fat_raw_dirent_t *)g_sector_buf;
        int entries_per_sector = vol->bytes_per_sector / sizeof(fat_raw_dirent_t);
        for (int i = 0; i < entries_per_sector; i++) {
            uint8_t first_byte = raw[i].name[0];
            if (first_byte == 0x00) return 0; 
            if (first_byte == 0xE5) continue;
            if (raw[i].attr == FAT_ATTR_LFN) continue;
            if (raw[i].attr & FAT_ATTR_VOLUME_ID) continue;
            char candidate[FAT16_MAX_NAME];
            fat16_name_to_string(raw[i].name, raw[i].ext, candidate);
            if (fat16_streq_ci(candidate, name)) {
                out->attr          = raw[i].attr;
                out->size          = raw[i].file_size;
                out->first_cluster = raw[i].first_cluster_lo;
                memcpy(out->name, candidate, FAT16_MAX_NAME);
                return 1;
            }
        }
    }
    return 0;
}
int fat16_read_file(fat16_volume_t *vol, const fat16_dirent_t *entry,
                     void *buffer, uint32_t buffer_cap) {
    if (!vol || !vol->mounted || !entry || !buffer) return -1;
    if (entry->attr & FAT_ATTR_DIRECTORY) return -1; 
    uint32_t to_read = entry->size;
    if (to_read > buffer_cap) to_read = buffer_cap;
    uint16_t cluster    = entry->first_cluster;
    uint32_t bytes_done = 0;
    uint8_t *out        = (uint8_t *)buffer;
    while (cluster != 0 && cluster < FAT16_CLUSTER_EOC_MIN && bytes_done < to_read) {
        uint32_t cluster_lba = vol->data_start_lba +
            (uint32_t)(cluster - 2) * vol->sectors_per_cluster;
        for (uint32_t s = 0; s < vol->sectors_per_cluster && bytes_done < to_read; s++) {
            if (ata_read_sectors(vol->drive, cluster_lba + s, 1, g_sector_buf) != 0)
                return -1;
            uint32_t remaining = to_read - bytes_done;
            uint32_t copy_len  = vol->bytes_per_sector;
            if (copy_len > remaining) copy_len = remaining;
            memcpy(out + bytes_done, g_sector_buf, copy_len);
            bytes_done += copy_len;
        }
        if (fat16_get_next_cluster(vol, cluster, &cluster) != 0)
            return -1;
    }
    return (int)bytes_done;
}
