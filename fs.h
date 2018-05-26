#ifndef FS_H_
#define FS_H_

#define FS_OK 0
#define FS_DISK_INIT_ERROR      1
#define FS_DISK_READ_ERROR      2
#define FS_DISK_WRITE_ERROR     3
#define FS_DISK_CLOSE_ERROR     4
#define FS_FULL                 5
#define FS_NOT_A_DIRECTORY      6
#define FS_WRONG_PATH           7
#define FS_PATH_TOO_LONG        8
#define FS_DIR_NAME_TOO_LONG    9
#define FS_BUFFER_TOO_SMALL     10

#define FS_SECTOR_SIZE          128

#define FS_DIR_NAME_MAX_LENGTH  27

#define FS_FILE     1
#define FS_DIR      2

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

typedef struct
{
    char        name[28];
    uint32_t    node;
    uint8_t     type;
} fs_dir_entry_t;

int fs_create(const fs_disk_operations_t* operations, size_t size, fs_t* result_fs);
int fs_open(const fs_disk_operations_t* operations, fs_t* result_fs);

int fs_close(fs_t* fs);

int fs_mkdir(fs_t* fs, const char* path);
int fs_dir_entries_count(fs_t* fs, const char* path, uint32_t* result);
int fs_dir_list(fs_t* fs, const char* path, fs_dir_entry_t* results, size_t* count, size_t max_results);

#endif