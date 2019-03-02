#include "disk_driver.h"
#include "error.h"
#include <stdio.h>
#include <stdlib.h> 

#define BLOCK_SIZE 512

// this is stored in the 1st block of the disk
typedef struct {
  int num_blocks;
  int bitmap_blocks;   // how many blocks in the bitmap
  int bitmap_entries;  // how many bytes are needed to store the bitmap
  
  int free_blocks;     // free blocks
  int first_free_block;// first block index
} DiskHeader; 

typedef struct {
  DiskHeader* header; // mmapped
  char* bitmap_data;  // mmapped (bitmap)
  int fd; // for us
} DiskDriver;

/**
   The blocks indices seen by the read/write functions 
   have to be calculated after the space occupied by the bitmap
*/


static void DiskDriver_initDiskHeader(DiskHeade* dh, int num_blocks, int entries, int free_blocks, int first_free_block){
	
	dh -> num_blocks = num_blocks;
	dh -> bitmap_blocks = num_blocks;  
	dh -> bitmap_entries = entries;  
  
	dh -> free_blocks = free_blocks;     
	dh -> first_free_block = first_free_block;

}


void DiskDriver_init(DiskDriver* disk, const char* filename, int num_blocks) {

	int ret;
	int fd = open(filename, O_CREAT | O_RDWR, 0600);
	
	int bitmap_size = num_blocks >> 3;
	
	int dd_size = sizeof(DiskHeader) + bitmap_size;
	
	void* dd_data = mmap(0, dd_size,  PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	check_err(dd_data == -1, "mmap failed\n");
	
	disk->header = (DiskHeader*)dd_data;
	disk->bitmap_data = sizeof(DiskHeader) + (char*)dd_data;
	disk->fd = fd;

	if (fd < 0) {
  		/* failure */
  		if (errno == EEXIST) {
    		/* the file already existed */
			ret = posix_fallocate(disk->fd, disk->header->num_blocks * BLOCK_SIZE, dd_size);
            check_err(ret != 0, "fallocate failed\n"); 

            disk->header->bitmap_entries &= 0;
		}
	} else {
  		ret = posix_fallocate(fd, 0, dd_size);
	    check_err(ret != 0, "fallocate failed\n");
	    /* init diskheader */
	    DiskDriver_initDiskHeader(disk->header, num_blocks, bitmap_size, num_blocks, 0);
	}	
	
	disk->bitmap_data &= 0;
	
}

