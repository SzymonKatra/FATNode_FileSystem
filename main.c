#define FILE_NAME "disk.fs"

#include <stdio.h>
#include "fs.h"

int real_init(void ** result_state);
int real_read(void* state, void* buffer, size_t position, size_t size);
int real_write(void* state, const void* buffer, size_t position, size_t size);
int real_close(void* state);

int main()
{
    fs_disk_operations_t operations;
    operations.init = &real_init;
    operations.read = &real_read;
    operations.write = &real_write;
    operations.close = &real_close;
    
    fs_t filesystem;
    
    if (fs_create(&operations, 16 * 1024, &filesystem) != FS_OK)
    {
        printf("Error while creating filesystem!\n");
        return -1;
    }
    int e;
    e = fs_mkdir(&filesystem, "/dupa");printf("%d\n", e);
    fs_mkdir(&filesystem, "/aaaa");
    fs_mkdir(&filesystem, "/bbbb");
    fs_mkdir(&filesystem, "/cccc");
    fs_mkdir(&filesystem, "/cccc/c");
    fs_mkdir(&filesystem, "/cccc/d");
    
    uint32_t cnt;
    fs_dir_entries_count(&filesystem, "/", &cnt);
    printf("%d", cnt);
    
    for (int i = 0; i < 128; i++)
    {
        
        //printf("%d\n", fs_file(&filesystem));
    }
    
    fs_close(&filesystem);
    
    return 0;
}

int real_init(void** result_state)
{
    FILE* file = fopen(FILE_NAME, "w+");
    
    if (file == NULL) return FS_DISK_INIT_ERROR;
    
    *result_state = file;
    
    return FS_OK;
}
int real_read(void* state, void* buffer, size_t position, size_t size)
{
    FILE* file = (FILE*)state;
    
    fseek(file, position, SEEK_SET);
    size_t read = fread(buffer, 1, size, file);
    
    if (read != size) return FS_DISK_READ_ERROR;
    
    return FS_OK;
}
int real_write(void* state, const void* buffer, size_t position, size_t size)
{
    FILE* file = (FILE*)state;
    
    fseek(file, position, SEEK_SET);
    size_t written = fwrite(buffer, 1, size, file);

    if (written != size) return FS_DISK_WRITE_ERROR;
    
    return FS_OK;
}
int real_close(void* state)
{
    FILE* file = (FILE*)state;
    
    int result = fclose(file);
    
    if (result != 0) return FS_DISK_CLOSE_ERROR;
    
    return FS_OK;
}