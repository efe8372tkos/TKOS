#pragma once
#include "types.h"
#include "ata.h"



































#define FAT16_PARTITION_LBA   2048

#define FAT16_MAX_NAME        13   


#define FAT_ATTR_READ_ONLY  0x01
#define FAT_ATTR_HIDDEN     0x02
#define FAT_ATTR_SYSTEM     0x04
#define FAT_ATTR_VOLUME_ID  0x08
#define FAT_ATTR_DIRECTORY  0x10
#define FAT_ATTR_ARCHIVE    0x20
#define FAT_ATTR_LFN        0x0F  






typedef struct {
    char     name[FAT16_MAX_NAME];  
    uint8_t  attr;                  
    uint32_t size;                  
    uint16_t first_cluster;         
} fat16_dirent_t;








typedef struct {
    uint8_t  drive;              
    int      mounted;            

    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint8_t  num_fats;
    uint16_t root_entry_count;
    uint16_t sectors_per_fat;

    uint32_t fat_start_lba;
    uint32_t root_start_lba;
    uint32_t root_sector_count;
    uint32_t data_start_lba;

    


    uint32_t total_sectors;
    uint32_t total_clusters;
} fat16_volume_t;












int fat16_init(uint8_t drive, fat16_volume_t *vol);











int fat16_list_root(fat16_volume_t *vol, fat16_dirent_t *out_entries, int max_entries);







int fat16_find_in_root(fat16_volume_t *vol, const char *name, fat16_dirent_t *out);











int fat16_read_file(fat16_volume_t *vol, const fat16_dirent_t *entry,
                     void *buffer, uint32_t buffer_cap);














int fat16_write_file(fat16_volume_t *vol, const char *filename,
                      const void *data, uint32_t size);








int fat16_delete_file(fat16_volume_t *vol, const char *filename);
