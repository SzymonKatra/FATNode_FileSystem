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
#define FS_NAME_TOO_LONG        9
#define FS_BUFFER_TOO_SMALL     10
#define FS_NOT_A_FILE           11
#define FS_NOT_EXISTS           12
#define FS_FILE_CLOSED          13
#define FS_EOF                  14
#define FS_ALREADY_EXISTS       15

#define FS_SECTOR_SIZE          128

#define FS_PATH_MAX_LENGTH      255
#define FS_NAME_MAX_LENGTH      27

#define FS_FILE         1
#define FS_DIR          2

#define FS_CREATE       (1 << 0)
#define FS_APPEND       (1 << 1)

#define FS_SEEK_BEGIN   1
#define FS_SEEK_CURRENT 2
#define FS_SEEK_END     3

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
    char        name[FS_NAME_MAX_LENGTH + 1];
    uint32_t    node;
    uint8_t     node_type;
    uint16_t    node_links_count;
} fs_dir_entry_t;

typedef struct
{
    uint32_t    node;
    uint32_t    pos;
    uint32_t    size;
    uint32_t    first_cluster;
    uint32_t    current_cluster;
    uint32_t    current_cluster_pos;
    uint8_t     is_opened;
} fs_file_t;

typedef struct
{
    uint32_t sectors;
    uint32_t clusters;
    uint32_t table_sectors;
    uint32_t free_clusters;
    uint32_t node_clusters;
    uint32_t data_clusters;
    uint32_t nodes;
    uint32_t allocated_nodes;
    uint32_t files_size;
    uint32_t dir_structures_size;
    uint32_t nodes_size;
    uint32_t used_space;
    uint32_t free_space;
    uint32_t total_size;
    uint32_t usable_space;
} fs_info_t;

int fs_create(const fs_disk_operations_t* operations, size_t size, fs_t* result_fs);
int fs_open(const fs_disk_operations_t* operations, fs_t* result_fs);

int fs_close(fs_t* fs);

int fs_mkdir(fs_t* fs, const char* path);
int fs_dir_entries_count(fs_t* fs, const char* path, uint32_t* result);
int fs_size(fs_t* fs, uint32_t node, uint32_t* files_size);
int fs_dir_list(fs_t* fs, const char* path, fs_dir_entry_t* results, size_t* count, size_t max_results);
int fs_entry_info(fs_t* fs, const char* path, fs_dir_entry_t* result);
int fs_link(fs_t* fs, const char* path, uint32_t node);
int fs_remove(fs_t* fs, const char* path);
int fs_info(fs_t* fs, fs_info_t* result);

int fs_file_open(fs_t* fs, const char* path, uint8_t flags, fs_file_t* result);
int fs_file_write(fs_t* fs, fs_file_t* file, const void* buffer, size_t size, size_t* written);
int fs_file_read(fs_t* fs, fs_file_t* file, void* buffer, size_t size, size_t* read);
int fs_file_seek(fs_t* fs, fs_file_t* file, uint8_t mode, int32_t pos);
int fs_file_discard(fs_t* fs, fs_file_t* file);
int fs_file_close(fs_t* fs, fs_file_t* file);

#endif