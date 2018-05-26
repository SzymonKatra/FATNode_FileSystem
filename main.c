#define FILE_NAME "disk.fs"

#include <stdio.h>
#include <string.h>
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
    fs_mkdir(&filesystem, "/dupa");
    fs_mkdir(&filesystem, "/aaaa");
    fs_mkdir(&filesystem, "/bbbb");
    fs_mkdir(&filesystem, "/cccc");
    fs_mkdir(&filesystem, "/cccc/c/");
    fs_mkdir(&filesystem, "/cccc/d");
    
    uint32_t cnt;
    fs_dir_entries_count(&filesystem, "/", &cnt);
    printf("%d\n", cnt);
    fs_dir_entries_count(&filesystem, "/cccc", &cnt);
    printf("%d\n", cnt);
    fs_dir_entries_count(&filesystem, "/cccc/c", &cnt);
    printf("%d\n", cnt);
    
    fs_file_t f;
    fs_file_open(&filesystem, "/cccc/ssss", FS_CREATE, &f);
    
    size_t written, read;
    char buffer[555];
    memset(buffer, 0xEB, 555);
    fs_file_write(&filesystem, &f, buffer, 555, &written);
    fs_file_close(&filesystem, &f);
    
    printf("bytes written: %d\n", written);
    
    fs_file_open(&filesystem, "/cccc/ssss", FS_CREATE, &f);
    fs_file_close(&filesystem, &f);
    
    memset(buffer, 0xAB, 134);
    
    fs_file_open(&filesystem, "/aaaa/x", FS_CREATE, &f);
    fs_file_write(&filesystem, &f, buffer, 35, &written);
    fs_file_close(&filesystem, &f);
    
    memset(buffer, 0xFF, 100);
    
    fs_file_open(&filesystem, "/cccc/xddddd", FS_CREATE, &f);
    fs_file_write(&filesystem, &f, buffer, 3, &written);
    fs_file_close(&filesystem, &f);
    
    memset(buffer, 0xCD, 134);
    
    fs_file_open(&filesystem, "/aaaa/x", FS_APPEND, &f);
    fs_file_write(&filesystem, &f, buffer, 134, &written);
    fs_file_close(&filesystem, &f);
    
    printf("bytes written: %d\n", written);
    
    fs_file_open(&filesystem, "/aaaa/x", 0, &f);
    fs_file_read(&filesystem, &f, buffer, 40, &read);
    fs_file_close(&filesystem, &f);
    
    printf("bytes read: %d\n", read);
    
    printf("er %d\n", fs_dir_entries_count(&filesystem, "/cccc/ssss", &cnt));
    printf("%d\n", cnt);
    
    fs_dir_entry_t entries[256];
    size_t count;
    
    fs_dir_list(&filesystem, "/cccc", entries, &count, 256);
    for (size_t i = 0; i < count; i++) printf("%s %d %d\n", entries[i].name, entries[i].node, entries[i].type);
    
    
    
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