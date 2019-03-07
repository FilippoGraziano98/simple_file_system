#pragma once
#include "disk_driver.h"
#include "bitmap.h"
#include "linked_list.h"

#define NAME_LEN 128

/*these are structures stored on disk*/

// header, occupies the first portion of each block in the disk
// represents a chained list of blocks (each chain represents a file)
typedef struct {
	int previous_block; // chained list (previous block)
	int next_block;		 	// chained list (next_block)
	int block_in_file; 	// position in the file, if 0 we have a file control block
} BlockHeader;


// this is in the first block of a chain, after the header
typedef struct {
	int directory_block; 	// first block of the parent directory
	int block_in_disk;	 	// repeated position of the block on the disk
	char name[NAME_LEN];
	int	size_in_bytes;		//contano solo i byte di dati, non la memoria utilizzata per le strutture di controllo
	int size_in_blocks;
	int is_dir;						// 0 for file, 1 for dir
} FileControlBlock;

// this is the first physical block of a file
// it has a header
// an FCB storing file infos
// and can contain some data

/******************* stuff on disk BEGIN *******************/
typedef struct {
	BlockHeader header;
	FileControlBlock fcb;
	char data[BLOCK_SIZE-sizeof(FileControlBlock) - sizeof(BlockHeader)] ;
} FirstFileBlock;

// this is one of the next physical blocks of a file
typedef struct {
	BlockHeader header;
	char	data[BLOCK_SIZE-sizeof(BlockHeader)];
} FileBlock;

// this is the first physical block of a directory
typedef struct {
	BlockHeader header;
	FileControlBlock fcb;
	int num_entries;
	int file_blocks[ (BLOCK_SIZE
			 -sizeof(BlockHeader)
			 -sizeof(FileControlBlock)
				-sizeof(int))/sizeof(int) ];//array of first_blocks of files
} FirstDirectoryBlock;

// this is remainder block of a directory
typedef struct {
	BlockHeader header;
	int file_blocks[ (BLOCK_SIZE-sizeof(BlockHeader))/sizeof(int) ];
} DirectoryBlock;
/******************* stuff on disk END *******************/

typedef struct {
	ListItem list;
	FirstDirectoryBlock* dir_start;
	int handler_cnt;
} OpenDirectoryInfo;

//TODO add modify flag, to see if write back or not
//TOOD add counter of ptr to file/dir, to free struct when no one using it any more
typedef struct {
	ListItem list;
	FirstFileBlock* file_start;
	int handler_cnt;
} OpenFileInfo;
	
typedef struct {
	DiskDriver* disk;
	// add more fields if needed
	ListHead OpenFiles;
	ListHead OpenDirectories;
	OpenDirectoryInfo* current_directory_block; //the current directory block
} SimpleFS;

// this is a file handle, used to refer to open files
typedef struct {
	SimpleFS* sfs;												// pointer to memory file system structure
	OpenFileInfo* globalOpenFileInfo;			// pointer to the first block of the file(read it)
	OpenDirectoryInfo* parent_directory;	// pointer to the directory where the file is stored
	BlockHeader* current_block;						// current block in the file
	int pos_in_file;											// absolute position of the cursor
	int pos_in_block;											// relative position of the cursor in the block
} FileHandle;

typedef struct {
	SimpleFS* sfs;															// pointer to memory file system structure
	OpenDirectoryInfo* globalOpenDirectoryInfo;	// pointer to the first block of the directory(read it)
	OpenDirectoryInfo* parent_directory;				// pointer to the parent directory (null if top level)
	BlockHeader* current_block;									// current block in the directory (last block)
	int pos_in_dir;															// absolute position of the cursor in the directory
	int pos_in_block;														// relative position of the cursor in the block
} DirectoryHandle;


/**
 *NOTE: I substituted all ptrs to blocks (FirstDirectoryBlock*, ...)
 *				with block indexes (to which I can access through DiskDriver_readBlock)
 * 				otherwise I had to possibilities:
 *					- store a ptr to a block in the mmapped region
 *							bad, since I want to access disk only through DiskDriver
 *					- store a ptr to a copy of the block in the mmaped region
 *							bad, I should have to bother keeping them congruent
 *							and what if I have a two handles to the same file
 *		NO! now I only mmap info block at the beginning
 *			so read file/directory blocks only when needed and bring them to memory
 * TODO but when am I supposed to release parent_directory ptr??
 *		it may happen it has already been freed!
 */

// initializes a file system on an already made disk
	// if disk empty, creates root directory /
	//else just returns a handle to it
// returns a handle to the top level directory stored in the first block
DirectoryHandle* SimpleFS_init(SimpleFS* fs, DiskDriver* disk);

// creates the inital structures, the top level directory
// has name "/" and its control block is in the first position
// it also clears the bitmap of occupied blocks on the disk
// the current_directory_block is cached in the SimpleFS struct
// and set to the top level directory
void SimpleFS_format(SimpleFS* fs);

// creates an empty file in the directory d
// returns null on error (file existing, no free blocks)
// an empty file consists only of a block of type FirstBlock
FileHandle* SimpleFS_createFile(DirectoryHandle* d, const char* filename);

// reads in the (preallocated) blocks array, the name of all files in a directory 
int SimpleFS_readDir(char** names, DirectoryHandle* d);


// opens a file in the	directory d. The file should be exisiting
FileHandle* SimpleFS_openFile(DirectoryHandle* d, const char* filename);

// closes a file handle (destroyes it)
void SimpleFS_closeFile(FileHandle* f);

// closes a directory handle (destroyes it) [my add]
void SimpleFS_closeDirectory(DirectoryHandle* d);

// writes in the file, at current position for size bytes stored in data
// overwriting and allocating new space if necessary
// returns the number of bytes written
int SimpleFS_write(FileHandle* f, void* data, int size);

// reads from the file, at current position size bytes stored in data
// (reads until EOF if necessary, then stops)
// returns the number of bytes read
int SimpleFS_read(FileHandle* f, void* data, int size);

// returns the number of bytes read (moving the current pointer to pos)
// returns pos on success
// -1 on error (file too short)
int SimpleFS_seek(FileHandle* f, int pos);

// seeks for a directory in d. If dirname is equal to ".." it goes one level up
// 0 on success, negative value on error
// it does side effect on the provided handle
int SimpleFS_changeDir(DirectoryHandle* d, char* dirname);

// creates a new directory in the current one (passed as d)
// (stored in fs->current_directory_block)
// 0 on success
// -1 on error
int SimpleFS_mkDir(DirectoryHandle* d, char* dirname);

// removes the file in the current directory
// returns -1 on failure 0 on success
// if a directory, it removes recursively all contained files
int SimpleFS_remove(SimpleFS* fs, char* filename);

//closes all open files/directories
void SimpleFS_close(SimpleFS* fs);
	

