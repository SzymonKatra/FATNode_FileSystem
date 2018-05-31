#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "fs.h"

#define MAX_COMMAND_LEN     255
#define MAX_COMMAND_ARGS    10
#define MAX_DIR_ENTRIES     255

#define COLOR_GREEN     	"\x1b[92m"
#define COLOR_CYAN          "\x1b[96m"
#define COLOR_RESET			"\x1b[0m"

#define HANDLE_FS_ERROR(x)  do { int result = x; if (result != FS_OK) { print_fs_error(result); return; } } while(0);

#define TMP_FILENAME        "tmp"

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
char current_dir[FS_PATH_MAX_LENGTH];

size_t parse_input(char* input, char** output, size_t max_outputs);

void init(int argc, char** argv);
int loop();
void cleanup();

void cmd_cp(const char* source, const char* destination);
void cmd_mv(const char* source, const char* destination);
void cmd_mkdir(const char* path);
void cmd_touch(const char* path);
void cmd_ln(const char* destination, const char* link_name);
void cmd_rm(const char* path);
void cmd_import(const char* real_source, const char* destination);
void cmd_export(const char* source, const char* real_destination);
void cmd_edit(const char* path);
void cmd_cat(const char* path);
void cmd_ls(const char* path, int show_details, int show_size);
void cmd_cd(const char* path);
void cmd_pwd();
void cmd_exp(const char* path, size_t count);
void cmd_trunc(const char* path, size_t count);
void cmd_fsinfo();
void cmd_help();

void print_fs_error(int fs_error_code);
void absolute_path(const char* path, char* result);

int main(int argc, char** argv)
{      
    init(argc, argv);

    while (loop());   
    
    cleanup();
    
    return 0;
}

size_t parse_input(char* input, char** output, size_t max_outputs)
{
    size_t len = strlen(input);
    if (input[len - 1] == '\n') input[len - 1] = 0;
    
    size_t current = 0;
    
    char* token = strtok(input, " ");
    while (token != NULL && current < max_outputs)
    {
        output[current++] = token;
        token = strtok(NULL, " ");
    }
    
    return current;
}

void init(int argc, char** argv)
{
    if (argc < 2)
    {
        puts(COLOR_RESET"Usage: ");
        puts("Open existing:    ./fs file_name");
        puts("Create new:       ./fs file_name size_in_bytes");
        exit(-1);
    }
    
    filename = argv[1];
    
    operations.read = &real_read;
    operations.write = &real_write;
    operations.close = &real_close;
    
    printf(COLOR_GREEN);
    
    if (argc >= 3)
    {
        operations.init = &real_init_create;
        if (fs_create(&operations, atoi(argv[2]), &fs) != FS_OK)
        {
            puts("Error occurred while creating file system.");
            exit(-1);
        }
        
        puts("File system successfully created.");
    }
    else
    {
        operations.init = &real_init_open;
        if (fs_open(&operations, &fs) != FS_OK)
        {
            puts("Error occured while opening file system.");
            exit(-1);
        }
        
        puts("File system successfully opened.");
    }
    
    strcpy(current_dir, "/");
    
    puts("Type help to get more information");
}

int loop()
{
    printf(COLOR_RESET);
    printf(COLOR_CYAN"%s"COLOR_RESET"$ ", current_dir);
    fgets(cmd, MAX_COMMAND_LEN, stdin);
    size_t args_count = parse_input(cmd, args, MAX_COMMAND_ARGS);
    
    if (args_count == 0) return 1;
    
    printf(COLOR_GREEN);
    
    if (args[0][0] == 0)
    {
        return 1;
    }
    else if (strcmp(args[0], "cp") == 0)
    {
        if (args_count < 3)
        {
            puts("cp requires 2 arguments");
            return 1;
        }
        
        cmd_cp(args[1], args[2]);
    }
    else if (strcmp(args[0], "mv") == 0)
    {
        if (args_count < 3)
        {
            puts("mv requires 2 arguments");
            return 1;
        }
        
        cmd_mv(args[1], args[2]);
    }
    else if (strcmp(args[0], "mkdir") == 0)
    {
        if (args_count < 2)
        {
            puts("mkdir requires 1 argument");
            return 1;
        }
        
        cmd_mkdir(args[1]);
    }
    else if (strcmp(args[0], "touch") == 0)
    {
        if (args_count < 2)
        {
            puts("touch required 1 argument");
            return 1;
        }
        
        cmd_touch(args[1]);
    }
    else if (strcmp(args[0], "ln") == 0)
    {
        if (args_count < 3)
        {
            puts("ln required 2 arguments");
            return 1;
        }
        
        cmd_ln(args[1], args[2]);
    }
    else if (strcmp(args[0], "rm") == 0)
    {
        if (args_count < 2)
        {
            puts("rm required 1 argument");
            return 1;
        }
        
        cmd_rm(args[1]);
    }
    else if (strcmp(args[0], "import") == 0)
    {
        if (args_count < 3)
        {
            puts("import requires 2 arguments");
            return 1;
        }
        
        cmd_import(args[1], args[2]);
    }
    else if (strcmp(args[0], "export") == 0)
    {
        if (args_count < 3)
        {
            puts("export requires 2 arguments");
            return 1;
        }
        
        cmd_export(args[1], args[2]);
    }
    else if (strcmp(args[0], "edit") == 0)
    {
        if (args_count < 2)
        {
            puts("edit requires 1 argument");
            return 1;
        }
        
        cmd_edit(args[1]);
    }
    else if (strcmp(args[0], "cat") == 0)
    {
        if (args_count < 2)
        {
            puts("cat requires 1 argument");
            return 1;
        }
        
        cmd_cat(args[1]);
    }
    else if (strcmp(args[0], "exp") == 0)
    {
        if (args_count < 3)
        {
            puts("exp requires 2 arguments");
            return 1;
        }
        
        cmd_exp(args[1], atoi(args[2]));
    }
    else if (strcmp(args[0], "trunc") == 0)
    {
        if (args_count < 3)
        {
            puts("trunc requires 2 arguments");
            return 1;
        }
        
        cmd_trunc(args[1], atoi(args[2]));
    }
    else if (strcmp(args[0], "cd") == 0)
    {
        if (args_count < 2)
        {
            puts("cd requires 1 argument");
            return 1;
        }
        
        cmd_cd(args[1]);
    }
    else if (strcmp(args[0], "ls") == 0)
    {
        char* path_arg = NULL;
        int details = 0;
        int size = 0;
        for (size_t i = 1; i < args_count; i++)
        {
            if (args[i][0] == '-')
            {
                if (strchr(args[i], 'd') != NULL) details = 1;
                if (strchr(args[i], 's') != NULL) size = 1;
            }
            else
            {
                if (path_arg != NULL)
                {
                    puts("Too many arguments specified");
                }
                else
                {
                    path_arg = args[i];
                }
            }
        }
        
        if (path_arg == NULL) path_arg = current_dir;
        
        cmd_ls(path_arg, details, size);
    }
    else if (strcmp(args[0], "pwd") == 0)
    {
        cmd_pwd();
    }
    else if (strcmp(args[0], "fsinfo") == 0)
    {
        cmd_fsinfo();
    }
    else if (strcmp(args[0], "help") == 0)
    {
        cmd_help();
    }
    else if (strcmp(args[0], "exit") == 0)
    {
        return 0;
    }
    else
    {
        puts("Unknown command. Type help to get more information.");
    }
        
    return 1;
}

void cleanup()
{
    printf(COLOR_GREEN);
    
    if (fs_close(&fs) != FS_OK)
    {
        puts("Error occured while closing file system.");
        exit(-1);
    }
    
    puts("File system successfully closed.");
}

void cmd_cp(const char* source, const char* destination)
{
    char src_path[FS_PATH_MAX_LENGTH];
    char dst_path[FS_PATH_MAX_LENGTH];
    absolute_path(source, src_path);
    absolute_path(destination, dst_path);
    
    fs_file_t file;
    HANDLE_FS_ERROR(fs_file_open(&fs, src_path, 0, &file));
    
    fs_file_t dst_file;
    HANDLE_FS_ERROR(fs_file_open(&fs, dst_path, FS_CREATE, &dst_file));

    size_t read;
    size_t written;
    char buffer[256];
    int err = fs_file_read(&fs, &file, buffer, 256, &read);
    while (err != FS_EOF)
    {
        if (err != FS_OK)
        {
            print_fs_error(err);
            return;
        }
        
        HANDLE_FS_ERROR(fs_file_write(&fs, &dst_file, buffer, read, &written));
        
        err = fs_file_read(&fs, &file, buffer, 256, &read);
    }
    
    HANDLE_FS_ERROR(fs_file_close(&fs, &file));
    HANDLE_FS_ERROR(fs_file_close(&fs, &dst_file));
}

void cmd_mv(const char* source, const char* destination)
{
    char src_path[FS_PATH_MAX_LENGTH];
    char dst_path[FS_PATH_MAX_LENGTH];
    absolute_path(source, src_path);
    absolute_path(destination, dst_path);
    
    HANDLE_FS_ERROR(fs_entry_info(&fs, src_path, &entries[0]));
    HANDLE_FS_ERROR(fs_link(&fs, dst_path, entries[0].node));
    HANDLE_FS_ERROR(fs_remove(&fs, src_path));
}

void cmd_mkdir(const char* path)
{
    char final_path[FS_PATH_MAX_LENGTH];
    absolute_path(path, final_path);
    
    HANDLE_FS_ERROR(fs_mkdir(&fs, final_path));
}

void cmd_touch(const char* path)
{
    char final_path[FS_PATH_MAX_LENGTH];
    absolute_path(path, final_path);
    
    fs_file_t file;
    HANDLE_FS_ERROR(fs_file_open(&fs, final_path, FS_CREATE, &file));
    HANDLE_FS_ERROR(fs_file_close(&fs, &file));
}

void cmd_ln(const char* destination, const char* link_name)
{
    char dst_path[FS_PATH_MAX_LENGTH];
    char link[FS_PATH_MAX_LENGTH];
    absolute_path(destination, dst_path);
    absolute_path(link_name, link);
    
    HANDLE_FS_ERROR(fs_entry_info(&fs, dst_path, &entries[0]));
    HANDLE_FS_ERROR(fs_link(&fs, link, entries[0].node));
}

void cmd_rm(const char* path)
{
    char final_path[FS_PATH_MAX_LENGTH];
    absolute_path(path, final_path);
    
    HANDLE_FS_ERROR(fs_remove(&fs, final_path));
}

void cmd_import(const char* real_source, const char* destination)
{
    char dst_path[FS_PATH_MAX_LENGTH];
    absolute_path(destination, dst_path);
    
    FILE* real_file = fopen(real_source, "r");
    if (real_file == NULL)
    {
        printf("Cannot open external file %s\n", real_source);
        return;
    }
    
    fs_file_t file;
    HANDLE_FS_ERROR(fs_file_open(&fs, dst_path, FS_CREATE, &file));
    
    char buffer[256];
    size_t read;
    size_t written;
    while (read = fread(buffer, 1, 256, real_file))
    {
        HANDLE_FS_ERROR(fs_file_write(&fs, &file, buffer, read, &written));
    }
    
    fclose(real_file);
    
    HANDLE_FS_ERROR(fs_file_close(&fs, &file));
}

void cmd_export(const char* source, const char* real_destination)
{
    char src_path[FS_PATH_MAX_LENGTH];
    absolute_path(source, src_path);
    
    fs_file_t file;
    HANDLE_FS_ERROR(fs_file_open(&fs, src_path, 0, &file));
    
    FILE* real_file = fopen(real_destination, "w+");
    if (real_file == NULL)
    {
        printf("Cannot open external file %s\n", real_destination);
        return;
    }
    
    char buffer[256];
    size_t read;
    int err = fs_file_read(&fs, &file, buffer, 256, &read);
    while (err != FS_EOF)
    {
        fwrite(buffer, 1, read, real_file);
        err = fs_file_read(&fs, &file, buffer, 256, &read);
        if (err != FS_EOF && err != FS_OK)
        {
            print_fs_error(err);
            return;
        }
    }
    
    fclose(real_file);
    
    HANDLE_FS_ERROR(fs_file_close(&fs, &file));
}

void cmd_edit(const char* path)
{
    char full_path[FS_PATH_MAX_LENGTH];
    absolute_path(path, full_path);
    
    fs_dir_entry_t result;
    int err = fs_entry_info(&fs, full_path, &result);
    
    if (err == FS_OK)
    {
        cmd_export(path, TMP_FILENAME);
    }
    else if (err != FS_NOT_EXISTS)
    {
        print_fs_error(err);
        return;
    }
    
    system("vim " TMP_FILENAME);
    
    cmd_import(TMP_FILENAME, path);

    remove(TMP_FILENAME);
}

void cmd_cat(const char* path)
{
    char full_path[FS_PATH_MAX_LENGTH];
    absolute_path(path, full_path);
    
    fs_file_t file;
    HANDLE_FS_ERROR(fs_file_open(&fs, full_path, 0, &file));
    
    char buffer[256];
    size_t read;
    int err = fs_file_read(&fs, &file, buffer, 256, &read);
    while (err != FS_EOF)
    {
        if (err != FS_OK)
        {
            print_fs_error(err);
            return;
        }
        
        for (size_t i = 0; i < read; i++) putchar(buffer[i]);
        
        err = fs_file_read(&fs, &file, buffer, 256, &read);
    }
    
    HANDLE_FS_ERROR(fs_file_close(&fs, &file));

    putchar('\n');
}

void cmd_ls(const char* path, int show_details, int show_size)
{
    char full_path[FS_PATH_MAX_LENGTH];
    absolute_path(path, full_path);
    
    size_t count;
    HANDLE_FS_ERROR(fs_dir_list(&fs, full_path, entries, &count, MAX_DIR_ENTRIES));
    
    for (size_t i = 0; i < count; i++)
    {
        printf("%-4s ", entries[i].node_type == FS_FILE ? "FILE" : "DIR");
        if (show_details)
        {
            time_t raw_time = (time_t)entries[i].node_modification_time;
            struct tm* time = localtime(&raw_time);
            
            char tbuffer[20];
            strftime(tbuffer, 20, "%Y-%m-%d %H:%M:%S", time);
            
            printf("0x%08X %2d %s ", entries[i].node, entries[i].node_links_count, tbuffer);
        }
        printf(" %-27s", entries[i].name);
        if (show_size && strcmp(entries[i].name, "..") != 0)
        {
            uint32_t size;
            HANDLE_FS_ERROR(fs_size(&fs, entries[i].node, &size));
            printf(" %d B", size);
        }
        putchar('\n');
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
            
            HANDLE_FS_ERROR(fs_entry_info(&fs, tmp_current, &entries[0]));
        }
    }
    
    strcpy(current_dir, tmp_current);
}

void cmd_pwd()
{
    puts(current_dir);
}

void cmd_exp(const char* path, size_t count)
{
    char full_path[FS_PATH_MAX_LENGTH];
    absolute_path(path, full_path);
    
    fs_file_t file;
    HANDLE_FS_ERROR(fs_file_open(&fs, full_path, FS_APPEND, &file));
    
    char buffer[256];
    memset(buffer, 0xFF, 256);
    size_t written;
    while (count)
    {
        size_t c = count;
        if (c > 256) c = 256;
        HANDLE_FS_ERROR(fs_file_write(&fs, &file, buffer, c, &written));
        count -= written;
    }
    
    HANDLE_FS_ERROR(fs_file_close(&fs, &file));
}

void cmd_trunc(const char* path, size_t count)
{
    char full_path[FS_PATH_MAX_LENGTH];
    absolute_path(path, full_path);
    
    fs_file_t file;
    HANDLE_FS_ERROR(fs_file_open(&fs, full_path, 0, &file));
    
    HANDLE_FS_ERROR(fs_file_seek(&fs, &file, FS_SEEK_END, count));
    
    HANDLE_FS_ERROR(fs_file_discard(&fs, &file));
    
    HANDLE_FS_ERROR(fs_file_close(&fs, &file));
}

void cmd_fsinfo()
{
    fs_info_t info;
    HANDLE_FS_ERROR(fs_info(&fs, &info));
    
    printf("Sectors (total / boot / allocation table): %d / %d / %d\n", info.sectors, 1, info.table_sectors);
    printf("Clusters (total / free / node / data): %d / %d / %d / %d\n", info.clusters, info.free_clusters, info.node_clusters, info.data_clusters);
    printf("Nodes (used / allocated): %d / %d\n", info.nodes, info.allocated_nodes);
    printf("File system size (total / usable): %d B / %d B\n", info.total_size, info.usable_space);    
    
    printf("Size (files / directory structures / nodes): %d B / %d B / %d B\n", info.files_size, info.dir_structures_size, info.nodes_size);
    
    printf("Usage: %d / %d B\n", info.used_space, info.usable_space);
}

void cmd_help()
{
    puts("cp source destination - Copies file from source to destination.");
    puts("mv source destination - Moves file from soruce to destination.");
    puts("mkdir path - Creates directory. Allows nested directories.");
    puts("touch path - Creates empty file.");
    puts("ln file_path link_name - Creates hard link of link_name to file_path.");
    puts("rm path - Removes file or directory recursively.");
    puts("import real_source destination - Imports external file into file system.");
    puts("export source real_destination - Exports file from file system.");
    puts("edit file - Enters edit mode for specified file.");
    puts("cat file - Prints content of specified file");
    puts("ls [path] [-ds] - Lists specified directory. If path not specified then current directory is used. Flag -d - show detailed information (node index, links count). Flag -s - show size of the files and directories.");
    puts("cd dir - Change current directory.");
    puts("pwd - Prints path to current directory.");
    puts("exp file bytes - Expands file by specified amount of bytes");
    puts("trunc file bytes - Truncates file by specified amount of bytes");
    puts("fsinfo - Displays info about file system");
    puts("exit - Closes file system and exists application.");
    puts("help - Displays help");
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
        case FS_FILE_CLOSED: puts("File is closed"); break;
        case FS_EOF: puts("End of file"); break;
        case FS_ALREADY_EXISTS: puts("Already exists"); break;
    }
}

void absolute_path(const char* path, char* result)
{
    if (path[0] != '/')
    {
        strcpy(result, current_dir);
        size_t len = strlen(result);
        strcpy(result + len, path);
    }
    else
    {
        strcpy(result, path);
    }
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