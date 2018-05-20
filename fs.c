#include "fs.h"

#include <string>

#define FS_SECTOR_SIZE      512
#define FS_SECTOR_POS(x)    ((x) * FS_SECTOR_SIZE)

typedef struct
{
    uint8_t     flags;
    uint8_t     type;
    uint32_t    size;
    uint16_t    links_count;
    uint32_t    start_sector;
    uint32_t    modification_time;
} node_t;

#define FS_NODES_IN_SECTOR   (FS_SECTOR_SIZE / sizeof(node_t))

#define FS_SECTOR_EMPTY      0x00000000
#define FS_SECTOR_EOF        0xFFFFFFFF
#define FS_SECTOR_NODE_BEGIN 0xFFFFFF00
#define FS_SECTOR_NODE_FULL  (FS_SECTOR_NODE_BEGIN + FS_NODES_IN_SECTOR)

static int _fs_write_sector(fs_t* fs, size_t sector_index); // uses fs->buffer
static int _fs_write_disk(fs_t* fs, size_t position, size_t size); // uses fs->buffer
static int _fs_write_disk(fs_t* fs, void* buffer, size_t position, size_t size);

static int _fs_read_sector(fs_t* fs, size_t sector_index); // uses fs->buffer
static int _fs_read_disk(fs_t* fs, size_t position, size_t size); // uses fs->buffer
static int _fs_read_disk(fs_t* fs, void* buffer, size_t position, size_t size);

int fs_create(const fs_disk_operations_t* operations, size_t size, fs_t* result_fs)
{
    result_fs->operations = *operations;
    
    if (result->operations.init(result_fs->state) != FS_OK) return FS_DISK_INIT_ERROR;

    result_fs->sectors_count = size / FS_SECTOR_SIZE;
    
    memset(&result_fs->buffer, 0, FS_SECTOR_SIZE);
    for (size_t i = 0; i < result_fs->sectors_count; i++)
    {
        _fs_write_sector(result_fs, i);
    }
    
    size_t remaining = size % FS_SECTOR_SIZE;
    if (remaining != 0)
    {
        _fs_write_disk(result_fs, FS_SECTOR_POS(result_fs->sectors_count), remaining);
    }
}

int fs_close(fs_t* fs)
{
    if (result->operations.close(fs->state) != FS_OK) return FS_DISK_CLOSE_ERROR;
}

static int _fs_write_sector(fs_t* fs, size_t sector_index) // uses state->buffer
{
    return _fs_write_disk(fs, FS_SECTOR_POS(sector_index), FS_SECTOR_SIZE);
}
static int _fs_write_disk(fs_t* fs, size_t position, size_t size) // uses state->buffer
{
    return _fs_write_disk(fs, &fs->buffer, position, size);
}
static int _fs_write_disk(fs_t* fs, void* buffer, size_t position, size_t size)
{
    return fs->operations.write(fs->state, buffer, position, size);
}

static int _fs_read_sector(fs_t* fs, size_t sector_index) // uses fs->buffer
{
    return _fs_read_sector(fs, FS_SECTOR_POS(sector_index), FS_SECTOR_SIZE);
}
static int _fs_read_disk(fs_t* fs, size_t position, size_t size) // uses fs->buffer
{
    return _fs_read_disk(fs, &fs->buffer, position, size);
}
static int _fs_read_disk(fs_t* fs, void* buffer, size_t position, size_t size)
{
    return fs->operations.read(fs->state, buffer, position, size);
}