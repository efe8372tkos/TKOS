
#pragma once
#include "types.h"
#include "fat16.h"
#include "task.h"


















#define TKX_MAGIC     0x31584B54UL   

typedef struct __attribute__((packed)) {
    uint32_t magic;         
    uint32_t version;       
    uint64_t entry_offset;  
    uint64_t image_size;    
    uint64_t stack_size;    
} tkx_header_t;

typedef enum {
    EXEC_OK              =  0,
    EXEC_ERR_NOT_FOUND   = -1,
    EXEC_ERR_IS_DIR      = -2,
    EXEC_ERR_TOO_SMALL   = -3,
    EXEC_ERR_NOMEM       = -4,
    EXEC_ERR_IO          = -5,
    EXEC_ERR_BAD_MAGIC   = -6,
    EXEC_ERR_BAD_ENTRY   = -7,
    EXEC_ERR_TRUNCATED   = -8,
    EXEC_ERR_TASK        = -9,
} exec_result_t;









int exec_load(fat16_volume_t *vol, const char *filename, task_t **out_task);


const char *exec_strerror(int result);
