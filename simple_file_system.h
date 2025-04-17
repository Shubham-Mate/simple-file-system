#ifndef SIMPLE_FILE_SYSTEM_H
#define SIMPLE_FILE_SYSTEM_H

#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define MAX_FILENAME_SIZE 255
#define BLOCK_SIZE 4096
#define UNUSED_FLAG 0
#define USED_FLAG 1
#define BITMAP_BLOCK 1
#define FCB_BLOCKS_START 9
#define FCB_BLOCKS_COUNT 4
#define SUPERBLOCK_BLOCK 0
#define ROOT_DIR_START 5
#define ROOT_DIR_COUNT 4
#define MAX_FILES 128
#define POINTERS_PER_BLK (BLOCK_SIZE / sizeof(uint32_t))
#define INVALID_BLOCK_POINTER 0xFF
#define MAX_OPEN_FILES 16

#define SFS_SEEK_SET 0
#define SFS_SEEK_CUR 1
#define SFS_SEEK_END 2

#define READ_MODE 0
#define WRITE_MODE 1

#pragma pack(push, 1)

struct FCB {
  char filename[MAX_FILENAME_SIZE + 1];
  char owner[MAX_FILENAME_SIZE + 1];
  uint32_t size;
  time_t created_at;
  time_t last_modified_at;
  bool used;
};

struct SuperBlock {
  uint32_t num_blocks;
  uint32_t num_free_blocks;
  uint32_t num_free_fcbs;
  uint32_t num_files;
};

struct IndexBlock {
  uint32_t block_pointers[POINTERS_PER_BLK];
};

struct DirectoryEntry {
  char filename[MAX_FILENAME_SIZE + 1];
  uint32_t size;
  uint32_t fcb_index;
  uint32_t index_block;
  bool used;
};

struct OpenFile {
  struct DirectoryEntry *dir_entry_pointer;
  int open_mode;
  int read_write_pointer;
};

#pragma pack(pop)

// Disk creation and management
int create_format_vdisk(char *vdiskname, unsigned int m);
int sfs_mount(char *vdiskname);
int sfs_umount();

// File operations
int sfs_create(char *filename);
int sfs_delete(char *filename);
int sfs_open(char *filename, int mode);
int sfs_close(int fd);
int sfs_seek(int fd, int offset, int whence);
int sfs_read(int fd, void *buffer, int size);
int sfs_write(int fd, void *buffer, int size);

// Utility functions
void write_block(void *block, uint32_t block_number);
void read_block(void *block, uint32_t block_number);

#endif // SIMPLE_FILE_SYSTEM_H
