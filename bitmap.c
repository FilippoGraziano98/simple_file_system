#include "bitmap.h"
#include "stdio.h"
#include "stdlib.h"

BitMapEntryKey BitMap_blockToIndex(int num) {
	BitMapEntryKey entry;
	entry.entry_num = num/8;		// numero del blocco da 8 bit (char) corrispondente
	entry.bit_num = num % 8;		// offset all'interno del char	
	return entry;
}

int BitMap_indexToBlock(int entry, uint8_t bit_num) {
	return entry*8 + bit_num;			
}

int BitMap_get(BitMap* bmap, int start, int status) {
	int index = start;
	while (index < bmap->num_bits) {
        BitMapEntryKey entry = BitMap_blockToIndex(index);
		int bit_value = bmap->entries[entry.entry_num] >> entry.bit_num & 1;
		if (bit_value == status)
			return index;
		index++;	
	}
	return -1;
}

int BitMap_set(BitMap* bmap, int pos, int status) {
	if (bmap->num_bits < pos)
		return -1;
	BitMapEntryKey entry = BitMap_blockToIndex(pos);
	int previous_status = bmap->entries[entry.entry_num] >> entry.bit_num & 1;
	if (status) {
		bmap->entries[entry.entry_num] |= 1 << entry.bit_num;
	} else {
		bmap->entries[entry.entry_num] &= ~(1 << entry.bit_num);
	}
	return previous_status;
}
