#ifndef FS_H_
#define FS_H_

#define FS_OK 0
#define FS_DISK_INIT_ERROR      1
#define FS_DISK_READ_ERROR      2
#define FS_DISK_WRITE_ERROR     3
#define FS_DISK_CLOSE_ERROR     4
#define FS_FULL                 5

#define FS_SECTOR_SIZE          512

#include <stdint.h>
#include <stddef.h>

typedef int (*disk_init)(void** result_state);
typedef int (*disk_read)(void* state, void* buffer, size_t position, size_t size);
typedef int (*disk_write)(void* state, const void* buffer, size_t position, size_t size);
typedef int (*disk_close)(void* state);

typedef struct
{
    disk_init   init;
    disk_read   read;
    disk_write  write;
    disk_close  close;
} fs_disk_operations_t;

typedef struct
{
    void*       state;
    fs_disk_operations_t operations;
    uint32_t    sectors_count;
    uint32_t    table_sector_start;
    uint32_t    table_sectors_count;
    uint32_t    clusters_sector_start;
    uint32_t    clusters_count;
    uint32_t    root_node;
    char        buffer[FS_SECTOR_SIZE];
} fs_t;

int fs_create(const fs_disk_operations_t* operations, size_t size, fs_t* result_fs);
int fs_open(const fs_disk_operations_t* operations, fs_t* result_fs);

int fs_close(fs_t* fs);


uint32_t fs_file(fs_t* fs);

#endif