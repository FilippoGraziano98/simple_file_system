#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bitmap.h"

int main(int argc, char** argv) {
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
	char mask;
	int j, left;
	for(i=0; i<(size+7)/8; i++){
		mask = 0;
		left = (size - i*8)>8 ? 8 : (size - i*8)%8;
		for(j=0; j<left; j++)
			mask = (mask<<1)|1;
		if( ((bmap->entries)[i]&0xFF) != (mask&0xFF)) printf("[set_1 b] something wrong in bitmap at char %d : 0x%x [expected 0x%x]\n",i,(bmap->entries)[i]&0xFF, mask&0xFF);
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
		mask = 0;
		left = (size - i*8)>8 ? 8 : (size - i*8)%8;
		for(j=0; j<left; j++)
			mask = (j%2==0) ? (mask<<1)|1 : (mask<<1);
		if( ((bmap->entries)[i]&0xFF) != (mask&0xFF)) printf("[set_mod2 b] something wrong in bitmap at char %d : 0x%x [expected 0x%x]\n",i,(bmap->entries)[i]&0xFF, mask&0xFF);
	}
	
	for(i=0; i<size; i++){
		s = BitMap_get(bmap, i, 1);
		if(s!=(i+(i+1)%2)%size) printf("[get_mod2]pos %d search 1 -> should have been %d, instead %d\n",i,(i+(i+1)%2),s);
		s = BitMap_get(bmap, i, 0);
		if(i!=99 && s!=(i+i%2)%size) printf("[get_mod2]pos %d search 0 -> should have been %d, instead %d\n",i,(i+i%2),s);
		else if(i==99 && s!=-1) printf("[get_mod2]pos %d search 0 -> should have been -1, instead %d\n",i,s);
	}
	printf("[README]prints only in case of error\n\tno prints = awesome\n");
	
	free(bmap->entries);
	free(bmap);
}
