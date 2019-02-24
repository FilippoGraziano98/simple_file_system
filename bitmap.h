#pragma once
#include <stdint.h>
typedef struct{
  int num_bits;
  char* entries;	//ogni char nel'array conterrà 8 bit, e quindi 8 entry nell bitmap
}  BitMap;

typedef struct {
  int entry_num;	//numero del char corrispondente
  char bit_num;		//offset all'interno del char [0,7]
} BitMapEntryKey;

// converts a block index to an index in the array,
// and a char that indicates the offset of the bit inside the array
BitMapEntryKey BitMap_blockToIndex(int num);

// converts a bit to a linear index
int BitMap_indexToBlock(int entry, uint8_t bit_num);

// returns the index of the first bit having status "status"
// in the bitmap bmap, and starts looking from position start
// search will go on circularly when reached the end of bitmap (until bumping into start again)
// if no bit has status "status" returns -1
int BitMap_get(BitMap* bmap, int start, int status);

// sets the bit at index pos in bmap to status
// returns previous status of the bit
// -1 if pos exceeds bmap->num_bits
int BitMap_set(BitMap* bmap, int pos, int status);
