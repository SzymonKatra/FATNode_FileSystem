#include "fs.h"

#include <string.h>

#define FS_SECTOR_POS(x)         ((x) * FS_SECTOR_SIZE)

#define FS_NODES_IN_CLUSTER      (FS_SECTOR_SIZE / sizeof(_fs_node_t))
#define FS_STATES_IN_SECTOR      (FS_SECTOR_SIZE / sizeof(uint32_t))

#define FS_CLUSTER_EMPTY         0x00000000
#define FS_CLUSTER_EOF           0xFFFFFFFE
#define FS_CLUSTER_INVALID       0xFFFFFFFF
#define FS_CLUSTER_NODE_BEGIN    0xFFFFFF00
#define FS_CLUSTER_NODE_FULL     (FS_CLUSTER_NODE_BEGIN + FS_NODES_IN_CLUSTER)

#define FS_NODE_TYPE_FILE       1
#define FS_NODE_TYPE_DIR        2

#define FS_NODE_FLAGS_INUSE     (1 << 0)

typedef struct
{
    uint8_t     type;
    uint8_t     flags;
    uint16_t    links_count;
    uint32_t    size;
    uint32_t    cluster_index;
    uint32_t    modification_time;
} _fs_node_t;

typedef struct
{
    uint32_t sectors_count;
    uint32_t root_node;
} _fs_bootstrap_sector_t;

typedef struct
{
    uint32_t state[FS_STATES_IN_SECTOR];
} _fs_table_sector_t;

typedef struct
{
    _fs_node_t nodes[FS_NODES_IN_CLUSTER];
} _fs_node_cluster_t;

static int _fs_create_node(fs_t* fs, uint32_t* result_node_number);

static uint32_t _fs_cluster_to_sector(fs_t* fs, uint32_t cluster);
static size_t _fs_cluster_state_pos(fs_t* fs, uint32_t cluster);

static int _fs_write_sector_buffer(fs_t* fs, size_t sector_index); // uses fs->buffer
static int _fs_write_disk_buffer(fs_t* fs, size_t position, size_t size); // uses fs->buffer
static int _fs_write_disk(fs_t* fs, const void* buffer, size_t position, size_t size);

static int _fs_read_sector_buffer(fs_t* fs, size_t sector_index); // uses fs->buffer
static int _fs_read_disk_buffer(fs_t* fs, size_t position, size_t size); // uses fs->buffer
static int _fs_read_disk(fs_t* fs, void* buffer, size_t position, size_t size);

int fs_create(const fs_disk_operations_t* operations, size_t size, fs_t* result_fs)
{
    int error;
    
    result_fs->operations = *operations;
    
    error = result_fs->operations.init(&result_fs->state);
    if (error != FS_OK) return error;

    result_fs->sectors_count = size / FS_SECTOR_SIZE;
    
    memset(&result_fs->buffer, 0, FS_SECTOR_SIZE);
    for (uint32_t i = 0; i < result_fs->sectors_count; i++)
    {
        error = _fs_write_sector_buffer(result_fs, i);
        if (error != FS_OK) return error;
    }
    
    size_t remaining = size % FS_SECTOR_SIZE;
    if (remaining != 0)
    {
        error = _fs_write_disk_buffer(result_fs, FS_SECTOR_POS(result_fs->sectors_count), remaining);
        if (error != FS_OK) return error;
    }
    
    size_t table_size = result_fs->sectors_count * sizeof(uint32_t);
    result_fs->table_sector_start = 1;
    result_fs->table_sectors_count = table_size / FS_SECTOR_SIZE;
    result_fs->clusters_sector_start = result_fs->table_sector_start + result_fs->table_sectors_count;
    result_fs->clusters_count = result_fs->sectors_count - result_fs->table_sectors_count - 1;
    
    error = _fs_create_node(result_fs, &result_fs->root_node);
    if (error != FS_OK) return error;
    
    _fs_bootstrap_sector_t* bootstrap = (_fs_bootstrap_sector_t*)result_fs->buffer;
    bootstrap->sectors_count = result_fs->sectors_count;
    bootstrap->root_node = result_fs->root_node;
    
    error = _fs_write_disk_buffer(result_fs, 0, sizeof(_fs_bootstrap_sector_t));
    if (error != FS_OK) return error;
    
    return FS_OK;
}

int fs_close(fs_t* fs)
{
    int error;
    
    error = fs->operations.close(fs->state);
    if (error != FS_OK) return error;
    
    return FS_OK;
}

uint32_t fs_file(fs_t* fs)
{
    uint32_t x;
    int e = _fs_create_node(fs, &x);
    return x;
}

static int _fs_create_node(fs_t* fs, uint32_t* result_node_number)
{
    int error;
    
    // search for existing sector with free nodes
    
    uint32_t first_empty_cluster_index =  FS_CLUSTER_INVALID;
    uint32_t node_cluster_index = FS_CLUSTER_INVALID;
    uint32_t current_table_sector_index = 0xFFFFFFFF;
    _fs_table_sector_t* table_sector = (_fs_table_sector_t*)fs->buffer;
    
    for (uint32_t i = 0; i < fs->clusters_count; i++)
    {
        uint32_t required_table_sector_index = i / FS_STATES_IN_SECTOR;
        uint32_t array_index = i % FS_STATES_IN_SECTOR;
        
        if (current_table_sector_index != required_table_sector_index)
        {
            uint32_t final_table_sector_index = required_table_sector_index + fs->table_sector_start;
            error = _fs_read_sector_buffer(fs, final_table_sector_index);
            if (error != FS_OK) return error;
            
            current_table_sector_index = required_table_sector_index;
        }
        
        uint32_t cluster_state = table_sector->state[array_index];
        if (cluster_state == FS_CLUSTER_EMPTY)
        {
            if (first_empty_cluster_index == FS_CLUSTER_INVALID) first_empty_cluster_index = i;
        }
        else if (cluster_state >= FS_CLUSTER_NODE_BEGIN && cluster_state < FS_CLUSTER_NODE_FULL)
        {
            // found node sector with free place
            node_cluster_index = i;
            
            cluster_state++;
            size_t state_pos = _fs_cluster_state_pos(fs, node_cluster_index);
            
            error = _fs_write_disk(fs, &cluster_state, state_pos, sizeof(uint32_t));
            if (error != FS_OK) return error;
            
            break;
        }
    }
    
    _fs_node_cluster_t* node_cluster = (_fs_node_cluster_t*)fs->buffer;
    if (node_cluster_index != FS_CLUSTER_INVALID)
    {
        // find free place for node in cluster
        uint32_t node_sector_index = _fs_cluster_to_sector(fs, node_cluster_index);
        error = _fs_read_sector_buffer(fs, node_sector_index);
        if (error != FS_OK) return error;
    
        for (uint8_t i = 0; i < FS_NODES_IN_CLUSTER; i++)
        {
            if (!(node_cluster->nodes[i].flags & FS_NODE_FLAGS_INUSE))
            {
                // found free place
                _fs_node_t node = node_cluster->nodes[i];
                memset(&node, 0, sizeof(_fs_node_t));
                node.flags |= FS_NODE_FLAGS_INUSE;
                
                size_t node_pos = FS_SECTOR_POS(node_sector_index) + i * sizeof(_fs_node_t);
                
                error = _fs_write_disk(fs, &node, node_pos, sizeof(_fs_node_t));
                if (error != FS_OK) return error;
                
                *result_node_number = (node_cluster_index << 8) | i;
                
                return FS_OK;
            }
        }
    }
    else if (first_empty_cluster_index != FS_CLUSTER_INVALID)
    {
        // no node sector with free places found, start new cluster
        uint32_t sector_index = _fs_cluster_to_sector(fs, first_empty_cluster_index);
        
        uint32_t state = FS_CLUSTER_NODE_BEGIN + 1;
        size_t state_pos = _fs_cluster_state_pos(fs, first_empty_cluster_index);
        
        error = _fs_write_disk(fs, &state, state_pos, sizeof(uint32_t));
        if (error != FS_OK) return error;
        
        memset(fs->buffer, 0, FS_SECTOR_SIZE);
        node_cluster->nodes[0].flags |= FS_NODE_FLAGS_INUSE;
        
        error = _fs_write_sector_buffer(fs, sector_index);
        if (error != FS_OK) return error;
        
        *result_node_number = first_empty_cluster_index << 8;
        
        return FS_OK;
    }
    
    // place for new node not found - file system is full
    return FS_FULL;
}

static uint32_t _fs_cluster_to_sector(fs_t* fs, uint32_t cluster)
{
    return fs->clusters_sector_start + cluster;
}

static size_t _fs_cluster_state_pos(fs_t* fs, uint32_t cluster)
{
    return FS_SECTOR_POS(fs->table_sector_start) + cluster * sizeof(uint32_t);
}

static int _fs_write_sector_buffer(fs_t* fs, size_t sector_index) // uses fs->buffer
{
    return _fs_write_disk_buffer(fs, FS_SECTOR_POS(sector_index), FS_SECTOR_SIZE);
}
static int _fs_write_disk_buffer(fs_t* fs, size_t position, size_t size) // uses fs->buffer
{
    return _fs_write_disk(fs, &fs->buffer, position, size);
}
static int _fs_write_disk(fs_t* fs, const void* buffer, size_t position, size_t size)
{
    return fs->operations.write(fs->state, buffer, position, size);
}

static int _fs_read_sector_buffer(fs_t* fs, size_t sector_index) // uses fs->buffer
{
    return _fs_read_disk_buffer(fs, FS_SECTOR_POS(sector_index), FS_SECTOR_SIZE);
}
static int _fs_read_disk_buffer(fs_t* fs, size_t position, size_t size) // uses fs->buffer
{
    return _fs_read_disk(fs, &fs->buffer, position, size);
}
static int _fs_read_disk(fs_t* fs, void* buffer, size_t position, size_t size)
{
    return fs->operations.read(fs->state, buffer, position, size);
}