#include "simple_file_system.h"
#include <fcntl.h>
#include <stdbool.h>
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

int sfs_mount(char *vdiskname) {
  vdisk_fd = open(vdiskname, O_RDWR);
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
/*int main() {
  create_format_vdisk("myvdisk", 20);
  sfs_mount("myvdisk");
  sfs_umount();
  return 0;
}*/
