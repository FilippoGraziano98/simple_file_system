#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bitmap.h"

int main() {
	int size = 100;
	BitMap* bmap = (BitMap*)malloc(sizeof(BitMap));
	bmap->num_bits = size;
	bmap->entries = (char*)malloc(sizeof(char)*((size+7)/8));
	memset(bmap->entries, 0, sizeof(char)*((size+7)/8));
	
	int i, s;
	for(i=0; i<size; i++){
		s = BitMap_set(bmap, i, 1);
		if(s!=0) printf("[set_1]pos %d should have been 0, instead %d\n",i,s);
	}
	for(i=0; i<(size+7)/8; i++){
		if((bmap->entries)[i]&0xFF != 0xFF) printf("something wrong in bitmap at char %d : %x\n",i,(bmap->entries)[i]&0xFF);
	}
	s = BitMap_get(bmap, 0, 0);
	if(s!=-1) printf("[get_0]pos %d all bits should be 1, instead found a 0 at %d\n",i,s);
	for(i=0; i<size; i++){
		s = BitMap_get(bmap, i, 1);
		if(s!=i) printf("[get_1]pos %d should have been 1, instead %d\n",i,s);
	}
	
	s = BitMap_set(bmap, 0, 0);
	if(s!=1) printf("[set_mod2]pos 0 should have been 1, instead %d\n",s);
	s = BitMap_get(bmap, 1, 0);
	if(s!=-1) printf("[get_0bis]pos %d all bits after pos 0 should be 1, instead found a 0 at %d\n",i,s);
	
	for(i=0; i<size; i++){
		s = BitMap_set(bmap, i, i%2);
		if(i && s!=1) printf("[set_mod2]pos %d should have been 1, instead %d\n",i,s);
	}
	for(i=0; i<(size+7)/8; i++){
		if((bmap->entries)[i]&0xFF != 0xAA) printf("something wrong in bitmap at char %d : %x\n",i,(bmap->entries)[i]&0xFF);
	}
	
	for(i=0; i<size; i++){
		s = BitMap_get(bmap, i, 1);
		if(s!=(i+(i+1)%2)%size) printf("[get_mod2]pos %d search 1 -> should have been %d, instead %d\n",i,(i+(i+1)%2),s);
		s = BitMap_get(bmap, i, 0);
		if(i!=99 && s!=(i+i%2)%size) printf("[get_mod2]pos %d search 0 -> should have been %d, instead %d\n",i,(i+i%2),s);
		else if(i==99 && s!=-1) printf("[get_mod2]pos %d search 0 -> should have been -1, instead %d\n",i,s);
	}
	printf("[README]prints only in case of error\n\tno prints = awesome\n");
}
