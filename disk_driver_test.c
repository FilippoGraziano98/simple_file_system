#include <stdio.h>
#include <string.h>

#include "bitmap.h"
#include "disk_driver.h"

int main(int argc, char** argv) {
  int err;
  
  DiskDriver dd;
	DiskDriver_init(&dd, "fs", 200);
	char junk[BLOCK_SIZE];
	int i;
	for(i=0;i<BLOCK_SIZE;i++)
		junk[i] = 97+(i%21);
	
	err = DiskDriver_writeBlock(&dd, junk, 2);
//	if(err==-1) printf("error in DiskDriver_writeBlock 0\n");
	printf("line 19: DiskDriver_writeBlock(&dd, junk, 2);\n");
	err = DiskDriver_writeBlock(&dd, junk, 2);
	if(err!=-1) printf("OK DiskDriver_writeBlock 1: I should not have been blocked (even if block 2 already occupied)\n");
	
//	printf("DiskDriver_writeBlock tests passed\n");
	
	char dump[BLOCK_SIZE+1];
	memset(dump,0,BLOCK_SIZE+1);
	printf("line 27: DiskDriver_readBlock(&dd, dump, 4);\n");
	err = DiskDriver_readBlock(&dd, dump, 4);
	if(err!=-1) printf("DiskDriver_writeBlock 1: I should have been blocked (since block 4 is free)\n");
	printf("line 30: DiskDriver_readBlock(&dd, dump, 2);\n");
	err = DiskDriver_readBlock(&dd, dump, 2);
	if(err==-1) printf("error in DiskDriver_readBlock 0\n");
	
	printf("read: %s\n", dump);
	
	if(memcmp(junk,dump,BLOCK_SIZE)!=0) printf("Incostincency, not reading what I wrote in\n");
		
	printf("line 38: DiskDriver_freeBlock(&dd, 2);\n");
	err = DiskDriver_freeBlock(&dd, 2);
	if(err==-1) printf("error freeing block 0\n");
	err = DiskDriver_getFreeBlock(&dd, 2);
	if(err==-1) printf("error DiskDriver_getFreeBlock block 0\n");
	else printf("first_free_block from 2 is:%d(expected 2)\n",err);
	
	printf("line 45: DiskDriver_writeBlock(&dd, junk, 2);\n");
	err = DiskDriver_writeBlock(&dd, junk, 2);
	if(err==-1) printf("error in DiskDriver_writeBlock 0\n");
	printf("line 48: DiskDriver_getFreeBlock(&dd, 2);\n");
	err = DiskDriver_getFreeBlock(&dd, 2);
	if(err==-1) printf("error DiskDriver_getFreeBlock block 0\n");
	else printf("first_free_block from 2 is:%d(expected 3)\n",err);
	
	DiskDriver_flush(&dd);
}
