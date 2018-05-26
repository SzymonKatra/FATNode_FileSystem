#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fs.h"

int real_init_create(void ** result_state);
int real_init_open(void ** result_state);
int real_read(void* state, void* buffer, size_t position, size_t size);
int real_write(void* state, const void* buffer, size_t position, size_t size);
int real_close(void* state);

const char* filename;

void init(int argc, char** argv);
void cleanup();

int main(int argc, char** argv)
{
    init(argc, argv);
    
    cleanup();
    
    return 0;
}

void init(int argc, char** argv)
{
    if (argc < 2) return -1;
    
    filename = argv[1];
    
    fs_disk_operations_t operations;
    operations.read = &real_read;
    operations.write = &real_write;
    operations.close = &real_close;
    
    fs_t fs;
    
    if (argc >= 3)
    {
        operations.init = &real_init_create;
        if (fs_create(&operations, atoi(argv[2]), &fs) != FS_OK)
        {
            puts("Error occurred while creating filesystem");
            exit(-1);
        }
    }
    else
    {
        operations.init = &real_init_open;
        if (fs_open(&operations, &fs) != FS_OK)
        {
            puts("Error occured while opening filesystem");
            exit(-1);
        }
    }
}

void cleanup()
{
    if (fs_close(&fs) != FS_OK)
    {
        puts("Error occured while closing filesystem");
        exit(-1);
    }
}

int test()
{
    fs_disk_operations_t operations;
    //operations.init = &real_init_create;
    operations.init = &real_init_open;
    operations.read = &real_read;
    operations.write = &real_write;
    operations.close = &real_close;
    
    fs_t filesystem;
    
    /*if (fs_create(&operations, 16 * 1024, &filesystem) != FS_OK)
    {
        printf("Error while creating filesystem!\n");
        return -1;
    }*/
    //fs_create(&operations, 16 * 1024, &filesystem);
    fs_open(&operations, &filesystem);
    fs_mkdir(&filesystem, "/dupa");
    fs_mkdir(&filesystem, "/aaaa");
    fs_mkdir(&filesystem, "/bbbb");
    fs_mkdir(&filesystem, "/cccc");
    fs_mkdir(&filesystem, "/cccc/c/");
    fs_mkdir(&filesystem, "/cccc/d");
    fs_mkdir(&filesystem, "/cccc/ddvvvvvvvv");
    
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
    
    printf("bytes written: %ld\n", written);
    
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
    
    printf("bytes written: %ld\n", written);
    
    fs_file_open(&filesystem, "/aaaa/x", 0, &f);
    fs_file_read(&filesystem, &f, buffer, 40, &read);
    fs_file_close(&filesystem, &f);
    
    printf("bytes read: %ld\n", read);
    
    const char* tekst = "Wsadzmy do naszego pliku jakis przykladowy tekst....";
    size_t len = strlen(tekst) + 1;
    
    fs_file_open(&filesystem, "/plik1.txt", FS_CREATE, &f);
    fs_file_write(&filesystem, &f, tekst, len, &written);
    fs_file_close(&filesystem, &f);
    
    fs_dir_entry_t entry;
    printf("entryerr: %d\n",fs_entry_info(&filesystem, "/plik1.txt", &entry));
    printf("ENTRY: %d\n", entry.node);
    
    char buf[100];
    fs_file_open(&filesystem, "/plik1.txt", 0, &f);
    fs_file_read(&filesystem, &f, buf, len, &read);
    fs_file_close(&filesystem, &f);
    
    puts(buf);
    memset(buf, 0xAA, 100);
    
    fs_link(&filesystem, "/aaaa/link.dupa", entry.node);
    
    fs_file_open(&filesystem, "/aaaa/link.dupa", 0, &f);
    fs_file_read(&filesystem, &f, buf, len, &read);
    fs_file_close(&filesystem, &f);
    
    puts(buf);
    
    fs_remove(&filesystem, "/aaaa/link.dupa");
    fs_remove(&filesystem, "/plik1.txt");
    fs_remove(&filesystem, "/aaaa/x");
    fs_remove(&filesystem, "/cccc/xddddd");
    fs_remove(&filesystem, "/cccc/ssss");
    fs_remove(&filesystem, "/cccc/c");
    fs_entry_info(&filesystem, "/cccc", &entry);
    fs_link(&filesystem, "/linkujemy", entry.node);
    fs_mkdir(&filesystem, "/dddddddddd");
    fs_remove(&filesystem, "/dddddddddd");
    //fs_remove(&filesystem ,"/cccc");
    fs_remove(&filesystem, "/linkujemy");
    fs_remove(&filesystem, "/d");
    fs_remove(&filesystem, "/aaaa");
    fs_remove(&filesystem, "/cccc");
    fs_remove(&filesystem, "/bbbb");
    fs_remove(&filesystem, "/dupa");
    fs_mkdir(&filesystem, "/a/b/c/d/");
    fs_remove(&filesystem, "/a");
    
    printf("er %d\n", fs_dir_entries_count(&filesystem, "/cccc/ssss", &cnt));
    printf("%d\n", cnt);
    
    fs_dir_entry_t entries[256];
    size_t count;
    
    fs_dir_list(&filesystem, "/", entries, &count, 256);
    for (size_t i = 0; i < count; i++) printf("%s %d %d\n", entries[i].name, entries[i].node, entries[i].type);
    
    
    
    fs_close(&filesystem);
    
    return 0;
}

int real_init_create(void** result_state)
{
    FILE* file = fopen(filename, "w+");
    
    if (file == NULL) return FS_DISK_INIT_ERROR;
    
    *result_state = file;
    
    return FS_OK;
}
int real_init_open(void** result_state)
{
    FILE* file = fopen(filename, "r+");
    
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