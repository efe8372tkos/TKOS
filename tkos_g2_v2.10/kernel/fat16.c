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

    

    uint32_t total_sectors = bpb.total_sectors_16
                              ? bpb.total_sectors_16
                              : bpb.total_sectors_32;
    if (total_sectors == 0) return -1; 

    vol->total_sectors = total_sectors;

    uint32_t volume_end_lba = FAT16_PARTITION_LBA + total_sectors;
    uint32_t data_sectors = (volume_end_lba > vol->data_start_lba)
                             ? (volume_end_lba - vol->data_start_lba) : 0;
    vol->total_clusters = data_sectors / vol->sectors_per_cluster;

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





#define FAT16_CLUSTER_FREE   0x0000
#define FAT16_CLUSTER_EOC    0xFFFF   









static int fat16_string_to_name(const char *filename, uint8_t *name8, uint8_t *ext3) {
    int i;
    for (i = 0; i < 8; i++) name8[i] = ' ';
    for (i = 0; i < 3; i++) ext3[i]  = ' ';

    if (!filename || !filename[0]) return 0;

    int ni = 0;
    const char *p = filename;
    while (*p && *p != '.') {
        if (ni >= 8) return 0; 
        name8[ni++] = (uint8_t)fat16_upper(*p);
        p++;
    }
    if (ni == 0) return 0; 

    if (*p == '.') {
        p++;
        int ei = 0;
        while (*p) {
            if (ei >= 3) return 0; 
            ext3[ei++] = (uint8_t)fat16_upper(*p);
            p++;
        }
    }
    return 1;
}







static int fat16_set_fat_entry(fat16_volume_t *vol, uint16_t cluster, uint16_t value) {
    uint32_t fat_offset       = (uint32_t)cluster * 2;
    uint32_t sector_in_fat    = fat_offset / vol->bytes_per_sector;
    uint32_t offset_in_sector = fat_offset % vol->bytes_per_sector;

    for (uint8_t f = 0; f < vol->num_fats; f++) {
        uint32_t fat_sector = vol->fat_start_lba +
                               (uint32_t)f * vol->sectors_per_fat + sector_in_fat;

        if (ata_read_sectors(vol->drive, fat_sector, 1, g_sector_buf) != 0) return -1;
        memcpy(&g_sector_buf[offset_in_sector], &value, sizeof(uint16_t));
        if (ata_write_sectors(vol->drive, fat_sector, 1, g_sector_buf) != 0) return -1;
    }
    return 0;
}








static int32_t fat16_find_free_cluster(fat16_volume_t *vol) {
    for (uint32_t c = 2; c < vol->total_clusters + 2; c++) {
        uint16_t val;
        if (fat16_get_next_cluster(vol, (uint16_t)c, &val) != 0) return -1;
        if (val == FAT16_CLUSTER_FREE) return (int32_t)c;
    }
    return -1;
}





static void fat16_free_chain(fat16_volume_t *vol, uint16_t cluster) {
    while (cluster != 0 && cluster < FAT16_CLUSTER_EOC_MIN) {
        uint16_t next;
        if (fat16_get_next_cluster(vol, cluster, &next) != 0) return;
        fat16_set_fat_entry(vol, cluster, FAT16_CLUSTER_FREE);
        cluster = next;
    }
}










static int fat16_locate_slot(fat16_volume_t *vol, const uint8_t *name8,
                              const uint8_t *ext3,
                              uint32_t *out_sector, int *out_index) {
    int      free_found  = 0;
    uint32_t free_sector = 0;
    int      free_index  = 0;

    for (uint32_t s = 0; s < vol->root_sector_count; s++) {
        if (ata_read_sectors(vol->drive, vol->root_start_lba + s, 1, g_sector_buf) != 0)
            return -1;

        fat_raw_dirent_t *raw = (fat_raw_dirent_t *)g_sector_buf;
        int entries_per_sector = vol->bytes_per_sector / sizeof(fat_raw_dirent_t);

        for (int i = 0; i < entries_per_sector; i++) {
            uint8_t first_byte = raw[i].name[0];

            if (first_byte == 0x00) {
                

                if (!free_found) {
                    free_sector = vol->root_start_lba + s;
                    free_index  = i;
                    free_found  = 1;
                }
                *out_sector = free_sector;
                *out_index  = free_index;
                return 0;
            }
            if (first_byte == 0xE5) {
                if (!free_found) {
                    free_sector = vol->root_start_lba + s;
                    free_index  = i;
                    free_found  = 1;
                }
                continue;
            }
            if (raw[i].attr == FAT_ATTR_LFN) continue;
            if (raw[i].attr & FAT_ATTR_VOLUME_ID) continue;

            if (memcmp(raw[i].name, name8, 8) == 0 &&
                memcmp(raw[i].ext,  ext3,  3) == 0) {
                *out_sector = vol->root_start_lba + s;
                *out_index  = i;
                return 1; 
            }
        }
    }

    if (free_found) {
        *out_sector = free_sector;
        *out_index  = free_index;
        return 0;
    }
    return -1; 
}




int fat16_write_file(fat16_volume_t *vol, const char *filename,
                      const void *data, uint32_t size) {
    if (!vol || !vol->mounted || !filename) return -1;
    if (size > 0 && !data) return -1;

    uint8_t name8[8], ext3[3];
    if (!fat16_string_to_name(filename, name8, ext3)) return -1;

    uint32_t dirent_sector;
    int      dirent_index;
    int      found = fat16_locate_slot(vol, name8, ext3, &dirent_sector, &dirent_index);
    if (found < 0) return -1; 

    


    if (found == 1) {
        if (ata_read_sectors(vol->drive, dirent_sector, 1, g_sector_buf) != 0) return -1;
        fat_raw_dirent_t *raw = (fat_raw_dirent_t *)g_sector_buf;
        uint16_t old_cluster = raw[dirent_index].first_cluster_lo;
        if (old_cluster != 0) fat16_free_chain(vol, old_cluster);
    }

    
    uint32_t bytes_per_cluster = (uint32_t)vol->bytes_per_sector * vol->sectors_per_cluster;
    uint32_t n_clusters = (size == 0) ? 0
                          : (size + bytes_per_cluster - 1) / bytes_per_cluster;

    uint16_t first_cluster = 0;
    uint16_t prev_cluster  = 0;

    for (uint32_t i = 0; i < n_clusters; i++) {
        int32_t c = fat16_find_free_cluster(vol);
        if (c < 0) {
            
            if (first_cluster != 0) fat16_free_chain(vol, first_cluster);
            return -1;
        }

        fat16_set_fat_entry(vol, (uint16_t)c, FAT16_CLUSTER_EOC);

        if (prev_cluster == 0) first_cluster = (uint16_t)c;
        else                   fat16_set_fat_entry(vol, prev_cluster, (uint16_t)c);

        prev_cluster = (uint16_t)c;
    }

    
    uint32_t       bytes_written = 0;
    uint16_t       cluster       = first_cluster;
    const uint8_t *src           = (const uint8_t *)data;

    while (cluster != 0 && bytes_written < size) {
        uint32_t cluster_lba = vol->data_start_lba +
            (uint32_t)(cluster - 2) * vol->sectors_per_cluster;

        for (uint32_t s = 0; s < vol->sectors_per_cluster && bytes_written < size; s++) {
            uint32_t remaining = size - bytes_written;
            uint32_t copy_len  = vol->bytes_per_sector;
            if (copy_len > remaining) copy_len = remaining;

            memcpy(g_sector_buf, src + bytes_written, copy_len);
            if (copy_len < vol->bytes_per_sector)
                memset(g_sector_buf + copy_len, 0, vol->bytes_per_sector - copy_len);

            if (ata_write_sectors(vol->drive, cluster_lba + s, 1, g_sector_buf) != 0)
                return -1; 

            bytes_written += copy_len;
        }

        uint16_t next;
        if (fat16_get_next_cluster(vol, cluster, &next) != 0) break;
        cluster = (next >= FAT16_CLUSTER_EOC_MIN) ? 0 : next;
    }

    
    if (ata_read_sectors(vol->drive, dirent_sector, 1, g_sector_buf) != 0) return -1;
    fat_raw_dirent_t *raw = (fat_raw_dirent_t *)g_sector_buf;
    fat_raw_dirent_t *e   = &raw[dirent_index];

    memcpy(e->name, name8, 8);
    memcpy(e->ext,  ext3,  3);
    e->attr              = FAT_ATTR_ARCHIVE;
    e->reserved          = 0;
    e->create_time_tenth = 0;
    e->create_time       = 0;
    e->create_date       = 0;
    e->access_date       = 0;
    e->first_cluster_hi  = 0;
    e->write_time        = 0;
    e->write_date        = 0;
    e->first_cluster_lo  = first_cluster;
    e->file_size         = size;

    if (ata_write_sectors(vol->drive, dirent_sector, 1, g_sector_buf) != 0) return -1;

    return 0;
}




int fat16_delete_file(fat16_volume_t *vol, const char *filename) {
    if (!vol || !vol->mounted || !filename) return -1;

    uint8_t name8[8], ext3[3];
    if (!fat16_string_to_name(filename, name8, ext3)) return -1;

    uint32_t dirent_sector;
    int      dirent_index;
    int found = fat16_locate_slot(vol, name8, ext3, &dirent_sector, &dirent_index);
    if (found != 1) return -1; 

    if (ata_read_sectors(vol->drive, dirent_sector, 1, g_sector_buf) != 0) return -1;
    fat_raw_dirent_t *raw = (fat_raw_dirent_t *)g_sector_buf;
    fat_raw_dirent_t *e   = &raw[dirent_index];

    if (e->attr & FAT_ATTR_DIRECTORY) return -1; 

    uint16_t cluster = e->first_cluster_lo;
    if (cluster != 0) fat16_free_chain(vol, cluster);

    e->name[0] = 0xE5; 

    if (ata_write_sectors(vol->drive, dirent_sector, 1, g_sector_buf) != 0) return -1;
    return 0;
}
