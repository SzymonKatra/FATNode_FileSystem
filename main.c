#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fs.h"
#include "parser.h"

#define MAX_COMMAND_LEN     255
#define MAX_COMMAND_ARGS    10
#define MAX_DIR_ENTRIES     255

int real_init_create(void ** result_state);
int real_init_open(void ** result_state);
int real_read(void* state, void* buffer, size_t position, size_t size);
int real_write(void* state, const void* buffer, size_t position, size_t size);
int real_close(void* state);

const char* filename;
fs_disk_operations_t operations;
fs_t fs;
char cmd[MAX_COMMAND_LEN];
char* args[MAX_COMMAND_ARGS];
fs_dir_entry_t entries[MAX_DIR_ENTRIES];
char current_dir[255];

void init(int argc, char** argv);
void cleanup();
void cmd_mkdir(const char* path);
void cmd_ls(const char* path);
void cmd_cd(const char* path);
void cmd_pwd();

void print_fs_error(int fs_error_code);

int main(int argc, char** argv)
{    
    init(argc, argv);
    
    puts("Type help to get more information");

    while (1)
    {
        printf("$ ");
        fgets(cmd, MAX_COMMAND_LEN, stdin);
        size_t args_count = parser_parse(cmd, args, MAX_COMMAND_ARGS);
        
        if (args_count == 0) continue;
        
        if (args[0][0] == 0)
        {
            continue;
        }
        else if (strcmp(args[0], "cp") == 0)
        {
            
        }
        else if (strcmp(args[0], "mv") == 0)
        {
            
        }
        else if (strcmp(args[0], "mkdir") == 0)
        {
            if (args_count < 2)
            {
                puts("mkdir requires 1 argument");
                continue;
            }
            
            cmd_mkdir(args[1]);
        }
        else if (strcmp(args[0], "ln") == 0)
        {
            
        }
        else if (strcmp(args[0], "rm") == 0)
        {
            
        }
        else if (strcmp(args[0], "import") == 0)
        {
            
        }
        else if (strcmp(args[0], "export") == 0)
        {
            
        }
        else if (strcmp(args[0], "exp") == 0)
        {
        }
        else if (strcmp(args[0], "trunc") == 0)
        {
            
        }
        else if (strcmp(args[0], "cd") == 0)
        {
            if (args_count < 2)
            {
                puts("cd requires 1 argument");
                continue;
            }
            
            cmd_cd(args[1]);
        }
        else if (strcmp(args[0], "ls") == 0)
        {
            if (args_count < 2)
            {
                cmd_ls(current_dir);
            }
            else
            {
                cmd_ls(args[1]);
            }
        }
        else if (strcmp(args[0], "pwd") == 0)
        {
            cmd_pwd();
        }
        else if (strcmp(args[0], "fsinfo") == 0)
        {
            
        }
        else if (strcmp(args[0], "help") == 0)
        {
            
        }
        else if (strcmp(args[0], "exit") == 0)
        {
            break;
        }
        else
        {
            puts("Unknown command. Type help to get more information");
        }
    }
    
    cleanup();
    
    return 0;
}

void init(int argc, char** argv)
{
    if (argc < 2) exit(-1);
    
    filename = argv[1];
    
    operations.read = &real_read;
    operations.write = &real_write;
    operations.close = &real_close;
    
    if (argc >= 3)
    {
        operations.init = &real_init_create;
        if (fs_create(&operations, atoi(argv[2]), &fs) != FS_OK)
        {
            puts("Error occurred while creating filesystem");
            exit(-1);
        }
        
        puts("Filesystem successfully created");
    }
    else
    {
        operations.init = &real_init_open;
        if (fs_open(&operations, &fs) != FS_OK)
        {
            puts("Error occured while opening filesystem");
            exit(-1);
        }
        
        puts("Filesystem successfully opened");
    }
    
    strcpy(current_dir, "/");
}

void cleanup()
{
    if (fs_close(&fs) != FS_OK)
    {
        puts("Error occured while closing filesystem");
        exit(-1);
    }
    
    puts("Filesystem successfully closed");
}

void cmd_mkdir(const char* path)
{
    int result = fs_mkdir(&fs, path);
    
    if (result != FS_OK) print_fs_error(result);
}

void cmd_ls(const char* path)
{
    size_t count;
    int result = fs_dir_list(&fs, path, entries, &count, MAX_DIR_ENTRIES);
    if (result != FS_OK)
    {
        print_fs_error(result);
        return;
    }
    
    for (size_t i = 0; i < count; i++)
    {
        puts(entries[i].name);
    }
}

void cmd_cd(const char* path)
{
    char tmp_path[255];
    strcpy(tmp_path, path);
    char* tmp_tokens[10];
    size_t tokens_count = 0;
    char* token = strtok(tmp_path, "/");
    while (token != NULL)
    {
        tmp_tokens[tokens_count++] = token;
        token = strtok(NULL, "/");
    }
    
    char tmp_current[255];
    strcpy(tmp_current, current_dir);
    
    for (int i = 0; i < tokens_count; i++)
    {
        token = tmp_tokens[i];
        
        if (strcmp(token, "..") == 0)
        {
            size_t curr_len = strlen(tmp_current);
            if (curr_len > 1)
            {
                *(tmp_current + curr_len - 1) = 0;
            
                char* last_slash = strrchr(tmp_current, '/');
            
                if (last_slash != NULL)
                {
                    *(last_slash + 1) = 0;
                }
            }
        }
        else if (strcmp(token, ".") != 0)
        {
            size_t curr_len = strlen(tmp_current);
            strcpy(tmp_current + curr_len, token);
            curr_len = strlen(tmp_current);
            *(tmp_current + curr_len) = '/';
            *(tmp_current + curr_len + 1) = 0;
            
            int result = fs_entry_info(&fs, tmp_current, &entries[0]);
            
            if (result != FS_OK)
            {
                print_fs_error(result);
                return;
            }
        }
    }
    
    strcpy(current_dir, tmp_current);
}

void cmd_pwd()
{
    puts(current_dir);
}

void print_fs_error(int fs_error_code)
{
    switch (fs_error_code)
    {
        case FS_OK: puts("Ok"); break;
        case FS_DISK_INIT_ERROR: puts("An error occurred while initializing disk"); break;
        case FS_DISK_READ_ERROR: puts("An error occured while reading from the disk"); break;
        case FS_DISK_WRITE_ERROR: puts("An error occurred while writing to the disk"); break;
        case FS_DISK_CLOSE_ERROR: puts("An errror occured while closing the disk"); break;
        case FS_FULL: puts("File system is full"); break;
        case FS_NOT_A_DIRECTORY: puts("Not a directory"); break;
        case FS_WRONG_PATH: puts("Wrong path specified"); break;
        case FS_PATH_TOO_LONG: puts("Path is too long"); break;
        case FS_NAME_TOO_LONG: puts("Name is too long"); break;
        case FS_BUFFER_TOO_SMALL: puts("Buffer is too small to handle result"); break;
        case FS_NOT_A_FILE: puts("Not a file"); break;
        case FS_NOT_EXISTS: puts("Not exists"); break;
        case FS_FILE_ALREADY_CLOSED: puts("File is closed"); break;
        case FS_EOF: puts("End of file"); break;
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