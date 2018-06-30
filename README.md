# FATNode file system

## Summary
This is an implementation of a file system which I designed for my operating system classes. It combines idea of File Allocation Table and UNIX inodes.

## Design
Entire disk is divided into small sectors of preconceived size (configured by FS_SECTOR_SIZE macro, 128 in current implementation).

The first sector is called **bootstrap sector** and contains all informations about file system neccessary to open it, including:
* Number of sectors
* Root node
* Index of first allocation table sector and number of sectors containing allocation table
* Index of sector with first cluster and number of clusters

Bootstrap sector is followed by **allocation table** which consist of few sectors depending on the file system size. Each table entry contains 4-byte information holding state of **cluster**:
* **```0x00000000```** - empty cluster
* **```0xFFFFFFFE```** - end of file
* **```0xFFFFFFFF```** - invalid cluster
* **```0xFFFFFF00```** - empty cluster holding nodes
* **```0xFFFFFF__```** - cluster holding nodes, __ indicates how many node structures are in use
* **```0xFFFFFF08```** - full cluster holding nodes
* **other value** - indicates index of next cluster containing continuation of data stored in this cluster

The rest of sectors in the file system are called **clusters** and they are used to hold file contents, directory structures or nodes.

**Node** is 16 byte structure holding information about file or directory stored in the file system:
* **```uint8_t flags```** - node flags, especially *FS_NODE_FLAGS_IN_USE* which indicates if entry is in use
* **```uint8_t type```** - type of node, either *FS_NODE_TYPE_FILE* or *FS_NODE_TYPE_DIR*
* **```uint16_t links_count```** - count of hard links to this node
* **```uint32_t size```** - size in bytes of file, if node is directory then total size of occupied clusters
* **```uint32_t cluster_index```** - first cluster containing data of file or directory
* **```uint32_t modification_time```** - last modification time, UNIX timestamp

Each node is indentified by its **node number**. Node number is 32 bit unsigned integer which contains exact location of node. Upper 24 bits are cluster index and lower 8 bits are index of structure within cluster.

**Directory** is simple list containing names paired with node indexes. Each entry occupy 32 bytes. First 28 bytes are reserved for name, terminated with null-terminator character. Last 4 bytes are occupied by node number. Each cluster can hold only 4 entries, thus next entires are stored in diffrent clusters and allocation table is used to indicate next part.

**Node cluster** is a cluster which can hold 8 node structures. When new node is requested, file system searches for existing node cluster with free entry. If not found, new cluster will be allocated for nodes and marked as *node cluster*.

## Implementation
Core file system logic is implemented in *fs.c* and *fs.h* files. *main.c* contains command line interface for manipulating file system and provides following commands:
* ```cp source destination``` - Copies file from source to destination.
* ```mv source destination``` - Moves file from soruce to destination.
* ```mkdir path``` - Creates directory. Allows nested directories.
* ```touch path``` - Creates empty file.
* ```ln file_path link_name``` - Creates hard link of link_name to file_path.
* ```rm path``` - Removes file or directory recursively.
* ```import real_source destination``` - Imports external file into file system.
* ```export source real_destination``` - Exports file from file system.
* ```edit file``` - Enters edit mode for specified file.
* ```cat file``` - Prints content of specified file
* ```ls [path] [-ds]``` - Lists specified directory. If path not specified then current directory is used. Flag -d - show detailed information (node index, links count, modification time). Flag -s - show size of the files and directories.
* ```cd dir``` - Change current directory.
* ```pwd``` - Prints path to current directory.
* ```exp file bytes``` - Expands file by specified amount of bytes
* ```trunc file bytes``` - Truncates file by specified amount of bytes
* ```fsinfo``` - Displays info about file system
* ```exit``` - Closes file system and exists application.
* ```help``` - Displays help

## Build
Use makefile

## Usage
Create new file system: ```./fs file_name size_in_bytes```  
Open existing file system: ```./fs file_name```

File system can be also used on real devices. In order to perform that, pass device path instead of file name and run *fs* with root privileges, example:  
```sudo ./fs /dev/sdb1 16384```

## Notes
This implementation is not well tested and usage for any real application is discouraged. Project has been made only for educational purposes.