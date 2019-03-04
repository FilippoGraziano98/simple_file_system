#include "disk_driver.h"
#include "error.h"
#include "bitmap.h"
#include <stdio.h>
#include <stdlib.h> 

#define BLOCK_SIZE 512

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


int DiskDriver_readBlock(DiskDriver* disk, void* dest, int block_num) {
	if(!disk->bitmap_data[block_num]) {
		return -1;
	} 
	
	memcpy(dest, disk->bitmap_data + disk->header->bitmap_entries + (block_num * BLOCK_SIZE), BLOCK_SIZE);
	return 0;
}


int DiskDriver_writeBlock(DiskDriver* disk, void* src, int block_num) {

	memcpy(disk->bitmap_data + disk->header->bitmap_entries + (block_num * BLOCK_SIZE) ,src , BLOCK_SIZE); // oppure disk_bitmap_data[block_num] + disk->header->bitmap_entries ?

	BitMap* bitmap;
	bitmap->num_bits = disk->header->bitmap_blocks;
	bitmap->entries = disk->bitmap_data;

	if (bitmap_set(bmap, block_num, 1) == -1) {
		return -1;
	}
	
	disk->header->free_blocks--;
	
	return 0;
}


