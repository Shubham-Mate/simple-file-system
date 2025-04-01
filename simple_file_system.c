#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#define BLOCK_SIZE 4 * 1024;
#define UNUSED_FLAG 0;
#define USED_FLAG 1;
#define BITMAP_BLOCK 1;

uint32_t block_count;
int vdisk_fd;

struct FCB {
  char filename[1000];
  char owner[1000];
  uint32_t size;
  time_t created_at;
  time_t last_modified_at;
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

  write_block(bitmap, BITMAP_BLOCK);
}
