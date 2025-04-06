#include "simple_file_system.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_VDISK_FILENAME 255

int main(int argc, char **argv) {
  // Argument 1: name of file for virtual disk
  // Argument 2: Number of Blocks to be alloted to virtual disk (Each block is
  // 4K Bytes)

  char vdisk_name[MAX_VDISK_FILENAME + 1];
  int num_blocks;

  if (argc != 3) {
    printf("Incorrect Format!\nCorrect format is: ./create_vdisk <virtual disk "
           "name> <number of blocks>");
    return -1;
  }

  strcpy(vdisk_name, argv[1]);
  num_blocks = atoi(argv[2]);

  printf("LOG: Creating Virtual disk %s...\n", vdisk_name);
  int status = create_format_vdisk(vdisk_name, num_blocks);
  if (status < 0) {
    printf("ERROR: Some problem occured while creating virutal disk");
    return -1;
  } else {
    printf("LOG: Successfully created virutal disk %s", vdisk_name);
  }
}
