#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#define MAX_FILENAME_SIZE 255
#define BLOCK_SIZE 4 * 1024
#define UNUSED_FLAG 0
#define USED_FLAG 1
#define BITMAP_BLOCK 1
#define FCB_BLOCKS_START 9
#define FCB_BLOCKS_COUNT 4
#define SUPERBLOCK_BLOCK 0
#define ROOT_DIR_START 5
#define ROOT_DIR_COUNT 4

uint32_t block_count;
int vdisk_fd;

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

struct DirectoryEntry {
  char filename[MAX_FILENAME_SIZE + 1];
  uint32_t size;
  uint32_t fcb_index;
  bool used;
};

#pragma pack(pop)

void write_block(void *block, uint32_t block_number) {
  uint32_t offset = block_number * BLOCK_SIZE;
  lseek(vdisk_fd, (off_t)offset, SEEK_SET);

  ssize_t bytes_written = write(vdisk_fd, block, BLOCK_SIZE);
}

void init_bitmap() {
  bool bitmap[BLOCK_SIZE];

  for (int i = 0; i < BLOCK_SIZE; i++) {
    bitmap[i] = UNUSED_FLAG;
  }

  write_block((void *)bitmap, BITMAP_BLOCK);
}

void init_superblock(int total_blocks, int available_blocks, int total_fcbs) {
  char block[BLOCK_SIZE] = {0};
  struct SuperBlock *superblock = (struct SuperBlock *)block;
  superblock->num_blocks = total_blocks;
  superblock->num_free_blocks = available_blocks;
  superblock->num_files = 0;
  superblock->num_free_fcbs = total_fcbs;

  write_block((void *)superblock, SUPERBLOCK_BLOCK);
}

int init_FCB() {
  int fcb_size = sizeof(struct FCB);
  int total_fcbs = 0;
  int num_fcbs = BLOCK_SIZE / fcb_size;
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

void init_root_directory() {
  int dir_entry_size = sizeof(struct DirectoryEntry);
  int num_entries = BLOCK_SIZE / dir_entry_size;

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
