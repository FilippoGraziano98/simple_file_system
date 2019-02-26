#include "simplefs.h"
#include <string.h>	//strncpy
#include "error.h"

#define DEBUG 0

static void printInfo_dir(FirstDirectoryBlock* dir) {
	printf("\n******************************\n");
	printf("directory name: %s, (is_dir = %d [1])\n", dir->fcb.name, dir->fcb.is_dir);
	printf("directory starts at: %d, previous:%d,  next: %d\n", dir->fcb.block_in_disk, dir->header.previous_block, dir->header.next_block);
	printf("directory size: %d bytes, %d blocks\n", dir->fcb.size_in_bytes, dir->fcb.size_in_blocks);
	printf("parent directory at: %d\n",dir->fcb.directory_block);
	printf("num files in dir: %d\n",dir->num_entries);
	int i;
	for (i=0; i<(dir->num_entries); i++)
		printf(" - file at %d\n", dir->file_blocks[i]);
	printf("******************************\n\n");
}

static void printInfo_file(FirstFileBlock* file) {
	printf("\n******************************\n");
	printf("file name: %s, (is_dir = %d [0])\n", file->fcb.name, file->fcb.is_dir);
	printf("file starts at: %d, previous:%d,  next: %d\n", file->fcb.block_in_disk, file->header.previous_block, file->header.next_block);
	printf("file size: %d bytes, %d blocks\n", file->fcb.size_in_bytes, file->fcb.size_in_blocks);
	printf("parent directory at: %d\n",file->fcb.directory_block);
	printf("******************************\n\n");
}

static void printResume(SimpleFS* fs) {
	printf("##########\n");
	int err;
	int root_blk = fs->disk->header->bitmap_blocks;
	//printf("root at %d\n",root_blk);
	FirstDirectoryBlock root;
	err = DiskDriver_readBlock(fs->disk, &root, root_blk);
	check_err(err,"[printResume]Error reading FirstDirectoryBlock to on-memory image of disk");
	printInfo_dir(&root);
	
	int i;
	FirstFileBlock file;
	for (i=0; i<(root.num_entries); i++) {
		err = DiskDriver_readBlock(fs->disk, &file, root.file_blocks[i]);
		check_err(err,"[printResume]Error reading FirstFileBlock to on-memory image of disk");
		printInfo_file(&file);
	}
}

static int getBlockInDisk(SimpleFS* fs, BlockHeader* blk) {
	int err;
	//se è il primo blocco leggo dal fcb
	if(blk->block_in_file == 0) {
		FirstFileBlock* fb = (FirstFileBlock*)blk;
		return fb->fcb.block_in_disk;
	}
	//altrimenti vado al blocco precedente e leggo l'index del successivo
	BlockHeader pb;
	err = DiskDriver_readBlock(fs->disk, &pb, blk->previous_block);
	check_err(err,"[printResume]Error reading FirstFileBlock to on-memory image of disk");
	return pb.next_block;
}

DirectoryHandle* SimpleFS_init(SimpleFS* fs, DiskDriver* disk) {
	int err;
	
	//get first free block from disk
		//and occupies it with the root directory
	int first_free_block = DiskDriver_getFreeBlock(disk, 0);
	//I could check if 	num_blocks = free_blocks+bitmap_blocks
	check_err(first_free_block,"[SimpleFS_init]Appears there is no free block in disk. Check the disk is empty!");
	
	int root_directory_index;	
	FirstDirectoryBlock* root_directory_start = (FirstDirectoryBlock*)calloc(1, sizeof(FirstDirectoryBlock));
	//se il primo blocco libero è quello subito successivo alla bitmap
		//allora il disco è ancora vuoto
			//quindi creo la root directory
	if(first_free_block == disk->header->bitmap_blocks) {
		if(DEBUG) printf("[SimpleFS_init] creating root dir\n");
		root_directory_index = first_free_block;
		
		//inizilializzo il BlockHeader
		root_directory_start->header.previous_block = -1; //this is the first block of directory
		root_directory_start->header.next_block = -1;//this is the last block of directory
		root_directory_start->header.block_in_file = 0;
		//inizilializzo il FileControlBlock
		root_directory_start->fcb.directory_block = -1;//this is root directory (no parent)
		root_directory_start->fcb.block_in_disk = first_free_block;
		strncpy(root_directory_start->fcb.name,"/",NAME_LEN);
		root_directory_start->fcb.size_in_bytes = sizeof(BlockHeader)+sizeof(FileControlBlock)+sizeof(int);//size del FirstDirectoryBlock, escluso array file_blocks
		root_directory_start->fcb.size_in_blocks = 1;
		root_directory_start->fcb.is_dir = 1;
		//inizilializzo la lista di file
		root_directory_start->num_entries = 0;
		
		//TODO should I backup updates immediately on disk
			//or keep then in memory and store them to the disk when closing the handle?
		err = DiskDriver_writeBlock(disk, root_directory_start, first_free_block);
		check_err(err,"[SimpleFS_init]Error writing FirstDirectoryBlock to on-memory image of disk");
	} else {
		//se questo disco conteneva già un file system,
			//restituisco handle alla root directory attuale
		//(assumo che la root directory sia stata storata subito dopo la bitmap)
		if(DEBUG) printf("[SimpleFS_init] reading root dir\n");
		root_directory_index = disk->header->bitmap_blocks;
		
		err = DiskDriver_readBlock(disk, root_directory_start, root_directory_index);
		check_err(err,"[SimpleFS_init]Error reading FirstDirectoryBlock from on-memory image of disk");
		//controllo che il file/directory qui memorizzato non abbia una directory genitore
		if(root_directory_start->fcb.directory_block != -1){
			printf("[SimpleFS_init]Error: Is this disk storing a File System? Root directory not found on it");
			exit(1);
		}
	}
	
	//inizializzo il File System
	fs->disk = disk;
	List_init(&(fs->OpenFiles));
	List_init(&(fs->OpenDirectories));
	
	OpenDirectoryInfo* open_dir = (OpenDirectoryInfo*)malloc(sizeof(OpenDirectoryInfo));
	open_dir->list.prev = 0;
	open_dir->list.next = 0;
	open_dir->dir_start = root_directory_start;
	open_dir->handler_cnt = 1; //fs->current_directory_block count as a handler
	List_insert(&(fs->OpenDirectories), NULL, (ListItem*)open_dir);
	
	fs->current_directory_block = open_dir;
	
	//initialize DirectoryHandle for root directory "/"
		//located at first free block	
	DirectoryHandle* dir_handle = (DirectoryHandle*)malloc(sizeof(DirectoryHandle));
	dir_handle->sfs = fs;
	dir_handle->globalOpenDirectoryInfo = open_dir;
	open_dir->handler_cnt++;
	dir_handle->parent_directory = NULL;
	dir_handle->current_block = (BlockHeader*)malloc(sizeof(DirectoryBlock));
	err = DiskDriver_readBlock(disk, dir_handle->current_block, root_directory_index);
	check_err(err,"[SimpleFS_init]Error reading FirstDirectoryBlock from on-memory image of disk");
	dir_handle->pos_in_dir = root_directory_start->fcb.size_in_bytes;
	dir_handle->pos_in_block = root_directory_start->fcb.size_in_bytes % BLOCK_SIZE;
	
	printResume(fs);
	
	return dir_handle;
}

void SimpleFS_format(SimpleFS* fs) {		
	//check here we don't have dirty open directories/files list...
	SimpleFS_close(fs);
	int err;

	//clear the bitmap, except for the bitmap_blocks (which must stay occupied)
	int i;
	for(i=(fs->disk->header->bitmap_blocks); i<(fs->disk->header->num_blocks); i++) {
		err = DiskDriver_freeBlock(fs->disk, i);
		check_err(err, "[SimpleFS_format] Error freeing disk blocks");
	}

	//creates root directory "/"
	
	//get first free block from disk and occupies it with the root directory
	int first_free_block = DiskDriver_getFreeBlock(fs->disk, 0);
	//I could check if 	num_blocks = free_blocks+bitmap_blocks
	check_err(first_free_block,"[SimpleFS_format]Appears there is no free block in formatted disk");
	
	
	FirstDirectoryBlock* root_directory_start = (FirstDirectoryBlock*)calloc(1, sizeof(FirstDirectoryBlock));
	//inizilializzo il BlockHeader
	root_directory_start->header.previous_block = -1; //this is the first block of directory
	root_directory_start->header.next_block = -1;//this is the last block of directory
	root_directory_start->header.block_in_file = 0;
	//inizilializzo il FileControlBlock
	root_directory_start->fcb.directory_block = -1;//this is root directory (no parent)
	root_directory_start->fcb.block_in_disk = first_free_block;
	strncpy(root_directory_start->fcb.name,"/",NAME_LEN);
	root_directory_start->fcb.size_in_bytes = sizeof(BlockHeader)+sizeof(FileControlBlock)+sizeof(int);//size del FirstDirectoryBlock, escluso array file_blocks
	root_directory_start->fcb.size_in_blocks = 1;
	root_directory_start->fcb.is_dir = 1;
	//inizilializzo la lista di file
	root_directory_start->num_entries = 0;
	
	err = DiskDriver_writeBlock(fs->disk, root_directory_start, first_free_block);
	check_err(err,"[SimpleFS_format]Error writing FirstDirectoryBlock to on-memory image of disk");

	//inizializzo il File System
	List_init(&(fs->OpenFiles));
	List_init(&(fs->OpenDirectories));
	
	OpenDirectoryInfo* open_dir = (OpenDirectoryInfo*)malloc(sizeof(OpenDirectoryInfo));
	open_dir->list.prev = 0;
	open_dir->list.next = 0;
	open_dir->dir_start = root_directory_start;
	open_dir->handler_cnt = 1; //fs->current_directory_block count as a handler
	List_insert(&(fs->OpenDirectories), fs->OpenDirectories.last, (ListItem*)open_dir);
	
	fs->current_directory_block = open_dir;
}

//search for file with name filename in directory d
	//returns the block index of the first file block
	//if last_directory_block!=NULL it also fills it with the last directory block
static int SimpleFS_findFile_diskBlockIndex(DirectoryHandle* d, const char* filename, int* last_directory_block) {
	int err;
	if(!d)
		return -1;
	
	DiskDriver* disk = d->sfs->disk;
	
	if(last_directory_block)
		*last_directory_block = d->globalOpenDirectoryInfo->dir_start->fcb.block_in_disk;
	
	int num_files = d->globalOpenDirectoryInfo->dir_start->num_entries;
	int directory_size = d->globalOpenDirectoryInfo->dir_start->fcb.size_in_bytes;

	memcpy(d->current_block, d->globalOpenDirectoryInfo->dir_start, BLOCK_SIZE);
	//err = DiskDriver_readBlock(disk, d->current_block, d->globalOpenDirectoryInfo->dir_start->fcb.block_in_disk);
	//check_err(err,"[SimpleFS_findFile]Error reading FirstDirectoryBlock from on-memory image of disk");
	
	d->pos_in_block = sizeof(BlockHeader)+sizeof(FileControlBlock)+sizeof(int);
	d->pos_in_dir = d->pos_in_block;
	
	int i=0, file_block_index, next_block_index;
	FirstFileBlock file_block;
	char current_name[NAME_LEN];
	while(i < num_files) {
		if(d->pos_in_dir >= directory_size) {
			printf("[SimpleFS_findFile] Unexpected EOF(1) while reading directory %s\n",d->globalOpenDirectoryInfo->dir_start->fcb.name);
			return -1;
		}
		//I am accessing *(int*)((void*)(d->current_block)+(d->pos_in_block))
		if(d->pos_in_dir < BLOCK_SIZE)//if FirstDirectoryBlock
			file_block_index = ((FirstDirectoryBlock*)d->current_block)->file_blocks[i];
		else
			file_block_index = ((DirectoryBlock*)d->current_block)->file_blocks[i];
			
		err = DiskDriver_readBlock(disk, &file_block, file_block_index);
		check_err(err,"[SimpleFS_findFile]Error reading next FirstFileBlock from on-memory image of disk");
		
		strncpy(current_name, file_block.fcb.name, NAME_LEN);
		if(strncmp(current_name, filename, NAME_LEN) == 0) {
			return file_block_index;
		}
	
		i++;
		d->pos_in_block += sizeof(int);
		d->pos_in_dir += sizeof(int);
	
		if(d->pos_in_block >= BLOCK_SIZE) {
			d->pos_in_block -= BLOCK_SIZE;
			next_block_index = d->current_block->next_block;
			
			if(next_block_index != -1) {
				err = DiskDriver_readBlock(disk, d->current_block, next_block_index);
				check_err(err,"[SimpleFS_findFile]Error reading next DirectoryBlock from on-memory image of disk");
				if(last_directory_block)
					*last_directory_block = next_block_index;
			} else if(i < num_files) {
				printf("[SimpleFS_findFile] Unexpected EOF(2) while reading directory %s\n",d->globalOpenDirectoryInfo->dir_start->fcb.name);
				return -1;
			}
		}
	}
	return -1;
}

static void add_file_entry_in_dir(SimpleFS* fs, FirstDirectoryBlock* dir, int last_dir_blck_index, int new_file_index){
	int err;
	int curr_size_bytes = dir->fcb.size_in_bytes;
	int size_blcks = dir->fcb.size_in_blocks;
	int num_entries = dir->num_entries;
	
	DiskDriver* disk = fs->disk;
	
	if( (curr_size_bytes+sizeof(int)+BLOCK_SIZE-1)/BLOCK_SIZE > size_blcks ) {
		// we have to add a block to the directory
			//get a free block on disk for the file
		int first_free_block = DiskDriver_getFreeBlock(fs->disk, 0);
		check_err(first_free_block,"[add_file_entry_in_dir]Appears there is no free block in disk.");
		
		if(dir->fcb.block_in_disk == last_dir_blck_index) {//se ho un solo blocco
			dir->header.next_block = first_free_block;
			err = DiskDriver_writeBlock(fs->disk, dir, dir->fcb.block_in_disk);
			check_err(err,"[add_file_entry_in_dir]Error writing FirstDirectoryBlock to on-memory image of disk");
		} else {
			DirectoryBlock blk;
			err = DiskDriver_readBlock(disk, &blk, last_dir_blck_index);
			check_err(err,"[add_file_entry_in_dir]Error reading last_dir_blck_index from on-memory image of disk");
			
			blk.header.next_block = first_free_block;
			err = DiskDriver_writeBlock(fs->disk, (void*)&blk, last_dir_blck_index);
			check_err(err,"[add_file_entry_in_dir]Error writing FirstDirectoryBlock to on-memory image of disk");
		}
		
		DirectoryBlock next_blk;
		next_blk.header.previous_block = last_dir_blck_index;
		next_blk.header.next_block = -1;
		next_blk.header.block_in_file = size_blcks;
		
		err = DiskDriver_writeBlock(fs->disk, (void*)&next_blk, first_free_block);
		check_err(err,"[add_file_entry_in_dir]Error writing DirectoryBlock to on-memory image of disk");
		
		dir->fcb.size_in_blocks++;
		size_blcks = dir->fcb.size_in_blocks;
		last_dir_blck_index = first_free_block;
	}
	
	if(dir->fcb.size_in_blocks == 1)//se ho un solo blocco
		dir->file_blocks[num_entries] = new_file_index;
	else {
		DirectoryBlock blk;
		err = DiskDriver_readBlock(disk, &blk, last_dir_blck_index);
		check_err(err,"[add_file_entry_in_dir]Error reading last_dir_blck_index from on-memory image of disk");
		int index =  num_entries - (BLOCK_SIZE-sizeof(BlockHeader)-sizeof(FileControlBlock)-sizeof(int))/sizeof(int) - (size_blcks-2)*(BLOCK_SIZE-sizeof(BlockHeader))/sizeof(int);
		blk.file_blocks[index] = new_file_index;
		
		err = DiskDriver_writeBlock(disk, (void*)&blk, last_dir_blck_index);
		check_err(err,"[add_file_entry_in_dir]Error writing last_dir_blck_index from on-memory image of disk");
	}
	
	dir->num_entries++;
	dir->fcb.size_in_bytes += sizeof(int);
	err = DiskDriver_writeBlock(disk, dir, dir->fcb.block_in_disk);
	check_err(err,"[add_file_entry_in_dir]Error writing FirstDirectoryBlock to on-memory image of disk");
}

FileHandle* SimpleFS_createFile(DirectoryHandle* d, const char* filename) {
	if(DEBUG) printf("[SimpleFS_createFile] create: %s\n",filename);
	int err;
	if(!d)
		return NULL;
	
	SimpleFS* fs = d->sfs;
	DiskDriver* disk = fs->disk;
	
	int last_directory_block = -1;
	int file_block_index = SimpleFS_findFile_diskBlockIndex(d, filename, &last_directory_block);
	if(file_block_index > 0)	{ //file already exists
		if(DEBUG) printf("[SimpleFS_createFile] file already exists\n");
		return NULL;
	}
	
	//get a free block on disk for the file
	int first_free_block = DiskDriver_getFreeBlock(disk, 0);
	check_err(first_free_block,"[SimpleFS_createFile]Appears there is no free block in disk.");
	if(DEBUG) printf("\t[SimpleFS_createFile] free block for file: %d\n",first_free_block);
	//aggiungo nell'elenco dei file nella directory
		//l'indice del blocco appena aggiunto
	add_file_entry_in_dir(fs, d->globalOpenDirectoryInfo->dir_start, last_directory_block, first_free_block);

	//creo il file nel blocco libero individuato
	FirstFileBlock* file_start = (FirstFileBlock*)calloc(1, sizeof(FirstDirectoryBlock));
		//inizilializzo il BlockHeader
	file_start->header.previous_block = -1; //this is the first block of directory
	file_start->header.next_block = -1;//this is the last block of directory
	file_start->header.block_in_file = 0;
	//inizilializzo il FileControlBlock
	file_start->fcb.directory_block = d->globalOpenDirectoryInfo->dir_start->fcb.block_in_disk;
	file_start->fcb.block_in_disk = first_free_block;
	strncpy(file_start->fcb.name,filename,NAME_LEN);
	file_start->fcb.size_in_bytes = sizeof(BlockHeader)+sizeof(FileControlBlock);//size del FirstDirectoryBlock, escluso array data
	file_start->fcb.size_in_blocks = 1;
	file_start->fcb.is_dir = 0;
	
	//TODO should I backup updates immediately on disk
		//or keep then in memory and store them to the disk when closing the handle?
	err = DiskDriver_writeBlock(disk, file_start, first_free_block);
	check_err(err,"[SimpleFS_createFile]Error writing FirstFileBlock to on-memory image of disk");

	OpenFileInfo* open_file = (OpenFileInfo*)malloc(sizeof(OpenFileInfo));
	open_file->list.prev = 0;
	open_file->list.next = 0;
	open_file->file_start = file_start;
	open_file->handler_cnt = 0;
	List_insert(&(fs->OpenFiles), fs->OpenFiles.last, (ListItem*)open_file);
		
	//initialize FileHandle
	FileHandle* file_handle = (FileHandle*)malloc(sizeof(FileHandle));
	file_handle->sfs = fs;
	file_handle->globalOpenFileInfo = open_file;
	open_file->handler_cnt++;
	printf("[SimpleFS_createFile] handler cnts on global OpenFile : %d\n", open_file->handler_cnt);
	file_handle->parent_directory = d->globalOpenDirectoryInfo;
	d->globalOpenDirectoryInfo->handler_cnt++;
	file_handle->current_block = (BlockHeader*)malloc(sizeof(DirectoryBlock));
	err = DiskDriver_readBlock(disk, file_handle->current_block, first_free_block);
	check_err(err,"[SimpleFS_createFile]Error reading FirstFileBlock from on-memory image of disk");
	file_handle->pos_in_file = file_start->fcb.size_in_bytes;
	file_handle->pos_in_block = file_start->fcb.size_in_bytes % BLOCK_SIZE;
	
	printResume(fs);
	return file_handle;
}

int SimpleFS_readDir(char** names, DirectoryHandle* d) {
	int err;
	if(!d)
		return -1;
	
	DiskDriver* disk = d->sfs->disk;
	
	int num_files = d->globalOpenDirectoryInfo->dir_start->num_entries;
	int directory_size = d->globalOpenDirectoryInfo->dir_start->fcb.size_in_bytes;

	memcpy(d->current_block, d->globalOpenDirectoryInfo->dir_start, BLOCK_SIZE);

	d->pos_in_block = sizeof(BlockHeader)+sizeof(FileControlBlock)+sizeof(int);
	d->pos_in_dir = d->pos_in_block;
	
	int i=0, file_block_index, next_block_index;
	FirstFileBlock file_block;
	while(i < num_files) {
		if(d->pos_in_dir >= directory_size) {
			printf("[SimpleFS_readDir] Unexpected EOF(1) while reading directory %s\n",d->globalOpenDirectoryInfo->dir_start->fcb.name);
			return -1;
		}

		if(d->pos_in_dir < BLOCK_SIZE)//if FirstDirectoryBlock
			file_block_index = ((FirstDirectoryBlock*)d->current_block)->file_blocks[i];
		else {
			int index =  num_files - (BLOCK_SIZE-sizeof(BlockHeader)-sizeof(FileControlBlock)-sizeof(int))/sizeof(int) - (d->globalOpenDirectoryInfo->dir_start->fcb.size_in_blocks - 2)*(BLOCK_SIZE-sizeof(BlockHeader))/sizeof(int);
			file_block_index = ((DirectoryBlock*)d->current_block)->file_blocks[index];
		}
		
		if(DEBUG) printf("[SimpleFS_readDir] file at %d\n",file_block_index);
		err = DiskDriver_readBlock(disk, &file_block, file_block_index);
		check_err(err,"[SimpleFS_readDir]Error reading next FirstFileBlock from on-memory image of disk");
		
		if(DEBUG) printf("[SimpleFS_readDir] - %s\n",file_block.fcb.name);
		
		strncpy(names[i], file_block.fcb.name, NAME_LEN);
		
		i++;
		d->pos_in_block += sizeof(int);
		d->pos_in_dir += sizeof(int);
	
		if(d->pos_in_block >= BLOCK_SIZE) {
			d->pos_in_block -= BLOCK_SIZE;
			next_block_index = d->current_block->next_block;
			
			if(next_block_index != -1) {
				err = DiskDriver_readBlock(disk, d->current_block, next_block_index);
				check_err(err,"[SimpleFS_readDir]Error reading next DirectoryBlock from on-memory image of disk");
			} else if(i < num_files) {
				printf("[SimpleFS_readDir] Unexpected EOF(2) while reading directory %s\n",d->globalOpenDirectoryInfo->dir_start->fcb.name);
			return -1;
			} else {
				return i;
			}
		}
	}
	return i;
}

static OpenFileInfo* SimpleFs_findGlobalOpenFileInfo(SimpleFS* fs, const char* name, int directory_block) {
	ListItem* curr_file_item = fs->OpenFiles.first;
	
	OpenFileInfo* curr_file;
	FirstFileBlock* file_blk;
	
	while(curr_file_item) {
		curr_file = (OpenFileInfo*)curr_file_item;
		file_blk = curr_file->file_start;
		
		if(strncmp(file_blk->fcb.name, name, NAME_LEN)==0 && file_blk->fcb.directory_block==directory_block)
			return curr_file;
		
		curr_file_item = curr_file_item->next;
	}
	
	return NULL;
}

FileHandle* SimpleFS_openFile(DirectoryHandle* d, const char* filename) {
	if(DEBUG) printf("[SimpleFS_openFile] open: %s\n",filename);
	int err;
	if(!d)
		return NULL;
	
	SimpleFS* fs = d->sfs;
	DiskDriver* disk = fs->disk;
	
	int file_block_index = SimpleFS_findFile_diskBlockIndex(d, filename, NULL);
	if(file_block_index < 0)	{ //file does not exist
		if(DEBUG) printf("[SimpleFS_openFile] file does not exists\n");
		return NULL;
	}
	
	if(DEBUG) printf("[SimpleFS_openFile] file at block %d\n", file_block_index);
	
	OpenFileInfo* open_file;
	
	open_file = SimpleFs_findGlobalOpenFileInfo(fs, filename, d->globalOpenDirectoryInfo->dir_start->fcb.block_in_disk);
	if(DEBUG && open_file) printf("[SimpleFS_openFile] file in Global OpenFiles at %p\n", open_file);
	
	if(!open_file) {
		if(DEBUG) printf("[SimpleFS_openFile] file not in Global OpenFiles list\n");
		FirstFileBlock* file_start = (FirstFileBlock*)malloc(sizeof(FirstFileBlock));
		err = DiskDriver_readBlock(disk, file_start, file_block_index);
		check_err(err,"[SimpleFS_openFile]Error reading FirstFileBlock from on-memory image of disk");
	
		open_file = (OpenFileInfo*)malloc(sizeof(OpenFileInfo));
		open_file->list.prev = 0;
		open_file->list.next = 0;
		open_file->file_start = file_start;
		open_file->handler_cnt = 0;
		List_insert(&(fs->OpenFiles), fs->OpenFiles.last, (ListItem*)open_file);
	}
	
	//initialize FileHandle
	FileHandle* file_handle = (FileHandle*)malloc(sizeof(FileHandle));
	file_handle->sfs = fs;
	file_handle->globalOpenFileInfo = open_file;
	open_file->handler_cnt++;
	file_handle->parent_directory = d->globalOpenDirectoryInfo;
	d->globalOpenDirectoryInfo->handler_cnt++;
	file_handle->current_block = (BlockHeader*)malloc(sizeof(DirectoryBlock));
	err = DiskDriver_readBlock(disk, file_handle->current_block, file_block_index);
	check_err(err,"[SimpleFS_openFile]Error reading FirstFileBlock from on-memory image of disk");
	file_handle->pos_in_file = sizeof(BlockHeader)+sizeof(FileControlBlock);
	file_handle->pos_in_block = file_handle->pos_in_file % BLOCK_SIZE;
	
	//printResume(fs);
	return file_handle;
}

int SimpleFS_write(FileHandle* f, void* data, int size) {
	if(!f)
		return -1;
	SimpleFS* fs = f->sfs;
	DiskDriver* disk = fs->disk;
	int err, written_bytes=0;
	//recusively first write bytes in current block
	//then allocate new one and goes on
	int bytes_to_write=0, bytes_left=size, need_blk=0;
	
	while(bytes_left > 0) {
		if(size <= BLOCK_SIZE-f->pos_in_block) {
			bytes_to_write = bytes_left;
			need_blk = 1;
		} else {
			bytes_to_write = BLOCK_SIZE-f->pos_in_block;
			need_blk = 0;
		}
		bytes_left -= bytes_to_write;
	
		memcpy((char*)f->current_block + f->pos_in_block, data, bytes_to_write);
		written_bytes+=bytes_to_write;
	
		f->pos_in_file += bytes_to_write;
		f->pos_in_block += bytes_to_write;
	
	
		int size_bytes = f->globalOpenFileInfo->file_start->fcb.size_in_bytes;
		int new_size_bytes = (size_bytes > f->pos_in_file) ? size_bytes : f->pos_in_file;
	
		f->globalOpenFileInfo->file_start->fcb.size_in_bytes = new_size_bytes;
		
		if(need_blk) {
			//se ho riempito il blocco attuale
			if(f->current_block->next_block == -1) {
				//se non ho un prossimo blocco nel file, lo alloco
				int first_free_block = DiskDriver_getFreeBlock(disk, 0);
				if(first_free_block < 0) {
					printf("[SimpleFS_write]Appears there is no free block in disk.");
					//se ho finito lo spazio s disco ritorno il numero di byte scritti finora
					return written_bytes;
				}
						
				f->current_block->next_block = first_free_block;
				f->globalOpenFileInfo->file_start->fcb.size_in_blocks++;
			}
			int curr_blk_index_in_disk = getBlockInDisk(fs, f->current_block);
			int curr_blk_in_file = f->current_block->block_in_file;
			int next_blk_index_in_disk = f->current_block->next_block;
			//switcho al prossimo blocco
			err = DiskDriver_writeBlock(disk, f->current_block, curr_blk_index_in_disk);
			check_err(err,"[SimpleFS_createFile]Error writing CurrentBlock to on-memory image of disk");
		
			//passo in f->current_block al prossimo
			memset(f->current_block, 0, BLOCK_SIZE);
				//inizilializzo il BlockHeader
			f->current_block->previous_block = curr_blk_index_in_disk;
			f->current_block->next_block = -1; //questo è ora l'ultimo blocco
			f->current_block->block_in_file = curr_blk_in_file+1;
			err = DiskDriver_writeBlock(disk, f->current_block, next_blk_index_in_disk);
			check_err(err,"[SimpleFS_createFile]Error writing FirstFileBlock to on-memory image of disk");
		
			f->pos_in_block = sizeof(BlockHeader);
		}
	}
	return written_bytes;
}

void SimpleFS_closeFile(FileHandle* f) {
	if(!f){
		if(DEBUG) printf("[SimpleFS_closeFile] invalid reference\n");
		return;
	}
	if(DEBUG) printf("[SimpleFS_closeFile] closing file handle : block %d\n", f->globalOpenFileInfo->file_start->fcb.block_in_disk);
	SimpleFS* fs = f->sfs;
	//TODO should I write back to disk changes
	f->globalOpenFileInfo->handler_cnt--;
	if(DEBUG) printf("[SimpleFS_closeFile] handler cnts on global OpenFile : %d\n", f->globalOpenFileInfo->handler_cnt);
	if(f->globalOpenFileInfo->handler_cnt < 1) {
		ListItem* item = List_detach(&(fs->OpenFiles), (ListItem*)f->globalOpenFileInfo);
		if(DEBUG) assert(item==(ListItem*)f->globalOpenFileInfo, "AssertionError: detatched item does not match");
		free(f->globalOpenFileInfo->file_start);
		free(f->globalOpenFileInfo);
	}
	
	f->parent_directory->handler_cnt--;
	if(f->parent_directory->handler_cnt < 1) {
		ListItem* item = List_detach(&(fs->OpenDirectories), (ListItem*)f->parent_directory);
		if(DEBUG) assert(item==(ListItem*)f->parent_directory, "AssertionError: detatched item does not match");
		free(f->parent_directory->dir_start);
		free(f->parent_directory);
	}
	
	free(f->current_block);
	free(f);
}

void SimpleFS_closeDirectory(DirectoryHandle* d) {
	if(!d){
		if(DEBUG) printf("[SimpleFS_closeDirectory] invalid reference\n");
		return;
	}
	if(DEBUG) printf("[SimpleFS_closeDirectory] closing directory handle : block %d\n", d->globalOpenDirectoryInfo->dir_start->fcb.block_in_disk);
	SimpleFS* fs = d->sfs;
	//TODO should I write back to disk changes
	
	d->globalOpenDirectoryInfo->handler_cnt--;
	if(DEBUG) printf("[SimpleFS_closeDirectory] handler cnts on global OpenDirectories : %d\n", d->globalOpenDirectoryInfo->handler_cnt);
	if(d->globalOpenDirectoryInfo->handler_cnt < 1) {
		ListItem* item = List_detach(&(fs->OpenDirectories), (ListItem*)d->globalOpenDirectoryInfo);
		if(DEBUG) assert(item==(ListItem*)d->globalOpenDirectoryInfo, "AssertionError: detatched item does not match");
		free(d->globalOpenDirectoryInfo->dir_start);
		free(d->globalOpenDirectoryInfo);
	}
	
	if(d->parent_directory) {
		d->parent_directory->handler_cnt--;
		if(d->parent_directory->handler_cnt < 1) {
			ListItem* item = List_detach(&(fs->OpenDirectories), (ListItem*)d->parent_directory);
			if(DEBUG) assert(item==(ListItem*)d->parent_directory, "AssertionError: detatched item does not match");
			free(d->parent_directory->dir_start);
			free(d->parent_directory);
		}
	}
	
	free(d->current_block);
	free(d);
}

void SimpleFS_close(SimpleFS* fs) {
	if(!fs){
		if(DEBUG) printf("[SimpleFS_close] invalid reference\n");
		return;
	}
	if(DEBUG) printf("[SimpleFS_close] closing file system\n");
	OpenFileInfo* open_file;
	while(fs->OpenFiles.size > 0){
		open_file = (OpenFileInfo*)List_detach(&(fs->OpenFiles), fs->OpenFiles.first);
		free(open_file->file_start);
		free(open_file);
	}
	OpenDirectoryInfo* open_dir;
	while(fs->OpenDirectories.size > 0){
		open_dir = (OpenDirectoryInfo*)List_detach(&(fs->OpenDirectories), fs->OpenDirectories.first);
		free(open_dir->dir_start);
		free(open_dir);
	}
}
