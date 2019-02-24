#pragma once
#include "bitmap.h"

#define BLOCK_SIZE 512

extern const char MAGIC_NUMBERS[8];

// this is stored in the 1st block of the disk
typedef struct {
	char magic_numbers[sizeof(MAGIC_NUMBERS)];
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

// opens the file (creating it if necessary)
// allocates the necessary space on the disk
// calculates how big the bitmap should be
// if the file was new
// compiles a disk header, and fills in the bitmap of appropriate size
// with all 0 (to denote the free space);
void DiskDriver_init(DiskDriver* disk, const char* filename, int num_blocks);

// reads the block in position block_num
// (copies BLOCK_SIZE bytes into dest from the file system's image [fd])
// returns -1 if the block is free accrding to the bitmap
// 0 otherwise
int DiskDriver_readBlock(DiskDriver* disk, void* dest, int block_num);

// writes a block in position block_num, and alters the bitmap accordingly
// (copies BLOCK_SIZE bytes from src to the file system's image [fd])
// returns -1 if operation not possible
int DiskDriver_writeBlock(DiskDriver* disk, void* src, int block_num);

// frees a block in position block_num, and alters the bitmap accordingly
// returns -1 if operation not possible
int DiskDriver_freeBlock(DiskDriver* disk, int block_num);

// returns the first free block in the disk from position (checking the bitmap)
// without setting it as occupied in the bitmap
int DiskDriver_getFreeBlock(DiskDriver* disk, int start);

// writes the data (flushing the mmaps)
int DiskDriver_flush(DiskDriver* disk);

//munmpas the mmapped blocks and closes fd
int DiskDriver_close(DiskDriver* disk);