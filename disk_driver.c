#include "disk_driver.h"
#include "error.h"
#include "bitmap.h"
#include <unistd.h>	
#include <stdio.h>
#include <stdlib.h> 
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>

#define DEBUG 0
#define BLOCK_SIZE 512

static void DiskDriver_printInfo(DiskHeader* dh) {
	printf("=========\n");
	printf("DISK DRIVER INITIALISED ...\n");
	printf("\t num_blocks : %d\n", dh -> num_blocks);
	printf("\t bitmap_blocks : %d\n", dh -> bitmap_blocks);
	printf("\t bitmap_entries : %d\n", dh -> bitmap_entries);
	printf("\t free_blocks : %d\n", dh -> free_blocks);
	printf("\t first_free_block (stored in header) : %d\n", dh -> first_free_block);
	
	
	BitMap bitmap;
	bitmap.num_bits = dh->num_blocks;
	bitmap.entries = ((void*)dh)+sizeof(DiskHeader);
	printf("\t first_free_block (actual in bitmap) : %d\n", BitMap_get(&bitmap, 0, 0));
	printf("=========\n");
}

static void DiskDriver_initDiskHeader(DiskHeader* dh, int num_blocks, int entries, int free_blocks, int first_free_block){
	dh -> num_blocks = num_blocks;
	dh -> bitmap_blocks = (num_blocks + BLOCK_SIZE-1)/BLOCK_SIZE;  
	dh -> bitmap_entries = entries;  
  
	dh -> free_blocks = free_blocks;     
	dh -> first_free_block = first_free_block;
}


void DiskDriver_init(DiskDriver* disk, const char* filename, int num_blocks) {
	int ret;
	int fd = open(filename, O_CREAT | O_RDWR, 0600);
	//prendo la size del file
	off_t size_fs = lseek(fd, 0, SEEK_END);
	if(DEBUG) printf("[DiskDriver_init]Opened File System \"%s\" of size %ld\n", filename, (long)size_fs);
	lseek(fd, 0, SEEK_SET);
	
	
	int bitmap_size = (num_blocks+7) >> 3;
	
	int dd_size = sizeof(DiskHeader) + bitmap_size;
	
	
	if(DEBUG) printf("[DiskDriver_init] size : %d\n",dd_size + num_blocks*BLOCK_SIZE);
	
	ret = posix_fallocate(fd, 0, dd_size + num_blocks*BLOCK_SIZE);
	check_err(ret, "fallocate failed\n");
	
	void* dd_data = mmap(0, dd_size + num_blocks*BLOCK_SIZE,  PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	check_err((long)dd_data, "mmap failed\n");
	
	if(DEBUG) printf("[DiskDriver_init] dd_data mmaped at %p\n", dd_data);
	
	disk->header = (DiskHeader*)dd_data;
	disk->bitmap_data = sizeof(DiskHeader) + (char*)dd_data;
	disk->fd = fd;

	if (size_fs == 0) {
		/* init diskheader */
    	DiskDriver_initDiskHeader(disk->header, num_blocks, bitmap_size, num_blocks, 0);
    	*(disk->bitmap_data) &= 0;
	}
	DiskDriver_printInfo(disk->header);
	
	if(DEBUG) printf("[DiskDriver_init] disk->header at %p\n", disk->header);
}


int DiskDriver_readBlock(DiskDriver* disk, void* dest, int block_num) {
	if(!dest || block_num >= disk->header->num_blocks) {
		printf("[DiskDriver_readBlock] invalid index of block to read : %d\n",block_num);
		return -1;
	}

	BitMap bitmap;
	bitmap.num_bits = disk->header->num_blocks;
	bitmap.entries = disk->bitmap_data;
	//check if block in pos block_num is not free
	int first_occupied_block = BitMap_get(&bitmap, block_num, 1);
	if(first_occupied_block!=block_num) { //wanted block is free
		printf("[DiskDriver_readBlock] wanted block is free : %d\n",block_num);
		return -1;
	}
	
	if(DEBUG) printf("[DiskDriver_readBlock] index of block to read : %d\n",block_num);
	if(DEBUG) printf("[DiskDriver_readBlock] starting address : %p\n",disk->bitmap_data + disk->header->bitmap_entries + (block_num * BLOCK_SIZE));
	memcpy(dest, disk->bitmap_data + disk->header->bitmap_entries + (block_num * BLOCK_SIZE), BLOCK_SIZE);

	return 0;
}


int DiskDriver_writeBlock(DiskDriver* disk, void* src, int block_num) {
	if(!src || block_num >= disk->header->num_blocks)
		return -1;
	if(DEBUG) printf("\t[DiskDriver_writeBlock] write on block %d\n",block_num);

	BitMap bitmap;
	bitmap.num_bits = disk->header->num_blocks;
	bitmap.entries = disk->bitmap_data;


	int old_status = BitMap_set(&bitmap, block_num, 1);
	if(old_status == -1)
		return -1;
	else if(old_status == 0)
		disk->header->free_blocks--;

	memcpy(disk->bitmap_data + disk->header->bitmap_entries + (block_num * BLOCK_SIZE), src, BLOCK_SIZE);

	if (block_num == disk->header->first_free_block)
		disk->header->first_free_block = BitMap_get(&bitmap, 0, 0);
		
	return 0;
}


int DiskDriver_freeBlock(DiskDriver* disk, int block_num) {
	if (block_num >= disk->header->num_blocks)
		return -1;
	
	BitMap bitmap;
	bitmap.num_bits = disk->header->num_blocks;
	bitmap.entries = disk->bitmap_data;

	if (BitMap_set(&bitmap, block_num, 0) == -1)
		return -1;

	disk->header->free_blocks++;
	
	if (block_num <	disk->header->first_free_block)
		disk->header->first_free_block = block_num;
	
	return 0;
}


int DiskDriver_getFreeBlock(DiskDriver* disk, int start) {
	if (start >= disk->header->num_blocks)
		return -1;
	
	if (start < disk->header->first_free_block)
		return disk->header->first_free_block;
	
	BitMap bitmap;
	bitmap.num_bits = disk->header->num_blocks;
	bitmap.entries = disk->bitmap_data;

	return BitMap_get(&bitmap, start, 0);
}


int DiskDriver_flush(DiskDriver* disk) {
	int ret;
	int dd_size = sizeof(DiskHeader) + disk->header->bitmap_entries + disk->header->num_blocks * BLOCK_SIZE;
	if(DEBUG) printf("[DiskDriver_flush] dd_size : %d\n",dd_size);
	if(DEBUG) printf("[DiskDriver_flush] mmapped at %p\n", disk->header);
	ret = msync((void*)(disk->header), dd_size, MS_ASYNC);
	if (ret == -1)
	    return -1;
	return 0;
}

