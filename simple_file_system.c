#include "simple_file_system.h"
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

// Utility functions
int min(int a, int b) { return a > b ? b : a; }

int dir_entry_size = sizeof(struct DirectoryEntry);
int num_entries;
uint32_t block_count;
int num_fcbs;

int vdisk_fd;

struct SuperBlock superblock;
struct DirectoryEntry *directory;
struct FCB *file_control_blocks;
bool bitmap[BLOCK_SIZE];

int file_count = 0;
int open_file_count = 0;
struct OpenFile open_file_table[MAX_OPEN_FILES];

int create_format_vdisk(char *vdiskname, unsigned int m) {
  char command[1000];
  int size;
  int num = 1;
  int count;

  size = num << m;
  count = size / BLOCK_SIZE;

  printf("LOG(create_format_vdisk): (m: %d, size: %d bytes, blocks: %d)\n", m,
         size, count);

  int header_count = 1 + 1 + FCB_BLOCKS_COUNT + ROOT_DIR_COUNT;

  if (count < header_count) {
    printf("ERROR: Larger disk size required!\n");
    return -1;
  }

  sprintf(command, "dd if=/dev/zero of=%s bs=%d count=%d", vdiskname,
          BLOCK_SIZE, count);
  system(command);

  vdisk_fd = open(vdiskname, O_RDWR);
  if (vdisk_fd < 0) {
    perror("Failed to open virtual disk");
    return -1;
  }

  int total_blocks = count;
  int available_blocks = total_blocks - header_count;

  init_bitmap();
  int total_fcbs = init_FCB();
  init_superblock(total_blocks, available_blocks, total_fcbs);
  init_root_directory();

  fsync(vdisk_fd);
  close(vdisk_fd);
  return 0;
}

void write_block(void *block, uint32_t block_number) {
  uint32_t offset = block_number * BLOCK_SIZE;
  lseek(vdisk_fd, (off_t)offset, SEEK_SET);

  ssize_t bytes_written = write(vdisk_fd, block, BLOCK_SIZE);
}

void read_block(void *block, uint32_t block_number) {
  // Reads the given block number and copies the content into block

  uint32_t offset = block_number * BLOCK_SIZE;
  lseek(vdisk_fd, (off_t)offset, SEEK_SET);

  read(vdisk_fd, block, BLOCK_SIZE);
}

// Bitmap related functions

void init_bitmap() {

  for (int i = 0; i < BLOCK_SIZE; i++) {
    bitmap[i] = UNUSED_FLAG;
  }

  write_block((void *)bitmap, BITMAP_BLOCK);
}

void load_bitmap() { read_block(bitmap, BITMAP_BLOCK); }

int find_empty_block() {
  for (int i = 0; i < BLOCK_SIZE; i++) {
    if (bitmap[i] == UNUSED_FLAG) {
      bitmap[i] = USED_FLAG;
      return i;
    }
  }
  return -1;
}

// Superblock related functions

void init_superblock(int total_blocks, int available_blocks, int total_fcbs) {
  char block[BLOCK_SIZE] = {0};
  struct SuperBlock *superblock = (struct SuperBlock *)block;
  superblock->num_blocks = total_blocks;
  superblock->num_free_blocks = available_blocks;
  superblock->num_files = 0;
  superblock->num_free_fcbs = total_fcbs;

  write_block((void *)superblock, SUPERBLOCK_BLOCK);
}

void get_superblock(struct SuperBlock *superblock_buffer) {
  char block[BLOCK_SIZE] = {0};
  read_block(block, SUPERBLOCK_BLOCK);
  memcpy(superblock_buffer, block, sizeof(struct SuperBlock));
}

int init_FCB() {
  int fcb_size = sizeof(struct FCB);
  int total_fcbs = 0;
  num_fcbs = BLOCK_SIZE / fcb_size;
  char block[BLOCK_SIZE] = {0};

  for (int i = 0; i < FCB_BLOCKS_COUNT; i++) {
    total_fcbs += num_fcbs;

    for (int j = 0; j < num_fcbs; j++) {
      struct FCB *fcb = (struct FCB *)(block + j * fcb_size);
      fcb->used = UNUSED_FLAG;
    }

    write_block((void *)block, FCB_BLOCKS_START + i);
  }
  return total_fcbs;
}

void load_FCBs() {
  char block[BLOCK_SIZE] = {0};
  int fcb_size = sizeof(struct FCB);
  num_fcbs = BLOCK_SIZE / fcb_size;
  file_control_blocks =
      malloc(num_fcbs * FCB_BLOCKS_COUNT * sizeof(struct FCB));
  for (int i = 0; i < FCB_BLOCKS_COUNT; i++) {
    read_block(block, FCB_BLOCKS_START + i);
    memcpy(&file_control_blocks[i * num_fcbs], block, num_fcbs * fcb_size);
  }
}

void init_open_file_table() {
  for (int i = 0; i < MAX_OPEN_FILES; i++) {
    open_file_table[i].dir_entry_pointer = NULL;
    open_file_table[i].read_write_pointer = 0;
  }
}

// Index Block operations

void set_index_block(struct IndexBlock *index_block, int block_number) {
  char block[BLOCK_SIZE];
  memcpy(block, index_block, sizeof(struct IndexBlock));

  // Zero out any remaining space in the block (for alignment/padding)
  if (sizeof(struct IndexBlock) < BLOCK_SIZE) {
    memset(block + sizeof(struct IndexBlock), 0,
           BLOCK_SIZE - sizeof(struct IndexBlock));
  }

  write_block(block, block_number);
}

void get_index_block(struct IndexBlock *index_block, int block_number) {
  char block[BLOCK_SIZE];
  read_block(block, block_number);
  memcpy(index_block, block, sizeof(struct IndexBlock));
}
// Directory operations

void init_root_directory() {
  int dir_entry_size = sizeof(struct DirectoryEntry);
  num_entries = BLOCK_SIZE / dir_entry_size;
  directory =
      malloc(num_entries * ROOT_DIR_COUNT * sizeof(struct DirectoryEntry));

  char block[BLOCK_SIZE];

  for (int i = 0; i < num_entries; i++) {
    struct DirectoryEntry *dir_entry =
        (struct DirectoryEntry *)(block + i * dir_entry_size);
    dir_entry->used = UNUSED_FLAG;
  }

  for (int i = 0; i < ROOT_DIR_COUNT; i++) {
    write_block((void *)block, ROOT_DIR_START + i);
  }
}

void load_directory() {
  char block[BLOCK_SIZE]; // buffer for reading
  int dir_entry_size = sizeof(struct DirectoryEntry);
  directory =
      malloc(num_entries * ROOT_DIR_COUNT * sizeof(struct DirectoryEntry));

  for (int i = 0; i < ROOT_DIR_COUNT; i++) {
    read_block(block, ROOT_DIR_START + i);
    memcpy(&directory[i * num_entries], block, num_entries * dir_entry_size);
  }
}

// File system operations

int sfs_mount(char *vdiskname) {
  vdisk_fd = open(vdiskname, O_RDWR);
  get_superblock(&superblock);
  load_directory();
  load_bitmap();
  load_FCBs();
  init_open_file_table();

  if (vdisk_fd < 0) {
    perror("Failed to mount vdisk");
    return -1;
  }
  printf("LOG(sfs_mount): Mounted %s successfully\n", vdiskname);

  return 0;
}

int sfs_umount() {
  if (vdisk_fd >= 0) {
    fsync(vdisk_fd); // Ensure all writes are flushed
    close(vdisk_fd);
    printf("LOG(sfs_umount): Unmounted successfully\n");
    vdisk_fd = -1;
  } else {
    printf("LOG(sfs_umount): No disk mounted.\n");
  }
  return 0;
}

int sfs_create(char *filename) {
  if (file_count == MAX_FILES) {
    printf("Max number of files created! Cannot create more files");
    return -1;
  }

  // Find first free directory entry + check if already a file of same name
  // exists
  int first_free_dir_entry = num_entries * ROOT_DIR_COUNT + 1;
  for (int i = 0; i < num_entries * ROOT_DIR_COUNT; i++) {
    if (directory[i].used == UNUSED_FLAG) {
      first_free_dir_entry = min(first_free_dir_entry, i);
    }

    if (directory[i].used == USED_FLAG &&
        strcmp(directory[i].filename, filename) == 0) {
      printf("Directory already has file of same name!\n");
      return -1;
    }
  }

  // Find first empty FCB
  int first_free_fcb = num_fcbs * FCB_BLOCKS_COUNT + 1;
  for (int i = 0; i < num_fcbs * FCB_BLOCKS_COUNT; i++) {
    if (file_control_blocks[i].used == UNUSED_FLAG) {
      first_free_fcb = min(first_free_fcb, i);
      break;
    }
  }

  if (first_free_fcb == num_fcbs * FCB_BLOCKS_COUNT + 1) {
    printf("No free FCBs remaining\n");
    return -1;
  }

  // Find first empty block in bitmap (for index block)
  int first_free_block = BLOCK_SIZE + 1;
  for (int i = 0; i < BLOCK_SIZE; i++) {
    if (bitmap[i] == false) {
      first_free_block = min(first_free_block, i);
      bitmap[first_free_block] = true;
      break;
    }
  }

  if (first_free_block == BLOCK_SIZE + 1) {
    printf("No free blocks remaining\n");
    return -1;
  }

  first_free_block += FCB_BLOCKS_COUNT + FCB_BLOCKS_START;

  // Write index block to first free block
  struct IndexBlock index_block;
  memset(index_block.block_pointers, INVALID_BLOCK_POINTER,
         sizeof(index_block.block_pointers));
  set_index_block(&index_block, first_free_block);

  // Set directory entry
  directory[first_free_dir_entry].index_block = first_free_block;
  directory[first_free_dir_entry].used = USED_FLAG;
  directory[first_free_dir_entry].fcb_index = first_free_fcb;
  strcpy(directory[first_free_dir_entry].filename, filename);
  directory[first_free_dir_entry].size = 0;

  // Set FCB
  file_control_blocks[first_free_fcb].used = USED_FLAG;
  strcpy(file_control_blocks[first_free_fcb].filename, filename);
  file_control_blocks[first_free_fcb].size = 0;
  file_control_blocks[first_free_fcb].created_at = time(NULL);
  file_control_blocks[first_free_fcb].last_modified_at = time(NULL);

  file_count++;

  return 0;
}

int sfs_delete(char *filename) {

  int dir_entry_index = num_entries * ROOT_DIR_COUNT + 1;
  for (int i = 0; i < num_entries * ROOT_DIR_COUNT; i++) {
    if (directory[i].used == USED_FLAG &&
        strcmp(directory[i].filename, filename) == 0) {
      dir_entry_index = i;
      break;
    }
  }

  if (dir_entry_index == num_entries * ROOT_DIR_COUNT + 1) {
    printf("Could not find given file\n");
    return -1;
  }

  // Mark directory entry as unused
  directory[dir_entry_index].used = UNUSED_FLAG;
  printf("Set dir entry to unused\n");

  // Mark file control block as ununsed
  file_control_blocks[directory[dir_entry_index].fcb_index].used = UNUSED_FLAG;
  printf("Set FCB to unused\n");

  // Mark all blocks in index block as unused
  char block[BLOCK_SIZE];
  read_block(block, directory[dir_entry_index].index_block);
  struct IndexBlock *index_block = (struct IndexBlock *)block;

  for (int i = 0; i < POINTERS_PER_BLK &&
                  index_block->block_pointers[i] != INVALID_BLOCK_POINTER;
       i++) {
    bitmap[index_block->block_pointers[i]] = UNUSED_FLAG;
  }
  printf("Set bitmap blocks to unused\n");

  // Mark index block as unused
  bitmap[directory[dir_entry_index].index_block] = UNUSED_FLAG;
  printf("Set bitmap index block to unused\n");

  file_count--;

  return 0;
}

int sfs_open(char *filename, int mode) {
  if (open_file_count >= MAX_OPEN_FILES) {
    printf("Maximum number of files already opened (%d)\n", open_file_count);
    return -1;
  }

  for (int i = 0; i < MAX_OPEN_FILES; i++) {
    if (open_file_table[i].dir_entry_pointer != NULL &&
        strcmp(open_file_table[i].dir_entry_pointer->filename, filename) == 0) {
      printf("This file is already opened somewhere!\n");
      return -1;
    }
  }

  // Find first free entry in open file table (this will also be our file
  // descriptor)
  int fd = -1;
  for (int i = 0; i < MAX_OPEN_FILES; i++) {
    if (open_file_table[i].dir_entry_pointer == NULL) {
      fd = i;
      break;
    }
  }

  // Find the directory entry of the file
  int dir_entry_index = num_entries * ROOT_DIR_COUNT + 1;
  for (int i = 0; i < num_entries * ROOT_DIR_COUNT; i++) {
    if (directory[i].used == USED_FLAG &&
        strcmp(directory[i].filename, filename) == 0) {
      dir_entry_index = i;
      break;
    }
  }

  if (dir_entry_index == num_entries * ROOT_DIR_COUNT + 1) {
    printf("Could not find given file\n");
    return -1;
  }

  open_file_table[fd].open_mode = mode;
  open_file_table[fd].read_write_pointer = 0;
  open_file_table[fd].dir_entry_pointer = &directory[dir_entry_index];
  open_file_count++;

  return fd;
}

int sfs_close(int fd) {
  if (fd >= MAX_OPEN_FILES) {
    printf("File descriptor out of range");
    return -1;
  }

  if (open_file_table[fd].dir_entry_pointer == NULL) {
    printf("The file was not open\n");
    return -1;
  }

  open_file_table[fd].dir_entry_pointer = NULL;
  open_file_count--;

  return 0;
}

int sfs_seek(int fd, int offset, int whence) {
  int read_write_pointer_copy = open_file_table[fd].read_write_pointer;
  switch (whence) {
  case SEEK_SET:
    open_file_table[fd].read_write_pointer = offset;
    break;

  case SEEK_CUR:
    open_file_table[fd].read_write_pointer += offset;
    break;

  case SEEK_END:
    open_file_table[fd].read_write_pointer =
        file_control_blocks[open_file_table[fd].dir_entry_pointer->fcb_index]
            .size +
        offset;
    break;
  }

  if (open_file_table[fd].read_write_pointer >
      file_control_blocks[open_file_table[fd].dir_entry_pointer->fcb_index]
          .size) {
    printf("Read-Write pointer going out of bounds\n");
    open_file_table[fd].read_write_pointer = read_write_pointer_copy;
    return -1;
  }

  return 0;
}

int sfs_read(int fd, void *buffer, int size) {

  if (fd >= MAX_OPEN_FILES || open_file_table[fd].dir_entry_pointer == NULL) {
    printf(
        "ERROR: The given file descriptor does not belong to an open file\n");
    return -1;
  }

  if (open_file_table[fd].open_mode != READ_MODE) {
    printf("ERROR: The given file is not opened in read mode\n");
    return -1;
  }
  int file_size =
      file_control_blocks[open_file_table[fd].dir_entry_pointer->fcb_index]
          .size;
  printf("File size: %d\n", file_size);

  int read_write_pointer = open_file_table[fd].read_write_pointer;
  if (read_write_pointer + size > file_size) {
    printf("ERROR: Not enough bytes to read in file\n");
    return -1;
  }

  if (size == 0) {
    return 0;
  }

  int start_block = read_write_pointer / BLOCK_SIZE;
  int start_block_offset = read_write_pointer % BLOCK_SIZE;
  int end_block = (read_write_pointer + size) / BLOCK_SIZE;
  int end_block_offset = (read_write_pointer + size) % BLOCK_SIZE;

  // Fetch the index block of the file
  struct IndexBlock index_block;
  get_index_block(&index_block,
                  open_file_table[fd].dir_entry_pointer->index_block);

  char block[BLOCK_SIZE];
  size_t copy_size;
  if (start_block == end_block) {
    read_block(block, index_block.block_pointers[start_block]);

    copy_size = end_block_offset - start_block_offset;
    memcpy(buffer, block + start_block_offset, copy_size);
    return 0;
  }

  // Copy from (starting pointer offset to end of starting block)
  read_block(block, index_block.block_pointers[start_block]);
  copy_size = BLOCK_SIZE - start_block_offset;
  memcpy(buffer, block + start_block_offset, copy_size);

  // Copy from first block after the starting offset till last complete end
  // block
  for (int i = start_block + 1; i < end_block; i++) {
    read_block(buffer + start_block_offset + (i - start_block) * BLOCK_SIZE,
               index_block.block_pointers[i]);
  }

  // Copy from last complete block to end pointer offset
  read_block(block, index_block.block_pointers[end_block]);
  copy_size = end_block_offset;
  memcpy(buffer + start_block_offset + (end_block - start_block) * BLOCK_SIZE,
         block, copy_size);

  open_file_table[fd].read_write_pointer =
      end_block * BLOCK_SIZE + end_block_offset;

  return 0;
}

int sfs_write(int fd, void *buffer, int size) {

  if (fd >= MAX_OPEN_FILES || open_file_table[fd].dir_entry_pointer == NULL) {
    printf(
        "ERROR: The given file descriptor does not belong to an open file\n");
    return -1;
  }

  if (open_file_table[fd].open_mode != WRITE_MODE) {
    printf("ERROR: The given file is not opened in write mode\n");
    return -1;
  }

  int file_size =
      file_control_blocks[open_file_table[fd].dir_entry_pointer->fcb_index]
          .size;
  int read_write_pointer = open_file_table[fd].read_write_pointer;

  int start_block = read_write_pointer / BLOCK_SIZE;
  int start_block_offset = read_write_pointer % BLOCK_SIZE;
  int end_block = (read_write_pointer + size) / BLOCK_SIZE;
  int end_block_offset = (read_write_pointer + size) % BLOCK_SIZE;

  if (size == 0) {
    return 0;
  }

  // Fetch the index block of the file
  struct IndexBlock index_block;
  get_index_block(&index_block,
                  open_file_table[fd].dir_entry_pointer->index_block);

  char block[BLOCK_SIZE];
  size_t copy_size;

  // If file is empty, find an empty block and assign it to the file
  if (index_block.block_pointers[start_block] == INVALID_BLOCK_POINTER) {
    int empty_block_index = find_empty_block();
    if (empty_block_index == -1) {
      printf("ERROR: Couldn't find a free block to assign to file\n");
      return -1;
    }
    index_block.block_pointers[start_block] = empty_block_index;
  }

  // If the start pointer and end pointer in same block
  if (start_block == end_block) {
    read_block(block, index_block.block_pointers[start_block]);

    copy_size = end_block_offset - start_block_offset;
    memcpy(block + start_block_offset, buffer, copy_size);
    write_block(block, index_block.block_pointers[start_block]);

    uint32_t new_size = read_write_pointer + size;

    // Update all size references
    open_file_table[fd].read_write_pointer = new_size;

    // Get FCB reference
    struct FCB *fcb =
        &file_control_blocks[open_file_table[fd].dir_entry_pointer->fcb_index];

    // Update metadata
    fcb->size = new_size;
    fcb->last_modified_at = time(NULL);
    open_file_table[fd].dir_entry_pointer->size = new_size;

    // Persist the index block changes
    set_index_block(&index_block,
                    open_file_table[fd].dir_entry_pointer->index_block);

    return 0;
  }

  // Write from start pointer offset to end of start pointer block
  read_block(block, index_block.block_pointers[start_block]);

  copy_size = BLOCK_SIZE - start_block_offset;
  memcpy(block + start_block_offset, buffer, copy_size);
  write_block(block, index_block.block_pointers[start_block]);

  // Write from first block after the starting offset till last complete end
  // block
  for (int i = start_block + 1; i < end_block; i++) {
    if (index_block.block_pointers[i] == INVALID_BLOCK_POINTER) {
      int empty_block_index = find_empty_block();
      if (empty_block_index == -1) {
        printf("ERROR: Couldn't find a free block to assign to file\n");
        open_file_table[fd].read_write_pointer +=
            start_block_offset + (i - 1 - start_block) * BLOCK_SIZE;
        return -1;
      }
      index_block.block_pointers[i] = empty_block_index;
    }
    write_block(buffer + start_block_offset + (i - start_block) * BLOCK_SIZE,
                index_block.block_pointers[i]);
  }

  // Write till end block offset

  if (index_block.block_pointers[end_block] == INVALID_BLOCK_POINTER) {
    int empty_block_index = find_empty_block();
    if (empty_block_index == -1) {
      printf("ERROR: Couldn't find a free block to assign to file\n");
      open_file_table[fd].read_write_pointer +=
          start_block_offset + (end_block - 1 - start_block) * BLOCK_SIZE;
      return -1;
    }
    index_block.block_pointers[end_block] = empty_block_index;
  }

  copy_size = end_block_offset;
  memcpy(block,
         buffer + start_block_offset + (end_block - start_block) * BLOCK_SIZE,
         copy_size);

  // Free the remaining blocks
  for (int i = end_block + 1; i < POINTERS_PER_BLK; i++) {
    if (index_block.block_pointers[i] != INVALID_BLOCK_POINTER) {
      bitmap[index_block.block_pointers[i]] = UNUSED_FLAG;
    }
    index_block.block_pointers[i] = INVALID_BLOCK_POINTER;
  }

  // At the end of sfs_write():
  uint32_t new_size = read_write_pointer + size;

  // Update all size references
  open_file_table[fd].read_write_pointer = new_size;

  // Get FCB reference
  struct FCB *fcb =
      &file_control_blocks[open_file_table[fd].dir_entry_pointer->fcb_index];

  // Update metadata
  fcb->size = new_size;
  fcb->last_modified_at = time(NULL);
  open_file_table[fd].dir_entry_pointer->size = new_size;

  // Persist the index block changes
  set_index_block(&index_block,
                  open_file_table[fd].dir_entry_pointer->index_block);

  return 0;
}

int sfs_append(char *filename, void *data, size_t size) {
  // Find the directory entry for the file
  int dir_entry_index = -1;
  for (int i = 0; i < num_entries * ROOT_DIR_COUNT; i++) {
    if (directory[i].used == USED_FLAG &&
        strcmp(directory[i].filename, filename) == 0) {
      dir_entry_index = i;
      break;
    }
  }

  if (dir_entry_index == -1) {
    printf("File not found\n");
    return -1;
  }

  // Get the corresponding FCB for the file
  struct FCB *fcb = &file_control_blocks[directory[dir_entry_index].fcb_index];
  uint32_t current_size = fcb->size;

  // If the file is empty, initialize index block
  if (current_size == 0) {
    struct IndexBlock new_index_block;
    memset(new_index_block.block_pointers, INVALID_BLOCK_POINTER,
           sizeof(new_index_block.block_pointers));
    set_index_block(&new_index_block, directory[dir_entry_index].index_block);
  }

  // Find the last block in the file's index block (or first available index
  // block)
  struct IndexBlock index_block;
  read_block(&index_block, directory[dir_entry_index].index_block);

  uint32_t last_block = INVALID_BLOCK_POINTER;
  for (int i = 0; i < POINTERS_PER_BLK; i++) {
    if (index_block.block_pointers[i] != INVALID_BLOCK_POINTER) {
      last_block = index_block.block_pointers[i];
    } else {
      break;
    }
  }

  // If the file is too short, allocate a new block
  if (last_block == INVALID_BLOCK_POINTER) {
    // Find a free block from the bitmap
    int new_block = -1;
    for (int i = 0; i < BLOCK_SIZE; i++) {
      if (bitmap[i] == UNUSED_FLAG) {
        new_block = i;
        bitmap[new_block] = true;
        break;
      }
    }

    if (new_block == -1) {
      printf("No free blocks available\n");
      return -1;
    }

    // Update the index block with the new block pointer
    for (int i = 0; i < POINTERS_PER_BLK; i++) {
      if (index_block.block_pointers[i] == INVALID_BLOCK_POINTER) {
        index_block.block_pointers[i] = new_block;
        set_index_block(&index_block, directory[dir_entry_index].index_block);
        last_block = new_block;
        break;
      }
    }
  }

  // Write the new data to the last block of the file
  char block[BLOCK_SIZE];
  read_block(block, last_block);
  uint32_t offset_in_block = current_size % BLOCK_SIZE;

  // Check if we have enough space in the current block
  if (offset_in_block + size <= BLOCK_SIZE) {
    memcpy(block + offset_in_block, data, size);
    write_block(block, last_block);
  } else {
    // If not enough space, we need to split the data across multiple blocks
    size_t remaining_size = size;
    char *data_ptr = (char *)data;

    // Write part of the data into the current block
    memcpy(block + offset_in_block, data_ptr, BLOCK_SIZE - offset_in_block);
    write_block(block, last_block);

    remaining_size -= (BLOCK_SIZE - offset_in_block);
    data_ptr += (BLOCK_SIZE - offset_in_block);

    // Allocate new blocks and continue writing
    while (remaining_size > 0) {
      int new_block = -1;
      for (int i = 0; i < BLOCK_SIZE; i++) {
        if (bitmap[i] == UNUSED_FLAG) {
          new_block = i;
          bitmap[new_block] = true;
          break;
        }
      }

      if (new_block == -1) {
        printf("No free blocks available\n");
        return -1;
      }

      read_block(block, new_block);
      size_t chunk_size =
          remaining_size > BLOCK_SIZE ? BLOCK_SIZE : remaining_size;
      memcpy(block, data_ptr, chunk_size);
      write_block(block, new_block);

      remaining_size -= chunk_size;
      data_ptr += chunk_size;

      // Update index block with the new block pointer
      for (int i = 0; i < POINTERS_PER_BLK; i++) {
        if (index_block.block_pointers[i] == INVALID_BLOCK_POINTER) {
          index_block.block_pointers[i] = new_block;
          set_index_block(&index_block, directory[dir_entry_index].index_block);
          break;
        }
      }
    }
  }

  // Update the file size in the FCB
  fcb->size += size;
  fcb->last_modified_at = time(NULL);

  // Update the directory entry
  directory[dir_entry_index].size = fcb->size;

  return 0;
}
