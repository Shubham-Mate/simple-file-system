
int init_FCB();

void init_bitmap();

void init_superblock(int total_blocks, int available_blocks, int total_fcbs);

void init_root_directory();

int create_format_vdisk(char *vdiskname, unsigned int m);
