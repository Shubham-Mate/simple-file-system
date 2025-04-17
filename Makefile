CARGS = -Wall

libsimplefs.a: simple_file_system.c
	@echo "Creating library (.a) file from simple_file_system"
	@gcc $(CARGS) -c simple_file_system.c
	@ar -cvq libsimplefs.a simple_file_system.o
	@ranlib libsimplefs.a

create_vdisk: create_vdisk.c libsimplefs.a
	@echo "Compiling create_vdisk file"
	@gcc $(CARGS) -o create_vdisk  create_vdisk.c   -L. -lsimplefs

clean:
	@echo "Removing all files except source files..."
	@rm create_vdisk libsimplefs.a test


test: test.c
	gcc -Wall -o test  test.c   -L. -lsimplefs
