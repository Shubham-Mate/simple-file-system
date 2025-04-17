#include "simple_file_system.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

void is_res_pass(int res) {
  if (res < 0) {
    exit(-1);
  }
}

void test_create_and_delete() {
  struct timeval start, end;
  int res_create;
  gettimeofday(&start, NULL);

  char *vfs_name = "vdisk";

  printf("* create_format_vdisk **\n");
  res_create = create_format_vdisk(vfs_name, 20); // NOTE: Max disk size 128 MB
  is_res_pass(res_create);
  gettimeofday(&end, NULL);

  printf("* Timing for formatting the disk **\n");
  printf("\tSeconds: %ld\n", end.tv_sec - start.tv_sec);
  printf("\tMicroseconds: %ld\n", (end.tv_sec * 1000000 + end.tv_usec) -
                                      (start.tv_sec * 1000000 + start.tv_usec));

  printf("* sfs_mount **\n");
  int res_mount = sfs_mount(vfs_name);
  is_res_pass(res_mount);

  // Create a single file and then delete it
  printf("* sfs_create **\n");
  int res_create_file = sfs_create("vfs_test.txt");
  is_res_pass(res_create_file);

  // Re-create and open the file
  sfs_create("vfs_test.txt");
  int fd = sfs_open("vfs_test.txt", READ_MODE);
  if (fd < 0) {
    printf("Error opening file\n");
    return;
  }

  // Test reading data back
  int read_data = 0;
  sfs_read(fd, &read_data, sizeof(int));
  printf("Data read: %d\n", read_data);

  // Close the file
  sfs_close(fd);

  // Unmount the file system
  printf("* sfs_umount **\n");
  int res_umount = sfs_umount();
  if (res_umount < 0) {
    printf("ERROR: Can't umount the file system!\n");
  }
  printf("[test] success!\n");
}

void test_multiple_file_operations() {
  struct timeval start, end;
  int res_create;
  gettimeofday(&start, NULL);

  char *vfs_name = "vfs_multi_files";
  printf("* create_format_vdisk (Multiple Files) **\n");
  res_create = create_format_vdisk(vfs_name, 20); // NOTE: Max disk size 128 MB
  is_res_pass(res_create);
  gettimeofday(&end, NULL);

  printf("* Timing for formatting the disk **\n");
  printf("\tSeconds: %ld\n", end.tv_sec - start.tv_sec);
  printf("\tMicroseconds: %ld\n", (end.tv_sec * 1000000 + end.tv_usec) -
                                      (start.tv_sec * 1000000 + start.tv_usec));

  printf("* sfs_mount **\n");
  int res_mount = sfs_mount(vfs_name);
  is_res_pass(res_mount);

  // Create multiple files
  char filename_test[100];
  for (int i = 0; i < 10; i++) {
    sprintf(filename_test, "file_%d.txt", i);
    res_create = sfs_create(filename_test);
    int fd = sfs_open(filename_test, WRITE_MODE);
    int buffer[2] = {i, i + 1};
    sfs_write(fd, buffer, sizeof(int));
    sfs_seek(fd, 0, SFS_SEEK_SET);
    sfs_close(fd);
    is_res_pass(res_create);
  }

  // Read data from files
  for (int i = 0; i < 10; i++) {
    sprintf(filename_test, "file_%d.txt", i);
    int fd = sfs_open(filename_test, READ_MODE);
    if (fd < 0) {
      printf("Error opening file %d\n", i);
      return;
    }
    sfs_seek(fd, 0, SFS_SEEK_SET);
    int read_data = 0;
    sfs_read(fd, &read_data, sizeof(int));
    printf("Data in file_%d.txt: %d\n", i, read_data);
    sfs_close(fd);
  }

  // Unmount the file system
  printf("* sfs_umount **\n");
  int res_umount = sfs_umount();
  if (res_umount < 0) {
    printf("ERROR: Can't umount the file system!\n");
  }
  printf("[test] success!\n");
}

int main(int argc, char **argv) {
  test_create_and_delete();
  test_multiple_file_operations();
  return 0;
}
