#include "fs.h"

#include <string.h>
#include <time.h>

#define FS_CHECK_ERROR(x)      { int error = x; if (error != FS_OK) return error; }

#define FS_SECTOR_POS(x)        ((x) * FS_SECTOR_SIZE)

#define FS_STATES_IN_SECTOR     (FS_SECTOR_SIZE / sizeof(uint32_t))
#define FS_NODES_IN_CLUSTER     (FS_SECTOR_SIZE / sizeof(_fs_node_t))
#define FS_REFERENCES_IN_CLUSTER (FS_SECTOR_SIZE / sizeof(_fs_reference_t))

#define FS_CLUSTER_EMPTY        0x00000000
#define FS_CLUSTER_EOF          0xFFFFFFFE
#define FS_CLUSTER_INVALID      0xFFFFFFFF
#define FS_CLUSTER_NODE_BEGIN   0xFFFFFF00
#define FS_CLUSTER_NODE_FULL    (FS_CLUSTER_NODE_BEGIN + FS_NODES_IN_CLUSTER)

#define FS_NODE_TYPE_FILE       1
#define FS_NODE_TYPE_DIR        2

#define FS_FIND_FILE        1
#define FS_FIND_DIR         2
#define FS_FIND_NOT_EXISTS  3

#define FS_NODE_FLAGS_INUSE     (1 << 0)

typedef struct
{
    uint8_t     flags;
    uint8_t     type;
    uint16_t    links_count;
    uint32_t    size;
    uint32_t    cluster_index;
    uint32_t    modification_time;
} _fs_node_t;

typedef struct
{
    uint32_t    sectors_count;
    uint32_t    root_node;
} _fs_bootstrap_sector_t;

typedef struct
{
    uint32_t    state[FS_STATES_IN_SECTOR];
} _fs_table_sector_t;

typedef struct
{
    _fs_node_t  nodes[FS_NODES_IN_CLUSTER];
} _fs_node_cluster_t;

typedef struct
{
    char        name[FS_NAME_MAX_LENGTH + 1];
    uint32_t    node;
} _fs_reference_t;

typedef struct
{
    _fs_reference_t ref[FS_REFERENCES_IN_CLUSTER];
} _fs_dir_cluster_t;

static int _fs_find_free_cluster(fs_t* fs, uint32_t* result);
static int _fs_create_node(fs_t* fs, uint32_t* result_node_number);
static int _fs_create_dir(fs_t* fs, uint32_t node, uint32_t parent_node, uint32_t* result_cluster);
static int _fs_dir_find_entry(fs_t* fs, uint32_t dir_node, const char* entry_name, uint8_t* result_code, uint32_t* result_node);
static int _fs_dir_add_entry(fs_t* fs, uint32_t dir_node, const char* entry_name, uint32_t entry_node);
static int _fs_find_node(fs_t* fs, const char* path, uint32_t* result_node, uint8_t* result_code);

static uint32_t _fs_cluster_to_sector(fs_t* fs, uint32_t cluster);
static size_t _fs_cluster_state_pos(fs_t* fs, uint32_t cluster);
static size_t _fs_node_pos(fs_t* fs, uint32_t node_number);

static int _fs_write_state(fs_t* fs, uint32_t cluster, uint32_t new_state);
static int _fs_write_node(fs_t* fs, uint32_t node_number, const _fs_node_t* node_data);
static int _fs_write_cluster_buffer(fs_t* fs, uint32_t cluster); // uses fs->buffer
static int _fs_write_sector_buffer(fs_t* fs, size_t sector_index); // uses fs->buffer
static int _fs_write_disk_buffer(fs_t* fs, size_t position, size_t size); // uses fs->buffer
static int _fs_write_disk(fs_t* fs, const void* buffer, size_t position, size_t size);

static int _fs_read_state(fs_t* fs, uint32_t cluster, uint32_t* result_state);
static int _fs_read_node(fs_t* fs, uint32_t node_number, _fs_node_t* node_data);
static int _fs_read_cluster_buffer(fs_t* fs, uint32_t cluster); // uses fs->buffer
static int _fs_read_sector_buffer(fs_t* fs, size_t sector_index); // uses fs->buffer
static int _fs_read_disk_buffer(fs_t* fs, size_t position, size_t size); // uses fs->buffer
static int _fs_read_disk(fs_t* fs, void* buffer, size_t position, size_t size);

int fs_create(const fs_disk_operations_t* operations, size_t size, fs_t* result_fs)
{   
    result_fs->operations = *operations;
    
    FS_CHECK_ERROR(result_fs->operations.init(&result_fs->state));
    
    result_fs->sectors_count = size / FS_SECTOR_SIZE;
    
    memset(result_fs->buffer, 0, FS_SECTOR_SIZE);
    for (uint32_t i = 0; i < result_fs->sectors_count; i++)
    {
        FS_CHECK_ERROR(_fs_write_sector_buffer(result_fs, i));
    }
    
    size_t remaining = size % FS_SECTOR_SIZE;
    if (remaining != 0)
    {
        FS_CHECK_ERROR(_fs_write_disk_buffer(result_fs, FS_SECTOR_POS(result_fs->sectors_count), remaining));
    }
    
    size_t table_size = result_fs->sectors_count * sizeof(uint32_t);
    result_fs->table_sector_start = 1;
    result_fs->table_sectors_count = table_size / FS_SECTOR_SIZE;
    if (table_size % FS_SECTOR_SIZE != 0) result_fs->table_sectors_count++;
    result_fs->clusters_sector_start = result_fs->table_sector_start + result_fs->table_sectors_count;
    result_fs->clusters_count = result_fs->sectors_count - result_fs->table_sectors_count - 1;
    
    FS_CHECK_ERROR(_fs_create_node(result_fs, &result_fs->root_node));
    
    _fs_node_t root_node_data;
    FS_CHECK_ERROR(_fs_read_node(result_fs, result_fs->root_node, &root_node_data));
    root_node_data.type = FS_NODE_TYPE_DIR;
    root_node_data.links_count = 1;
    
    FS_CHECK_ERROR(_fs_create_dir(result_fs, result_fs->root_node, result_fs->root_node, &root_node_data.cluster_index));  
    
    FS_CHECK_ERROR(_fs_write_node(result_fs, result_fs->root_node, &root_node_data));
    
    _fs_bootstrap_sector_t* bootstrap = (_fs_bootstrap_sector_t*)result_fs->buffer;
    bootstrap->sectors_count = result_fs->sectors_count;
    bootstrap->root_node = result_fs->root_node;
    
    FS_CHECK_ERROR(_fs_write_disk_buffer(result_fs, 0, sizeof(_fs_bootstrap_sector_t)));
    
    return FS_OK;
}

int fs_close(fs_t* fs)
{
    FS_CHECK_ERROR(fs->operations.close(fs->state));
    
    return FS_OK;
}

int fs_mkdir(fs_t* fs, const char* path)
{
    if (path[0] != '/') return FS_WRONG_PATH;
    
    uint32_t node = fs->root_node;
    
    if (strlen(path) > 255) return FS_PATH_TOO_LONG;
    
    char pathBuffer[256];
    strcpy(pathBuffer, path);
    char* name = strtok(pathBuffer, "/");
    while (name != NULL)
    {
        if (strlen(name) > FS_NAME_MAX_LENGTH) return FS_NAME_TOO_LONG;
        
        uint8_t find_status;
        uint32_t find_node;
        FS_CHECK_ERROR(_fs_dir_find_entry(fs, node, name, &find_status, &find_node));
        if (find_status == FS_FIND_FILE) return FS_NOT_A_DIRECTORY;
        else if (find_status == FS_FIND_NOT_EXISTS)
        {
            uint32_t new_node;
            FS_CHECK_ERROR(_fs_create_node(fs, &new_node));
            
            _fs_node_t new_node_data;
            FS_CHECK_ERROR(_fs_read_node(fs, new_node, &new_node_data));
            new_node_data.type = FS_NODE_TYPE_DIR;
            new_node_data.links_count = 1;
            new_node_data.modification_time = (uint32_t)time(NULL);
            
            FS_CHECK_ERROR(_fs_create_dir(fs, new_node, node, &new_node_data.cluster_index));
            
            FS_CHECK_ERROR(_fs_write_node(fs, new_node, &new_node_data));
            
            FS_CHECK_ERROR(_fs_dir_add_entry(fs, node, name, new_node));
            
            node = new_node;
        }
        else if (find_status == FS_FIND_DIR)
        {
            node = find_node;
        }
        
        name = strtok(NULL, "/");
    }
    
    return FS_OK;
}

int fs_dir_entries_count(fs_t* fs, const char* path, uint32_t* result)
{
    uint32_t node;
    uint8_t status;
    FS_CHECK_ERROR(_fs_find_node(fs, path, &node, &status));
    if (status != FS_FIND_DIR)
    {
        switch (status)
        {
            case FS_FIND_FILE: return FS_NOT_A_DIRECTORY;
            case FS_FIND_NOT_EXISTS: return FS_WRONG_PATH;
        }
    }
    
    _fs_node_t node_data;
    FS_CHECK_ERROR(_fs_read_node(fs, node, &node_data));
    
    if (node_data.type != FS_NODE_TYPE_DIR) return FS_NOT_A_DIRECTORY;
    
    _fs_dir_cluster_t* dir = (_fs_dir_cluster_t*)fs->buffer;
    
    *result = 0;
    uint32_t current_cluster = node_data.cluster_index;
    do
    {
        FS_CHECK_ERROR(_fs_read_cluster_buffer(fs, current_cluster));
    
        for (size_t i = 0; i < FS_REFERENCES_IN_CLUSTER; i++)
        {
            if (dir->ref[i].name[0] != 0) (*result)++;
        }
        
        FS_CHECK_ERROR(_fs_read_state(fs, current_cluster, &current_cluster));
    }
    while (current_cluster != FS_CLUSTER_EOF);
    
    return FS_OK;
}

int fs_dir_list(fs_t* fs, const char* path, fs_dir_entry_t* results, size_t* count, size_t max_results)
{
    uint32_t node;
    uint8_t status;
    FS_CHECK_ERROR(_fs_find_node(fs, path, &node, &status));
    if (status != FS_FIND_DIR)
    {
        switch (status)
        {
            case FS_FIND_FILE: return FS_NOT_A_DIRECTORY;
            case FS_FIND_NOT_EXISTS: return FS_WRONG_PATH;
        }
    }
    
    _fs_node_t node_data;
    FS_CHECK_ERROR(_fs_read_node(fs, node, &node_data));
    
    if (node_data.type != FS_NODE_TYPE_DIR) return FS_NOT_A_DIRECTORY;
    
    _fs_dir_cluster_t* dir = (_fs_dir_cluster_t*)fs->buffer;
    
    *count = 0;
    uint32_t current_cluster = node_data.cluster_index;
    do
    {
        FS_CHECK_ERROR(_fs_read_cluster_buffer(fs, current_cluster));
    
        for (size_t i = 0; i < FS_REFERENCES_IN_CLUSTER; i++)
        {
            if (dir->ref[i].name[0] != 0)
            {
                if (*count >= max_results) return FS_BUFFER_TOO_SMALL;
                
                strcpy(results[*count].name, dir->ref[i].name);
                results[*count].node = dir->ref[i].node;
                
                _fs_node_t entry_node_data;
                FS_CHECK_ERROR(_fs_read_node(fs, dir->ref[i].node, &entry_node_data));
                
                results[*count].type = entry_node_data.type == FS_NODE_TYPE_FILE ? FS_FILE : FS_DIR;
                
                (*count)++;
            }
        }
        
        FS_CHECK_ERROR(_fs_read_state(fs, current_cluster, &current_cluster));
    }
    while (current_cluster != FS_CLUSTER_EOF);
    
    return FS_OK;
}

int fs_file_open(fs_t* fs, const char* path, uint8_t flags, fs_file_t* result)
{
    size_t len = strlen(path);
    if (len > 255) return FS_PATH_TOO_LONG;
    if (*(path + len - 1) == '/') return FS_WRONG_PATH;
    
    uint8_t status;
    FS_CHECK_ERROR(_fs_find_node(fs, path, &result->node, &status));
    
    if (status == FS_FIND_DIR) return FS_NOT_A_FILE;
    else if (status == FS_FIND_NOT_EXISTS)
    {
        if (!(flags & FS_CREATE)) return FS_FILE_NOT_EXISTS;
        
        // create file
        char dirpath[256];
        char filename[FS_NAME_MAX_LENGTH + 1];
        
        strcpy(dirpath, path);
        char* separator = strrchr(dirpath, '/');
        
        strcpy(filename, separator + 1);
        
        *(separator + 1) = 0;
        
        if (strlen(filename) > FS_NAME_MAX_LENGTH) return FS_NAME_TOO_LONG;
        
        uint32_t dir_node;
        uint8_t dir_status;
        FS_CHECK_ERROR(_fs_find_node(fs, path, &dir_node, &dir_status));
        
        // previous _fs_find_node finished successfully so dir_node for sure is a directory
        
        FS_CHECK_ERROR(_fs_create_node(fs, &result->node));
        
        _fs_node_t node_data;
        FS_CHECK_ERROR(_fs_read_node(fs, result->node, &node_data));
        node_data.type = FS_NODE_TYPE_FILE;
        node_data.links_count = 1;
        node_data.size = 0;
        node_data.modification_time = (uint32_t)time(NULL);
        
        FS_CHECK_ERROR(_fs_find_free_cluster(fs, &node_data.cluster_index));
        FS_CHECK_ERROR(_fs_write_state(fs, node_data.cluster_index, FS_CLUSTER_EOF));
        
        FS_CHECK_ERROR(_fs_write_node(fs, result->node, &node_data));
        
        FS_CHECK_ERROR(_fs_dir_add_entry(fs, dir_node, filename, result->node));
        
        result->pos = 0;
        result->first_cluster = node_data.cluster_index;
        result->current_cluster = node_data.cluster_index;
        result->current_cluster_pos = 0;
        result->size = node_data.size;
    }
    else if (status == FS_FIND_FILE)
    {
        // open existing file
        
        _fs_node_t node_data;
        FS_CHECK_ERROR(_fs_read_node(fs, result->node, &node_data));
        
        result->pos = 0;
        result->first_cluster = node_data.cluster_index;
        result->current_cluster = node_data.cluster_index;
        result->current_cluster_pos = 0;
        
        if (flags & FS_CREATE)
        {
            node_data.size = 0;
            node_data.modification_time = (uint32_t)time(NULL);
            FS_CHECK_ERROR(_fs_write_node(fs, result->node, &node_data));
            
            // free up all clusters except first
            uint32_t cluster_state;
            FS_CHECK_ERROR(_fs_read_state(fs, node_data.cluster_index, &cluster_state));
            while (cluster_state != FS_CLUSTER_EOF)
            {
                uint32_t next_cluster;
                FS_CHECK_ERROR(_fs_read_state(fs, cluster_state, &next_cluster));
                FS_CHECK_ERROR(_fs_write_state(fs, cluster_state, FS_CLUSTER_EMPTY));
                cluster_state = next_cluster;
            }
            
            FS_CHECK_ERROR(_fs_write_state(fs, node_data.cluster_index, FS_CLUSTER_EOF));
        }
        
        result->size = node_data.size;
        
        if (flags & FS_APPEND)
        {
            FS_CHECK_ERROR(fs_file_seek(fs, result, FS_SEEK_END, 0));
        }
    }
    
    result->is_opened = 1;
    
    return FS_OK;
}

int fs_file_write(fs_t* fs, fs_file_t* file, const void* buffer, size_t size, size_t* written)
{
    *written = 0;
    
    const uint8_t* byte_buffer = (const uint8_t*)buffer;
    
    while (size)
    {
        uint32_t remaining_in_cluster = FS_SECTOR_SIZE - file->current_cluster_pos;
        if (size < remaining_in_cluster) remaining_in_cluster = size;
        
        if (remaining_in_cluster > 0)
        {
            size_t disk_pos = FS_SECTOR_POS(_fs_cluster_to_sector(fs, file->current_cluster));
            disk_pos += file->current_cluster_pos;
    
            FS_CHECK_ERROR(_fs_write_disk(fs, byte_buffer, disk_pos, remaining_in_cluster));
    
            size -= remaining_in_cluster;
            file->current_cluster_pos += remaining_in_cluster;
            file->pos += remaining_in_cluster;
            byte_buffer += remaining_in_cluster;
            *written += remaining_in_cluster;
        }

        if (size)
        {
            // obtain new cluster if there are remaining bytes to write
            
            uint32_t cluster_state;
            FS_CHECK_ERROR(_fs_read_state(fs, file->current_cluster, &cluster_state));
            
            if (cluster_state == FS_CLUSTER_EOF)
            {
                // allocate new cluster
                uint32_t new_cluster;
                FS_CHECK_ERROR(_fs_find_free_cluster(fs, &new_cluster));
                
                FS_CHECK_ERROR(_fs_write_state(fs, new_cluster, FS_CLUSTER_EOF));
                FS_CHECK_ERROR(_fs_write_state(fs, file->current_cluster, new_cluster));
                
                file->current_cluster = new_cluster;
                file->current_cluster_pos = 0;
            }
            else
            {
                // switch to next cluster
                file->current_cluster = cluster_state;
                file->current_cluster_pos = 0;
            }
        }
    }
    
    if (file->pos > file->size) file->size = file->pos;
}

int fs_file_read(fs_t* fs, fs_file_t* file, void* buffer, size_t size, size_t* read)
{
    if (file->pos >= file->size) return FS_EOF;
    
    if (file->pos + size > file->size) size = file->size - file->pos;
    
    *read = 0;
    
    uint8_t* byte_buffer = (uint8_t*)buffer;
    
    while (size)
    {
        uint32_t remaining_in_cluster = FS_SECTOR_SIZE - file->current_cluster_pos;
        if (size < remaining_in_cluster) remaining_in_cluster = size;
        
        if (remaining_in_cluster > 0)
        {
            size_t disk_pos = FS_SECTOR_POS(_fs_cluster_to_sector(fs, file->current_cluster));
            disk_pos += file->current_cluster_pos;
            
            FS_CHECK_ERROR(_fs_read_disk(fs, byte_buffer, disk_pos, remaining_in_cluster));
            
            size -= remaining_in_cluster;
            file->current_cluster_pos += remaining_in_cluster;
            file->pos += remaining_in_cluster;
            byte_buffer += remaining_in_cluster;
            *read += remaining_in_cluster;
        }
        
        if (file->pos > file->size) return FS_EOF;
        
        if (size)
        {
            // obtain new cluster if there are remaining bytes to read
            
            uint32_t cluster_state;
            FS_CHECK_ERROR(_fs_read_state(fs, file->current_cluster, &cluster_state));
            
            if (cluster_state == FS_CLUSTER_EOF) return FS_EOF;
            
            // switch to next cluster
            file->current_cluster = cluster_state;
            file->current_cluster_pos = 0;
        }
    }
}

int fs_file_seek(fs_t* fs, fs_file_t* file, uint8_t mode, int32_t pos)
{
    switch (mode)
    {
        case FS_SEEK_CURRENT: pos = file->pos + pos; break;
        case FS_SEEK_END: pos = file->size - pos; break;
    }
    
    if (pos < 0) return FS_EOF;
    if (pos > file->size) return FS_EOF;
    
    uint32_t clusters_to_skip = pos / FS_SECTOR_SIZE;
    uint32_t current_cluster = file->first_cluster;
    while (clusters_to_skip)
    {
        uint32_t next_cluster;
        FS_CHECK_ERROR(_fs_read_state(fs, current_cluster, &next_cluster));
        if (next_cluster == FS_CLUSTER_EOF) return FS_EOF;
        
        current_cluster = next_cluster;
        
        clusters_to_skip--;
    }
    
    file->current_cluster = current_cluster;
    file->current_cluster_pos = pos % FS_SECTOR_SIZE;
    file->pos = pos;
    
    return FS_OK;
}

int fs_file_close(fs_t* fs, fs_file_t* file)
{
    if (!file->is_opened) return FS_FILE_ALREADY_CLOSED;
    
    _fs_node_t node_data;
    FS_CHECK_ERROR(_fs_read_node(fs, file->node, &node_data));
    
    node_data.size = file->size;
    node_data.modification_time = (uint32_t)time(NULL);
    
    FS_CHECK_ERROR(_fs_write_node(fs, file->node, &node_data));
    
    file->is_opened = 0;
    
    return FS_OK;
}

int fs_link(fs_t* fs, const char* path, uint32_t node)
{
    
}

static int _fs_find_free_cluster(fs_t* fs, uint32_t* result)
{
    uint32_t current_table_sector_index = 0xFFFFFFFF;
    _fs_table_sector_t* table_sector = (_fs_table_sector_t*)fs->buffer;
    
    for (uint32_t i = 0; i < fs->clusters_count; i++)
    {
        uint32_t required_table_sector_index = i / FS_STATES_IN_SECTOR;
        uint32_t array_index = i % FS_STATES_IN_SECTOR;
        
        if (current_table_sector_index != required_table_sector_index)
        {
            uint32_t final_table_sector_index = required_table_sector_index + fs->table_sector_start;
            FS_CHECK_ERROR(_fs_read_sector_buffer(fs, final_table_sector_index));
            
            current_table_sector_index = required_table_sector_index;
        }
        
        uint32_t cluster_state = table_sector->state[array_index];
        if (cluster_state == FS_CLUSTER_EMPTY)
        {
            *result = i;
            return FS_OK;
        }
    }
    
    return FS_FULL;
}

static int _fs_create_node(fs_t* fs, uint32_t* result_node_number)
{
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
            FS_CHECK_ERROR(_fs_read_sector_buffer(fs, final_table_sector_index));
            
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
            FS_CHECK_ERROR(_fs_write_state(fs, node_cluster_index, cluster_state));
            
            break;
        }
    }
    
    _fs_node_cluster_t* node_cluster = (_fs_node_cluster_t*)fs->buffer;
    if (node_cluster_index != FS_CLUSTER_INVALID)
    {
        // find free place for node in cluster
        uint32_t node_sector_index = _fs_cluster_to_sector(fs, node_cluster_index);
        FS_CHECK_ERROR(_fs_read_sector_buffer(fs, node_sector_index));
    
        for (uint8_t i = 0; i < FS_NODES_IN_CLUSTER; i++)
        {
            if (!(node_cluster->nodes[i].flags & FS_NODE_FLAGS_INUSE))
            {
                // found free place
                _fs_node_t node;
                memset(&node, 0, sizeof(_fs_node_t));
                node.flags |= FS_NODE_FLAGS_INUSE;
                
                *result_node_number = (node_cluster_index << 8) | i;
                
                FS_CHECK_ERROR(_fs_write_node(fs, *result_node_number, &node));
                
                return FS_OK;
            }
        }
    }
    else if (first_empty_cluster_index != FS_CLUSTER_INVALID)
    {
        // no node sector with free places found, start new cluster
        uint32_t sector_index = _fs_cluster_to_sector(fs, first_empty_cluster_index);
        
        FS_CHECK_ERROR(_fs_write_state(fs, first_empty_cluster_index, FS_CLUSTER_NODE_BEGIN + 1));
        
        memset(fs->buffer, 0, FS_SECTOR_SIZE);
        node_cluster->nodes[0].flags |= FS_NODE_FLAGS_INUSE;
        
        FS_CHECK_ERROR(_fs_write_sector_buffer(fs, sector_index));
        
        *result_node_number = first_empty_cluster_index << 8;
        
        return FS_OK;
    }
    
    // place for new node not found - file system is full
    return FS_FULL;
}

static int _fs_create_dir(fs_t* fs, uint32_t node, uint32_t parent_node, uint32_t* result_cluster)
{
    FS_CHECK_ERROR(_fs_find_free_cluster(fs, result_cluster));
    
    FS_CHECK_ERROR(_fs_write_state(fs, *result_cluster, FS_CLUSTER_EOF));
    
    memset(fs->buffer, 0, FS_SECTOR_SIZE);
    _fs_dir_cluster_t* dir = (_fs_dir_cluster_t*)fs->buffer;
    strcpy(dir->ref[0].name, ".");
    dir->ref[0].node = node;
    
    strcpy(dir->ref[1].name, "..");
    dir->ref[1].node = parent_node;
    
    FS_CHECK_ERROR(_fs_write_cluster_buffer(fs, *result_cluster));
    
    return 0;
}

static int _fs_dir_find_entry(fs_t* fs, uint32_t dir_node, const char* entry_name, uint8_t* result_code, uint32_t* result_node)
{
    _fs_node_t node_data;
    
    FS_CHECK_ERROR(_fs_read_node(fs, dir_node, &node_data));
    
    if (node_data.type != FS_NODE_TYPE_DIR) return FS_NOT_A_DIRECTORY;
    
    uint32_t current_cluster = node_data.cluster_index;
    _fs_dir_cluster_t* dir = (_fs_dir_cluster_t*)fs->buffer;
    do
    {
        FS_CHECK_ERROR(_fs_read_cluster_buffer(fs, current_cluster));
        
        for (size_t i = 0; i < FS_REFERENCES_IN_CLUSTER; i++)
        {
            if (strcmp(dir->ref[i].name, entry_name) == 0)
            {
                _fs_node_t entry_node_data;
                FS_CHECK_ERROR(_fs_read_node(fs, dir->ref[i].node, &entry_node_data));
            
                switch (entry_node_data.type)
                {
                    case FS_NODE_TYPE_FILE: *result_code = FS_FIND_FILE; break;
                    case FS_NODE_TYPE_DIR: *result_code = FS_FIND_DIR; break;
                }
                
                *result_node = dir->ref[i].node;
            
                return FS_OK;
            }
        }
        
        FS_CHECK_ERROR(_fs_read_state(fs, current_cluster, &current_cluster));
    } while (current_cluster != FS_CLUSTER_EOF);
    
    *result_code = FS_FIND_NOT_EXISTS;
    return FS_OK;
}

static int _fs_dir_add_entry(fs_t* fs, uint32_t dir_node, const char* entry_name, uint32_t entry_node)
{
    _fs_node_t node_data;
    
    FS_CHECK_ERROR(_fs_read_node(fs, dir_node, &node_data));
    
    if (node_data.type != FS_NODE_TYPE_DIR) return FS_NOT_A_DIRECTORY;
    
    uint32_t current_cluster = node_data.cluster_index;
    uint32_t prev_cluster = FS_CLUSTER_INVALID;
    _fs_dir_cluster_t* dir = (_fs_dir_cluster_t*)fs->buffer;
    do
    {
        FS_CHECK_ERROR(_fs_read_cluster_buffer(fs, current_cluster));
        
        for (size_t i = 0; i < FS_REFERENCES_IN_CLUSTER; i++)
        {
            if (dir->ref[i].name[0] == 0)
            {
                // found free entry
                strcpy(dir->ref[i].name, entry_name);
                dir->ref[i].node = entry_node;
                
                FS_CHECK_ERROR(_fs_write_cluster_buffer(fs, current_cluster));
                
                return FS_OK;
            }
        }
        
        prev_cluster = current_cluster;
        FS_CHECK_ERROR(_fs_read_state(fs, current_cluster, &current_cluster));
    } while (current_cluster != FS_CLUSTER_EOF);
    
    // all entries in directory clusters are occupied
    // allocate next cluster
    
    uint32_t new_cluster;
    FS_CHECK_ERROR(_fs_find_free_cluster(fs, &new_cluster));
    
    FS_CHECK_ERROR(_fs_write_state(fs, prev_cluster, new_cluster)); // link to next cluster
    FS_CHECK_ERROR(_fs_write_state(fs, new_cluster, FS_CLUSTER_EOF));
    
    memset(fs->buffer, 0, FS_SECTOR_SIZE);
    strcpy(dir->ref[0].name, entry_name);
    dir->ref[0].node = entry_node;
    
    FS_CHECK_ERROR(_fs_write_cluster_buffer(fs, new_cluster));
    
    return FS_OK;
}

static int _fs_find_node(fs_t* fs, const char* path, uint32_t* result_node, uint8_t* result_code)
{
    if (path[0] != '/') return FS_WRONG_PATH;
    
    *result_node = fs->root_node;
    
    if (strlen(path) > 255) return FS_PATH_TOO_LONG;
    
    char pathBuffer[256];
    strcpy(pathBuffer, path);
    char* name = strtok(pathBuffer, "/");
    while (name != NULL)
    {
        if (strlen(name) > FS_NAME_MAX_LENGTH) return FS_NAME_TOO_LONG;
        
        uint8_t find_status;
        uint32_t find_node;
        FS_CHECK_ERROR(_fs_dir_find_entry(fs, *result_node, name, &find_status, &find_node));
        
        name = strtok(NULL, "/");
        
        if (name != NULL)
        {
            // element in the middle of path should be directory
            if (find_status != FS_FIND_DIR) return FS_NOT_A_DIRECTORY;
        }
        else
        {
            *result_code = find_status;
        }
        
        *result_node = find_node;
    }
    
    return FS_OK;
}

static uint32_t _fs_cluster_to_sector(fs_t* fs, uint32_t cluster)
{
    return fs->clusters_sector_start + cluster;
}
static size_t _fs_cluster_state_pos(fs_t* fs, uint32_t cluster)
{
    return FS_SECTOR_POS(fs->table_sector_start) + cluster * sizeof(uint32_t);
}
static size_t _fs_node_pos(fs_t* fs, uint32_t node_number)
{
    size_t index = node_number & 0x000000FF;
    uint32_t cluster = node_number >> 8;
    uint32_t sector = _fs_cluster_to_sector(fs, cluster);
    
    return FS_SECTOR_POS(sector) + index * sizeof(_fs_node_t);
}

static int _fs_write_state(fs_t* fs, uint32_t cluster, uint32_t new_state)
{
    size_t pos = _fs_cluster_state_pos(fs, cluster);
    
    return _fs_write_disk(fs, &new_state, pos, sizeof(uint32_t));
}
static int _fs_write_node(fs_t* fs, uint32_t node_number, const _fs_node_t* node_data)
{
    size_t pos = _fs_node_pos(fs, node_number);
    
    return _fs_write_disk(fs, node_data, pos, sizeof(_fs_node_t));
}
static int _fs_write_cluster_buffer(fs_t* fs, uint32_t cluster) // uses fs->buffer
{
    size_t sector_index = _fs_cluster_to_sector(fs, cluster);
    
    return _fs_write_sector_buffer(fs, sector_index);
}
static int _fs_write_sector_buffer(fs_t* fs, size_t sector_index) // uses fs->buffer
{
    return _fs_write_disk_buffer(fs, FS_SECTOR_POS(sector_index), FS_SECTOR_SIZE);
}
static int _fs_write_disk_buffer(fs_t* fs, size_t position, size_t size) // uses fs->buffer
{
    return _fs_write_disk(fs, fs->buffer, position, size);
}
static int _fs_write_disk(fs_t* fs, const void* buffer, size_t position, size_t size)
{
    return fs->operations.write(fs->state, buffer, position, size);
}

static int _fs_read_state(fs_t* fs, uint32_t cluster, uint32_t* result_state)
{
    size_t pos = _fs_cluster_state_pos(fs, cluster);
    
    return _fs_read_disk(fs, result_state, pos, sizeof(uint32_t));
}
static int _fs_read_node(fs_t* fs, uint32_t node_number, _fs_node_t* node_data)
{
    size_t pos = _fs_node_pos(fs, node_number);
    
    return _fs_read_disk(fs, node_data, pos, sizeof(_fs_node_t));
}
static int _fs_read_cluster_buffer(fs_t* fs, uint32_t cluster) // uses fs->buffer
{
    size_t sector_index = _fs_cluster_to_sector(fs, cluster);
    
    return _fs_read_sector_buffer(fs, sector_index);
}
static int _fs_read_sector_buffer(fs_t* fs, size_t sector_index) // uses fs->buffer
{
    return _fs_read_disk_buffer(fs, FS_SECTOR_POS(sector_index), FS_SECTOR_SIZE);
}
static int _fs_read_disk_buffer(fs_t* fs, size_t position, size_t size) // uses fs->buffer
{
    return _fs_read_disk(fs, fs->buffer, position, size);
}
static int _fs_read_disk(fs_t* fs, void* buffer, size_t position, size_t size)
{
    return fs->operations.read(fs->state, buffer, position, size);
}