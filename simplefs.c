#include "simplefs.h"
#include <string.h>	//strncpy
#include "error.h"


#define DEBUG 0


static void printInfo_dir(FirstDirectoryBlock* dir) {
	if(!dir) {
		printf("[printInfo_dir] Invalid arguments\n");
		return;	
	}
	
	printf("\n******************************\n");
	printf("directory name: %s, (is_dir = %d [1])\n", dir->fcb.name, dir->fcb.is_dir);
	printf("directory starts at: %d, previous:%d,  next: %d\n", dir->fcb.block_in_disk, dir->header.previous_block, dir->header.next_block);
	printf("directory size: %d bytes, %d blocks\n", dir->fcb.size_in_bytes, dir->fcb.size_in_blocks);
	printf("parent directory at: %d\n",dir->fcb.directory_block);
	printf("num files in dir: %d\n",dir->num_entries);
/*	int i;*/
/*	for (i=0; i<(dir->num_entries); i++)*/
/*		printf(" - file at %d\n", dir->file_blocks[i]);*/
	printf("******************************\n\n");
}

static void printInfo_file(FirstFileBlock* file) {
	if(!file) {
		printf("[printInfo_file] Invalid arguments\n");
		return;	
	}
	
	printf("\n******************************\n");
	printf("file name: %s, (is_dir = %d [0])\n", file->fcb.name, file->fcb.is_dir);
	printf("file starts at: %d, previous:%d,  next: %d\n", file->fcb.block_in_disk, file->header.previous_block, file->header.next_block);
	printf("file size: %d bytes, %d blocks\n", file->fcb.size_in_bytes, file->fcb.size_in_blocks);
	printf("parent directory at: %d\n",file->fcb.directory_block);
	int head_len=0;
	if(file->fcb.size_in_blocks > 1)	
		head_len = BLOCK_SIZE-sizeof(FileControlBlock)-sizeof(BlockHeader);
	else
		head_len = file->fcb.size_in_bytes-sizeof(FileControlBlock)-sizeof(BlockHeader);
	printf("head of data:\n");
	int i;
	for(i=0; i<head_len; i++)
		printf("0x%x ", file->data[i]);
	printf("\n");
	printf("******************************\n\n");
}

static void printResume(SimpleFS* fs) {
	if(!fs) {
		printf("[printResume] Invalid arguments\n");
		return;	
	}
	
	printf("##########\n");
	int err;
	int root_blk = FIRST_DIRECTORY_BLOCK;
	//printf("root at %d\n",root_blk);
	FirstDirectoryBlock root;
	err = DiskDriver_readBlock(fs->disk, &root, root_blk);
	check_err(err,"[printResume]Error reading FirstDirectoryBlock to on-memory image of disk");
	printInfo_dir(&root);
	
	//following code useful for debug only for directories of at most 1 block
/*	int i;*/
/*	FirstFileBlock file;*/
/*	for (i=0; i<(root.num_entries); i++) {*/
/*		err = DiskDriver_readBlock(fs->disk, &file, root.file_blocks[i]);*/
/*		check_err(err,"[printResume]Error reading FirstFileBlock to on-memory image of disk");*/
/*		if(file.fcb.is_dir)*/
/*			printInfo_dir((FirstDirectoryBlock*)&file);*/
/*		else*/
/*			printInfo_file(&file);*/
/*	}*/
}

static void SimpleFs_printGlobalOpenFiles(SimpleFS* fs) {
	if(!fs) {
		printf("[SimpleFs_printGlobalOpenFiles] Invalid arguments\n");
		return;	
	}
	
	ListItem* curr_file_item = fs->OpenFiles.first;
	
	printf("globalOpenFiles:\n");
	
	OpenFileInfo* curr_file;
	FirstFileBlock* file_blk;
	
	while(curr_file_item) {
		curr_file = (OpenFileInfo*)curr_file_item;
		file_blk = curr_file->file_start;
		printf("-%s %d\n",file_blk->fcb.name, curr_file->handler_cnt);
		
		curr_file_item = curr_file_item->next;
	}
}

static void SimpleFs_printGlobalOpenDirs(SimpleFS* fs) {
	if(!fs) {
		printf("[SimpleFs_printGlobalOpenDirs] Invalid arguments\n");
		return;	
	}
	
	ListItem* curr_file_item = fs->OpenDirectories.first;
	
	printf("globalOpenDirs:\n");
	
	OpenDirectoryInfo* curr_file;
	FirstDirectoryBlock* file_blk;
	
	while(curr_file_item) {
		curr_file = (OpenDirectoryInfo*)curr_file_item;
		file_blk = curr_file->dir_start;
		printf("-%s %d\n",file_blk->fcb.name, curr_file->handler_cnt);
		
		curr_file_item = curr_file_item->next;
	}
}

static int getBlockInDisk(SimpleFS* fs, BlockHeader* blk) {
	if(!fs || !blk) {
		printf("[getBlockInDisk] Invalid arguments\n");
		exit(1);	
	}
	
	int err;
	//se è il primo blocco leggo dal fcb
	if(blk->block_in_file == 0) {
		FirstFileBlock* fb = (FirstFileBlock*)blk;
		return fb->fcb.block_in_disk;
	}
	if(DEBUG) printf("[getBlockInDisk] not first block in file\n");
	//altrimenti vado al blocco precedente e leggo l'index del successivo
	
	if(DEBUG) printf("[getBlockInDisk] previous block in file/dir is: %d\n",blk->previous_block);
	
	BlockHeader* pb = (BlockHeader*)calloc(1,BLOCK_SIZE);
	err = DiskDriver_readBlock(fs->disk, pb, blk->previous_block);
	check_err(err,"[getBlockInDisk]Error reading FirstFileBlock to on-memory image of disk");
	
	int block_index = pb->next_block;
	free(pb);
	
	if(DEBUG) printf("[getBlockInDisk] next block of prev in file/dir is: %d\n",block_index);
	return block_index;
}

DirectoryHandle* SimpleFS_init(SimpleFS* fs, DiskDriver* disk) {
	if(!fs || !disk) {
		printf("[SimpleFS_init] Invalid arguments\n");
		return NULL;		
	}
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
	if(first_free_block == FIRST_DIRECTORY_BLOCK) {
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
		root_directory_start->fcb.size_in_bytes = sizeof(int);
		root_directory_start->fcb.size_in_blocks = (sizeof(BlockHeader)+sizeof(FileControlBlock)+sizeof(int) + BLOCK_SIZE-1)/BLOCK_SIZE;
		root_directory_start->fcb.is_dir = 1;
		//inizilializzo la lista di file
		root_directory_start->num_entries = 0;
		
		err = DiskDriver_writeBlock(disk, root_directory_start, first_free_block);
		check_err(err,"[SimpleFS_init]Error writing FirstDirectoryBlock to on-memory image of disk");
	} else {
		//se questo disco conteneva già un file system,
			//restituisco handle alla root directory attuale
		//(assumo che la root directory sia stata storata subito dopo la bitmap)
		if(DEBUG) printf("[SimpleFS_init] reading root dir\n");
		root_directory_index = FIRST_DIRECTORY_BLOCK;
		
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
	
	OpenDirectoryInfo* open_dir = (OpenDirectoryInfo*)calloc(1,sizeof(OpenDirectoryInfo));
	open_dir->list.prev = 0;
	open_dir->list.next = 0;
	open_dir->dir_start = root_directory_start;
	open_dir->handler_cnt = 0;
	List_insert(&(fs->OpenDirectories), NULL, (ListItem*)open_dir);
	
	//initialize DirectoryHandle for root directory "/"
		//located at first free block	
	DirectoryHandle* dir_handle = (DirectoryHandle*)calloc(1,sizeof(DirectoryHandle));
	dir_handle->sfs = fs;
	dir_handle->globalOpenDirectoryInfo = open_dir;
	open_dir->handler_cnt++;
	dir_handle->parent_directory = NULL;
	dir_handle->current_block = (BlockHeader*)calloc(1,sizeof(DirectoryBlock));	
	err = DiskDriver_readBlock(disk, dir_handle->current_block, root_directory_index);
	check_err(err,"[SimpleFS_init]Error reading FirstDirectoryBlock from on-memory image of disk");
	while(dir_handle->current_block->next_block > 0) {
		err = DiskDriver_readBlock(disk, dir_handle->current_block, dir_handle->current_block->next_block);
		check_err(err,"[add_file_entry_in_dir]Error reading next DirectoryBlock from on-memory image of disk");
	}
	dir_handle->pos_in_dir = sizeof(int) + root_directory_start->num_entries*sizeof(int);
	if(dir_handle->current_block->previous_block < 0)
		dir_handle->pos_in_block = sizeof(BlockHeader)+sizeof(FileControlBlock)+sizeof(int)+root_directory_start->num_entries*sizeof(int);
	else {
		int entries_in_first_b = (BLOCK_SIZE-sizeof(BlockHeader)-sizeof(FileControlBlock)-sizeof(int))/sizeof(int);
		int entries_in_othr_b = (BLOCK_SIZE-sizeof(BlockHeader))/sizeof(int);
		int num_othr_b = dir_handle->current_block->block_in_file-1;
		dir_handle->pos_in_block = sizeof(BlockHeader)+(root_directory_start->num_entries - entries_in_first_b - num_othr_b*entries_in_othr_b)*sizeof(int);
	}
	
	printResume(fs);
	if(DEBUG) printf("pos_in_dir %d , pos_in_block %d\n",dir_handle->pos_in_dir,dir_handle->pos_in_block);
	
	return dir_handle;
}

void SimpleFS_format(SimpleFS* fs) {
	if(!fs) {
		printf("[SimpleFS_format] Invalid arguments\n");
		return;		
	}
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
	root_directory_start->fcb.size_in_bytes = sizeof(int);
	root_directory_start->fcb.size_in_blocks = (sizeof(BlockHeader)+sizeof(FileControlBlock)+sizeof(int) + BLOCK_SIZE-1)/BLOCK_SIZE;
	root_directory_start->fcb.is_dir = 1;
	//inizilializzo la lista di file
	root_directory_start->num_entries = 0;
	
	err = DiskDriver_writeBlock(fs->disk, root_directory_start, first_free_block);
	check_err(err,"[SimpleFS_format]Error writing FirstDirectoryBlock to on-memory image of disk");

	//inizializzo il File System
	List_init(&(fs->OpenFiles));
	List_init(&(fs->OpenDirectories));
	
	OpenDirectoryInfo* open_dir = (OpenDirectoryInfo*)calloc(1,sizeof(OpenDirectoryInfo));
	open_dir->list.prev = 0;
	open_dir->list.next = 0;
	open_dir->dir_start = root_directory_start;
	open_dir->handler_cnt = 0;
	List_insert(&(fs->OpenDirectories), fs->OpenDirectories.last, (ListItem*)open_dir);
}

//search for file with name filename in directory d
	//returns the block index of the first file block
	//if is_dir is not NULL, it will be set to 1 if dir, 0 if file
static int SimpleFS_findFile_diskBlockIndex(DirectoryHandle* d, const char* filename, int* is_dir) {
	if(!d || !filename) {
		printf("[SimpleFS_findFile_diskBlockIndex] Invalid arguments\n");
		exit(1);	
	}

	if(DEBUG) printf("[SimpleFS_findFile] look for: %s\n",filename);
	int err;
	
	DiskDriver* disk = d->sfs->disk;
	int res=-1;
	
	//default I initialize it as file, change it if dir
	if(is_dir)
		*is_dir = 0;
	
	int num_files = d->globalOpenDirectoryInfo->dir_start->num_entries;
	int directory_size = d->globalOpenDirectoryInfo->dir_start->fcb.size_in_bytes;

	//scorrerò la directory usando a local block as current_block
	BlockHeader* aux_block = (BlockHeader*)calloc(1,BLOCK_SIZE);
	memcpy(aux_block, d->globalOpenDirectoryInfo->dir_start, BLOCK_SIZE);
	//err = DiskDriver_readBlock(disk, d->current_block, d->globalOpenDirectoryInfo->dir_start->fcb.block_in_disk);
	//check_err(err,"[SimpleFS_findFile]Error reading FirstDirectoryBlock from on-memory image of disk");
	
	int aux_pos_in_block = sizeof(BlockHeader)+sizeof(FileControlBlock)+sizeof(int);
	int aux_pos_in_dir = sizeof(int);
	
	int i=0, file_block_index, next_block_index;
	FirstFileBlock file_block;
	char current_name[NAME_LEN];
	while(i < num_files) {
		if(DEBUG) {
			printf("[SimpleFS_findFile_diskBlockIndex] searching at index %d\n",i);
			printf("[SimpleFS_findFile_diskBlockIndex]aux_pos_in_dir %d, directory_size %d\n",aux_pos_in_dir, directory_size);
		}
		if(aux_pos_in_dir >= directory_size) {
			printf("[SimpleFS_findFile] Unexpected EOF(1) while reading directory %s\n",d->globalOpenDirectoryInfo->dir_start->fcb.name);
			goto end_findFile;
		}
		//I am accessing *(int*)((void*)(d->current_block)+(d->pos_in_block))
		if( aux_block->previous_block == -1 )//if FirstDirectoryBlock
			file_block_index = ((FirstDirectoryBlock*)aux_block)->file_blocks[i];
		else {
			int block_in_file = aux_block->block_in_file;
			int index =  i - (BLOCK_SIZE-sizeof(BlockHeader)-sizeof(FileControlBlock)-sizeof(int))/sizeof(int) - (block_in_file - 1)*(BLOCK_SIZE-sizeof(BlockHeader))/sizeof(int);
			file_block_index = ((DirectoryBlock*)aux_block)->file_blocks[index];
			if(DEBUG) {
				printf("[SimpleFS_findFile_diskBlockIndex] in frist block %lu indexes\n",(BLOCK_SIZE-sizeof(BlockHeader)-sizeof(FileControlBlock)-sizeof(int))/sizeof(int));
				printf("[SimpleFS_findFile_diskBlockIndex] in other blocks %lu indexes\n",(BLOCK_SIZE-sizeof(BlockHeader))/sizeof(int));
				printf("[SimpleFS_findFile_diskBlockIndex] there are %d other blocks\n",block_in_file - 1);
				printf("[SimpleFS_findFile_diskBlockIndex] accessing at index %d\n",index);
			}
		}
		
		if(DEBUG) printf("[SimpleFS_findFile] next FirstFileBlock at %d\n", file_block_index);
		err = DiskDriver_readBlock(disk, &file_block, file_block_index);
		check_err(err,"[SimpleFS_findFile]Error reading next FirstFileBlock from on-memory image of disk");
		
		strncpy(current_name, file_block.fcb.name, NAME_LEN);
		if(DEBUG) printf("[SimpleFS_findFile] next filename : %s\n",current_name);
		if(strncmp(current_name, filename, NAME_LEN) == 0) {
			if(is_dir && file_block.fcb.is_dir) //if dir set as dir
				*is_dir = 1;
			res = file_block_index;
			goto end_findFile;
		}
	
		i++;
		aux_pos_in_block += sizeof(int);
		aux_pos_in_dir += sizeof(int);
	
		if(aux_pos_in_block >= BLOCK_SIZE) {
		
			if(DEBUG)
				printf("[SimpleFS_findFile_diskBlockIndex] switch block\n");
			aux_pos_in_block = sizeof(BlockHeader);
			next_block_index = aux_block->next_block;
			
			if(next_block_index != -1) {
				err = DiskDriver_readBlock(disk, aux_block, next_block_index);
				check_err(err,"[SimpleFS_findFile]Error reading next DirectoryBlock from on-memory image of disk");
			} else if(i < num_files) {
				printf("[SimpleFS_findFile] Unexpected EOF(2) while reading directory %s, looking for %s\n",d->globalOpenDirectoryInfo->dir_start->fcb.name, filename);
				goto end_findFile;
			}
		}
	}
	end_findFile:
		free(aux_block);
		return res;
}

static void SimpleFS_addFileEntry_inDir(SimpleFS* fs, DirectoryHandle* d, int new_file_index) {
	if(!fs || !d) {
		printf("[SimpleFS_addFileEntry_inDir] Invalid arguments\n");
		exit(1);		
	}
	
	int err;

	DiskDriver* disk = fs->disk;

	int curr_pos_in_block = d->pos_in_block;
	int size_blcks = d->globalOpenDirectoryInfo->dir_start->fcb.size_in_blocks;
	int num_entries = d->globalOpenDirectoryInfo->dir_start->num_entries;

	if(DEBUG){
		printf("[SimpleFS_addFileEntry_inDir] add file_index: %d\n",new_file_index);
		printf("[SimpleFS_addFileEntry_inDir] curr_pos_in_block: %d\n",curr_pos_in_block);
		printf("[SimpleFS_addFileEntry_inDir] size_blcks: %d\n",size_blcks);
		printf("[SimpleFS_addFileEntry_inDir] num_entries: %d\n",num_entries);
		
	}

	while(d->current_block->next_block > 0) {
		printf("[add_file_entry_in_dir] Warning: current_block was supposed to be still storing last block!!\n");
		err = DiskDriver_readBlock(disk, d->current_block, d->current_block->next_block);
		check_err(err,"[add_file_entry_in_dir]Error reading next DirectoryBlock from on-memory image of disk");
	}
	
	if( curr_pos_in_block+sizeof(int) > BLOCK_SIZE ) {
		if(DEBUG) printf("##### ADD NEW BLOCK!! #####\n");
	
		// we have to add a block to the directory
			//get a free block on disk for the file
		int first_free_block = DiskDriver_getFreeBlock(disk, 0);
		check_err(first_free_block,"[add_file_entry_in_dir]Appears there is no free block in disk.");
		
		int curr_last_blk_index;
		
		if(d->globalOpenDirectoryInfo->dir_start->fcb.size_in_blocks == 1) {//se ho un solo blocco
			curr_last_blk_index = d->globalOpenDirectoryInfo->dir_start->fcb.block_in_disk;
		
			d->globalOpenDirectoryInfo->dir_start->header.next_block = first_free_block;
			
			//need wirte back now not to crash in getBlockInDisk at line 390
			err = DiskDriver_writeBlock(disk, d->globalOpenDirectoryInfo->dir_start, d->globalOpenDirectoryInfo->dir_start->fcb.block_in_disk);
			check_err(err,"[add_file_entry_in_dir]Error writing FirstDirectoryBlock to on-memory image of disk");
		} else {			
			curr_last_blk_index = getBlockInDisk(fs, d->current_block);
			
			d->current_block->next_block = first_free_block;
			err = DiskDriver_writeBlock(disk, d->current_block, curr_last_blk_index);
			check_err(err,"[add_file_entry_in_dir]Error writing DirectoryBlock to on-memory image of disk");
		}
		
		//passo in f->current_block al prossimo
		d->pos_in_block = sizeof(BlockHeader);
		memset(d->current_block, 0, BLOCK_SIZE);
			//inizilializzo il BlockHeader
		d->current_block->previous_block = curr_last_blk_index;
		d->current_block->next_block = -1; //questo è ora l'ultimo blocco
		d->current_block->block_in_file = size_blcks;
			//write back at line 338
		//err = DiskDriver_writeBlock(disk, d->current_block, first_free_block);
		//check_err(err,"[add_file_entry_in_dir]Error writing new DirectoryBlock to on-memory image of disk");
		
		d->globalOpenDirectoryInfo->dir_start->fcb.size_in_blocks++;
		//err = DiskDriver_writeBlock(fs->disk, d->globalOpenDirectoryInfo->dir_start, d->globalOpenDirectoryInfo->dir_start->fcb.block_in_disk);
		//check_err(err,"[add_file_entry_in_dir]Error writing FirstDirectoryBlock to on-memory image of disk");
		
		size_blcks = d->globalOpenDirectoryInfo->dir_start->fcb.size_in_blocks;
	}
	
	if(DEBUG) printf("[add_file_entry_in_dir] curr_num_entries : %d, add file_index: %d, num blocks: %d\n", num_entries, new_file_index, d->globalOpenDirectoryInfo->dir_start->fcb.size_in_blocks);
	if(d->globalOpenDirectoryInfo->dir_start->fcb.size_in_blocks == 1)//se ho un solo blocco
		d->globalOpenDirectoryInfo->dir_start->file_blocks[num_entries] = new_file_index;
	else {
		int index =  num_entries - (BLOCK_SIZE-sizeof(BlockHeader)-sizeof(FileControlBlock)-sizeof(int))/sizeof(int) - (size_blcks-2)*(BLOCK_SIZE-sizeof(BlockHeader))/sizeof(int);
		if(DEBUG) printf("[add_file_entry_in_dir] accessing at index: %d\n", index);
		((DirectoryBlock*)(d->current_block))->file_blocks[index] = new_file_index;
		
		err = DiskDriver_writeBlock(disk, d->current_block, getBlockInDisk(fs, d->current_block));
		check_err(err,"[add_file_entry_in_dir]Error writing last DirectoryBlock from on-memory image of disk");
	}
	
	d->globalOpenDirectoryInfo->dir_start->num_entries++;
	d->globalOpenDirectoryInfo->dir_start->fcb.size_in_bytes += sizeof(int);
	d->pos_in_block += sizeof(int);
	d->pos_in_dir += sizeof(int);
	err = DiskDriver_writeBlock(disk, d->globalOpenDirectoryInfo->dir_start, d->globalOpenDirectoryInfo->dir_start->fcb.block_in_disk);
	check_err(err,"[add_file_entry_in_dir]Error writing FirstDirectoryBlock to on-memory image of disk");
}

FileHandle* SimpleFS_createFile(DirectoryHandle* d, const char* filename) {
	if(!d || !filename) {
		printf("[SimpleFS_createFile] Invalid arguments\n");
		return NULL;		
	}
	
	if(DEBUG) printf("[SimpleFS_createFile] create: %s\n",filename);
	int err;
	
	SimpleFS* fs = d->sfs;
	DiskDriver* disk = fs->disk;
	
	int file_block_index = SimpleFS_findFile_diskBlockIndex(d, filename, NULL);
	if(file_block_index > 0)	{ //file already exists
		printf("[SimpleFS_createFile] a file with name %s already exists\n",filename);
		return NULL;
	}
	
	//get a free block on disk for the file
	int new_file_index = DiskDriver_getFreeBlock(disk, 0);
	check_err(new_file_index,"[SimpleFS_createFile]Appears there is no free block in disk.");
	if(DEBUG) printf("\t[SimpleFS_createFile] free block for file: %d\n",new_file_index);
	//aggiungo nell'elenco dei file nella directory
		//l'indice del blocco appena aggiunto
	
	//creo il file nel blocco libero individuato
	FirstFileBlock* file_start = (FirstFileBlock*)calloc(1, sizeof(FirstFileBlock));
		//inizilializzo il BlockHeader
	file_start->header.previous_block = -1; //this is the first block of directory
	file_start->header.next_block = -1;//this is the last block of directory
	file_start->header.block_in_file = 0;
	//inizilializzo il FileControlBlock
	file_start->fcb.directory_block = d->globalOpenDirectoryInfo->dir_start->fcb.block_in_disk;
	file_start->fcb.block_in_disk = new_file_index;
	strncpy(file_start->fcb.name,filename,NAME_LEN);
	file_start->fcb.size_in_bytes = 0;
	file_start->fcb.size_in_blocks = (sizeof(BlockHeader)+sizeof(FileControlBlock) + BLOCK_SIZE-1)/BLOCK_SIZE;
	file_start->fcb.is_dir = 0;
	
	err = DiskDriver_writeBlock(disk, file_start, new_file_index);
	check_err(err,"[SimpleFS_createFile]Error writing FirstFileBlock to on-memory image of disk");

	//add file_entry in dir list
		//after writing file on disk, so DiskDriver_getFreeBlock we'll see file_block as occupied
			//and not use it as a new block for the directory in case it was needed
	if(DEBUG) printf("SimpleFS_createFile calls add_file_entry_in_dir\n");
	SimpleFS_addFileEntry_inDir(fs, d, new_file_index);

	OpenFileInfo* open_file = (OpenFileInfo*)calloc(1,sizeof(OpenFileInfo));
	open_file->list.prev = 0;
	open_file->list.next = 0;
	open_file->file_start = file_start;
	open_file->handler_cnt = 0;
	List_insert(&(fs->OpenFiles), NULL, (ListItem*)open_file);
		
	//initialize FileHandle
	FileHandle* file_handle = (FileHandle*)calloc(1,BLOCK_SIZE);
	file_handle->sfs = fs;
	file_handle->globalOpenFileInfo = open_file;
	open_file->handler_cnt++;
	//printf("[SimpleFS_createFile] handler cnts on global OpenFile : %d\n", open_file->handler_cnt);
	file_handle->parent_directory = d->globalOpenDirectoryInfo;
	d->globalOpenDirectoryInfo->handler_cnt++;
	file_handle->current_block = (BlockHeader*)calloc(1,sizeof(DirectoryBlock));
	memcpy(file_handle->current_block, open_file->file_start, BLOCK_SIZE);
	//err = DiskDriver_readBlock(disk, file_handle->current_block, new_file_index);
	//check_err(err,"[SimpleFS_createFile]Error reading FirstFileBlock from on-memory image of disk");
	file_handle->pos_in_file = 0;
	file_handle->pos_in_block = sizeof(BlockHeader)+sizeof(FileControlBlock) % BLOCK_SIZE;
	
	if(DEBUG) printResume(fs);
	if(DEBUG) printf("pos_in_dir %d , pos_in_block %d\n\n###################\n\n",d->pos_in_dir,d->pos_in_block);
	return file_handle;
}

int SimpleFS_readDir(char** names, int* is_dir, DirectoryHandle* d) {
	if(!d || !names) {
		printf("[SimpleFS_readDir] Invalid arguments\n");
		return -1;		
	}
	
	int err;
	
	DiskDriver* disk = d->sfs->disk;
	
	int num_files = d->globalOpenDirectoryInfo->dir_start->num_entries;
	int directory_size = d->globalOpenDirectoryInfo->dir_start->fcb.size_in_bytes;

	//scorrerò la lista in d->current_block
		//tanto dovendola scorrere tutta lascerò sempre l'ultimo blocco in current_block
	memcpy(d->current_block, d->globalOpenDirectoryInfo->dir_start, BLOCK_SIZE);

	d->pos_in_block = sizeof(BlockHeader)+sizeof(FileControlBlock)+sizeof(int);
	d->pos_in_dir = sizeof(int);
	
	int i=0, file_block_index, next_block_index;
	
	BlockHeader* file_block = (BlockHeader*)malloc(BLOCK_SIZE);
	FileControlBlock* file_fcb = ((void*)file_block)+sizeof(BlockHeader);
	while(i < num_files) {
		if(DEBUG) printf("[SimpleFS_readDir] %d\n",i);
		if(d->pos_in_dir >= directory_size) {
			printf("[SimpleFS_readDir] Unexpected EOF(1) while reading directory %s\n",d->globalOpenDirectoryInfo->dir_start->fcb.name);
			return -1;
		}
		
/*		int j;*/
/*		for (j=0; j<5; j++)*/
/*			printf("[SimpleFS_readDir] in dir %d-th file in block %d\n",j,((FirstDirectoryBlock*)d->current_block)->file_blocks[j]);*/

		if( d->current_block->previous_block == -1 ) {//if FirstDirectoryBlock
			file_block_index = ((FirstDirectoryBlock*)d->current_block)->file_blocks[i];
			if(DEBUG) printf("[SimpleFS_readDir] still in FirstDirectoryBlock, next block_index %d\n",file_block_index);
		}
		else {
			int block_in_file = d->current_block->block_in_file;
			int index =  i - (BLOCK_SIZE-sizeof(BlockHeader)-sizeof(FileControlBlock)-sizeof(int))/sizeof(int) - (block_in_file - 1)*(BLOCK_SIZE-sizeof(BlockHeader))/sizeof(int);
			if(DEBUG) printf("[SimpleFS_readDir] accessing index %d in list\n",index);
			file_block_index = ((DirectoryBlock*)d->current_block)->file_blocks[index];
			if(DEBUG) printf("[SimpleFS_readDir] not in FirstDirectoryBlock, next block_index %d\n",file_block_index);
		}
		
		if(DEBUG) printf("[SimpleFS_readDir] file at %d\n",file_block_index);
		err = DiskDriver_readBlock(disk, file_block, file_block_index);
		check_err(err,"[SimpleFS_readDir]Error reading next FirstFileBlock from on-memory image of disk");
		
		if(DEBUG) printf("[SimpleFS_readDir] - %s\n",file_fcb->name);
		
		//printf("[SimpleFS_readDir] sizeof(names[i]): %d, sizeof(file_block.fcb.name): %d, NAME_LEN: %d\n",sizeof(*names[i]),sizeof(file_block.fcb.name),NAME_LEN);
		strncpy(names[i], file_fcb->name, NAME_LEN);
		is_dir[i] = file_fcb->is_dir;
		
		i++;
		d->pos_in_block += sizeof(int);
		d->pos_in_dir += sizeof(int);
		
		if(DEBUG) printf("[SimpleFS_readDir] pos_in_block : %d\n",d->pos_in_block);
		if(d->pos_in_block >= BLOCK_SIZE) {
			d->pos_in_block = sizeof(BlockHeader);
			next_block_index = d->current_block->next_block;
			
			if(next_block_index != -1) {
				err = DiskDriver_readBlock(disk, d->current_block, next_block_index);
				check_err(err,"[SimpleFS_readDir]Error reading next DirectoryBlock from on-memory image of disk");
			} else if(i < num_files) {
				printf("[SimpleFS_readDir] Unexpected EOF(2) while reading directory %s\n",d->globalOpenDirectoryInfo->dir_start->fcb.name);
			return -1;
			} else {
				free(file_block);
				return i;
			}
		}
	}
	free(file_block);
	return i;
}

static OpenFileInfo* SimpleFs_findGlobalOpenFileInfo(SimpleFS* fs, const char* filename, int parent_directory_block) {
	if(!fs || !filename) {
		printf("[SimpleFs_findGlobalOpenFileInfo] Invalid arguments\n");
		exit(1);		
	}
	
	ListItem* curr_file_item = fs->OpenFiles.first;
	
	OpenFileInfo* curr_file;
	FirstFileBlock* file_blk;
	
	while(curr_file_item) {
		curr_file = (OpenFileInfo*)curr_file_item;
		file_blk = curr_file->file_start;
		
		if(strncmp(file_blk->fcb.name, filename, NAME_LEN)==0 && file_blk->fcb.directory_block==parent_directory_block)
			return curr_file;
		
		curr_file_item = curr_file_item->next;
	}
	
	return NULL;
}

static OpenDirectoryInfo* SimpleFs_findGlobalOpenDirectoryInfo(SimpleFS* fs, const char* dirname, int parent_directory_block) {
	if(!fs || !dirname) {
		printf("[SimpleFs_findGlobalOpenDirectoryInfo] Invalid arguments\n");
		exit(1);		
	}
	ListItem* curr_dir_item = fs->OpenDirectories.first;
	
	OpenDirectoryInfo* curr_dir;
	FirstDirectoryBlock* dir_blk;
	
	while(curr_dir_item) {
		curr_dir = (OpenDirectoryInfo*)curr_dir_item;
		dir_blk = curr_dir->dir_start;
		
		if(strncmp(dir_blk->fcb.name, dirname, NAME_LEN)==0 && dir_blk->fcb.directory_block==parent_directory_block)
			return curr_dir;
		
		curr_dir_item = curr_dir_item->next;
	}
	
	return NULL;
}

FileHandle* SimpleFS_openFile(DirectoryHandle* d, const char* filename) {
	if(!d || !filename) {
		printf("[SimpleFS_openFile] Invalid arguments\n");
		return NULL;		
	}
	
	if(DEBUG) printf("[SimpleFS_openFile] open: %s\n",filename);
	int err;
	
	SimpleFS* fs = d->sfs;
	DiskDriver* disk = fs->disk;
	
	int is_dir_flag = 0;
	int file_block_index = SimpleFS_findFile_diskBlockIndex(d, filename, &is_dir_flag);
	if(file_block_index < 0)	{ //file does not exist
		if(DEBUG) printf("[SimpleFS_openFile] file does not exists\n");
		return NULL;
	} else if( is_dir_flag ) { //file is dir
		if(DEBUG) printf("[SimpleFS_openFile] filename is a directory\n");
		return NULL;
	}
	
	if(DEBUG) printf("[SimpleFS_openFile] file at block %d\n", file_block_index);
	
	OpenFileInfo* open_file;
	
	open_file = SimpleFs_findGlobalOpenFileInfo(fs, filename, d->globalOpenDirectoryInfo->dir_start->fcb.block_in_disk);
	if(DEBUG && open_file) printf("[SimpleFS_openFile] file in Global OpenFiles at %p\n", open_file);
	
	if(!open_file) {
		if(DEBUG) printf("[SimpleFS_openFile] file not in Global OpenFiles list\n");
		FirstFileBlock* file_start = (FirstFileBlock*)calloc(1,sizeof(FirstFileBlock));
		err = DiskDriver_readBlock(disk, file_start, file_block_index);
		check_err(err,"[SimpleFS_openFile]Error reading FirstFileBlock from on-memory image of disk");
	
		open_file = (OpenFileInfo*)calloc(1,sizeof(OpenFileInfo));
		open_file->list.prev = 0;
		open_file->list.next = 0;
		open_file->file_start = file_start;
		open_file->handler_cnt = 0;
		List_insert(&(fs->OpenFiles), NULL, (ListItem*)open_file);
	}
	
	//initialize FileHandle
	FileHandle* file_handle = (FileHandle*)calloc(1,sizeof(FileHandle));
	file_handle->sfs = fs;
	file_handle->globalOpenFileInfo = open_file;
	open_file->handler_cnt++;
	file_handle->parent_directory = d->globalOpenDirectoryInfo;
	d->globalOpenDirectoryInfo->handler_cnt++;
	file_handle->current_block = (BlockHeader*)calloc(1,sizeof(DirectoryBlock));
	err = DiskDriver_readBlock(disk, file_handle->current_block, file_block_index);
	check_err(err,"[SimpleFS_openFile]Error reading FirstFileBlock from on-memory image of disk");
	file_handle->pos_in_file = 0;
	file_handle->pos_in_block = sizeof(BlockHeader)+sizeof(FileControlBlock) % BLOCK_SIZE;
	
	//printResume(fs);
	return file_handle;
}

int SimpleFS_write(FileHandle* f, void* data, int size) {
	if(!f || !data) {
		printf("[SimpleFS_write] Invalid arguments\n");
		return -1;		
	}
	
	if(DEBUG) printf("[SimpleFS_write]to file\n");
	if(DEBUG) printInfo_file(f->globalOpenFileInfo->file_start);
	if(!f)
		return -1;
	SimpleFS* fs = f->sfs;
	DiskDriver* disk = fs->disk;
	int err, written_bytes=0;
	//recusively first write bytes in current block
	//then allocate new one and goes on
	int bytes_to_write=0, bytes_left=size, need_blk=0;
	
	while(bytes_left > 0) {
		if(bytes_left < BLOCK_SIZE-f->pos_in_block) {
			bytes_to_write = bytes_left;
			need_blk = 0;
		} else {
			bytes_to_write = BLOCK_SIZE-f->pos_in_block;
			need_blk = 1;
		}
		bytes_left -= bytes_to_write;
	
		if(DEBUG) printf("[SimpleFS_write]bytes_to_write : %d, bytes_left : %d, need_blk : %d\n", bytes_to_write,bytes_left,need_blk);
		if(DEBUG) printf("[SimpleFS_write]pos_in_block : %d\n", f->pos_in_block);
		if(DEBUG) printf("[SimpleFS_write]current_block : %p, writing_ptr : %p\n", f->current_block,(char*)(f->current_block)+f->pos_in_block);
		memcpy((char*)(f->current_block)+f->pos_in_block, (char*)data+written_bytes, bytes_to_write);
		if(DEBUG) printf("[SimpleFS_write]first char: [to write] 0x%x, [written] 0x%x\n", *((char*)data+written_bytes), *((char*)(f->current_block)+f->pos_in_block));
		written_bytes+=bytes_to_write;
	
		f->pos_in_file += bytes_to_write;
		f->pos_in_block += bytes_to_write;
	

		int added_blk_flag = 0;
		
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
				added_blk_flag = 1;
			}
			int curr_blk_index_in_disk = getBlockInDisk(fs, f->current_block);
			int curr_blk_in_file = f->current_block->block_in_file;
			int next_blk_index_in_disk = f->current_block->next_block;
			//switcho al prossimo blocco
			err = DiskDriver_writeBlock(disk, f->current_block, curr_blk_index_in_disk);
			check_err(err,"[SimpleFS_write]Error writing CurrentBlock to on-memory image of disk");
			
			//if this was first block on disk, ricarico il fcb
			if(curr_blk_index_in_disk == f->globalOpenFileInfo->file_start->fcb.block_in_disk) {
				err = DiskDriver_readBlock(disk, f->globalOpenFileInfo->file_start, curr_blk_index_in_disk);
				check_err(err,"[SimpleFS_openFile]Error reading FirstFileBlock from on-memory image of disk");
			}
		
			//passo in f->current_block al prossimo
			memset(f->current_block, 0, BLOCK_SIZE);
				//inizilializzo il BlockHeader
			f->current_block->previous_block = curr_blk_index_in_disk;
			f->current_block->next_block = -1; //questo è ora l'ultimo blocco
			f->current_block->block_in_file = curr_blk_in_file+1;
			err = DiskDriver_writeBlock(disk, f->current_block, next_blk_index_in_disk);
			check_err(err,"[SimpleFS_write]Error writing FirstFileBlock to on-memory image of disk");
		
			f->pos_in_block = sizeof(BlockHeader);
		} else{
			int curr_blk_index_in_disk = getBlockInDisk(fs, f->current_block);
			err = DiskDriver_writeBlock(disk, f->current_block, curr_blk_index_in_disk);
			check_err(err,"[SimpleFS_write]Error writing CurrentBlock to on-memory image of disk");
			//if this was first block on disk, ricarico il fcb
			if(curr_blk_index_in_disk == f->globalOpenFileInfo->file_start->fcb.block_in_disk) {
				err = DiskDriver_readBlock(disk, f->globalOpenFileInfo->file_start, curr_blk_index_in_disk);
				check_err(err,"[SimpleFS_openFile]Error reading FirstFileBlock from on-memory image of disk");
			}
		}
		
		int size_bytes = f->globalOpenFileInfo->file_start->fcb.size_in_bytes;
		int new_size_bytes = (size_bytes > f->pos_in_file) ? size_bytes : f->pos_in_file;
		f->globalOpenFileInfo->file_start->fcb.size_in_bytes = new_size_bytes;
		
		if(added_blk_flag)
			f->globalOpenFileInfo->file_start->fcb.size_in_blocks++;
			
		err = DiskDriver_writeBlock(disk, f->globalOpenFileInfo->file_start, f->globalOpenFileInfo->file_start->fcb.block_in_disk);
		check_err(err,"[SimpleFS_write]Error writing FirstFileBlock to on-memory image of disk");
	}
	if(DEBUG) printf("[SimpleFS_write] end:\n");
	if(DEBUG) printInfo_file(f->globalOpenFileInfo->file_start);
	return written_bytes;
}

int SimpleFS_read(FileHandle* f, void* data, int size) {
	if(!f || !data) {
		printf("[SimpleFS_read] Invalid arguments\n");
		return -1;
	}
	
	SimpleFS* fs = f->sfs;
	DiskDriver* disk = fs->disk;
	
	int err;
	int read_bytes=0;
	int bytes_to_read=0, bytes_left=size, need_blk=0;
	
	int file_size = f->globalOpenFileInfo->file_start->fcb.size_in_bytes;
	//evito di leggere byte non presenti nel file
	if (f->pos_in_file + size > file_size)
		bytes_left = file_size - f->pos_in_file;
	
	//recusively first read bytes in current block
	//then allocate new one and goes on
	while(bytes_left > 0) {
		if(bytes_left < BLOCK_SIZE-f->pos_in_block) {
			bytes_to_read = bytes_left;
			need_blk = 0;
		} else {
			bytes_to_read = BLOCK_SIZE-f->pos_in_block;
			need_blk = 1;
		}
		bytes_left -= bytes_to_read;
	
		memcpy(data+read_bytes, (char*)f->current_block + f->pos_in_block, bytes_to_read);
		read_bytes+=bytes_to_read;
	
		f->pos_in_file += bytes_to_read;
		f->pos_in_block += bytes_to_read;
	
		if(need_blk) {
			int next_blk_index_in_disk = f->current_block->next_block;
			//switcho al prossimo blocco
		
			//passo in f->current_block al prossimo
			err = DiskDriver_readBlock(disk, f->current_block, next_blk_index_in_disk);
			check_err(err,"[SimpleFS_read]Error reading NextFileBlock from on-memory image of disk");
				//inizilializzo il BlockHeader
		
			f->pos_in_block = sizeof(BlockHeader);
		}
	}
	
	//SimpleFs_printGlobalOpenFiles(fs);
	return read_bytes;
}

int SimpleFS_seek(FileHandle* f, int pos) {
	if(!f) {
		printf("[SimpleFS_seek] Invalid arguments\n");
		return -1;
	}
	
	SimpleFS* fs = f->sfs;
	DiskDriver* disk = fs->disk;
	
	int err;
	int file_size = f->globalOpenFileInfo->file_start->fcb.size_in_bytes;
	if(pos >= file_size) //EOF
		return -1;
	
	//restart from first block in current_block
	err = DiskDriver_readBlock(disk, f->current_block, f->globalOpenFileInfo->file_start->fcb.block_in_disk);
	check_err(err,"[SimpleFS_openFile]Error reading FirstFileBlock from on-memory image of disk");
	f->pos_in_file = 0;
	f->pos_in_block = sizeof(BlockHeader)+sizeof(FileControlBlock) % BLOCK_SIZE;
	
	int skipped_bytes=0;
	//recusively first write bytes in current block
	//then allocate new one and goes on
	int bytes_to_skip=0, bytes_left=pos, need_blk=0;
	
	while(bytes_left > 0) {
		if(bytes_left < BLOCK_SIZE-f->pos_in_block) {
			bytes_to_skip = bytes_left;
			need_blk = 0;
		} else {
			bytes_to_skip = BLOCK_SIZE-f->pos_in_block;
			need_blk = 1;
		}
		bytes_left -= bytes_to_skip;
	
		skipped_bytes+=bytes_to_skip;
	
		f->pos_in_file += bytes_to_skip;
		f->pos_in_block += bytes_to_skip;
	
		if(need_blk) {
			int next_blk_index_in_disk = f->current_block->next_block;
			//switcho al prossimo blocco
		
			//passo in f->current_block al prossimo
			err = DiskDriver_readBlock(disk, f->current_block, next_blk_index_in_disk);
			check_err(err,"[SimpleFS_read]Error reading NextFileBlock from on-memory image of disk");
				//inizilializzo il BlockHeader
		
			f->pos_in_block = sizeof(BlockHeader);
		}
	}
	return skipped_bytes;
}

int SimpleFS_changeDir(DirectoryHandle* d, char* dirname) {
	if(!d || !dirname) {
		printf("[SimpleFS_changeDir] Invalid arguments\n");
		return -1;
	}
	
	int err;
	SimpleFS* fs = d->sfs;
	DiskDriver* disk = fs->disk;
	
	if( strncmp(dirname, "..", NAME_LEN) == 0 ) {
		//go to parent directory
		if(DEBUG) printf("[SimpleFS_changeDir] go to parent directory\n");
		
		//1. check there is a parent diectory (i.e. current directory is not root)
		if( !d->parent_directory ) {
			printf("[SimpleFS_changeDir] Error: current directory is root directory, there is no parent directory\n");
			return -1;
		}
		
		//2. free the globalOpenDirectoryInfo
		d->globalOpenDirectoryInfo->handler_cnt--;
		if(DEBUG) printf("[SimpleFS_changeDir] handler cnts on global OpenDirectories : %d\n", d->globalOpenDirectoryInfo->handler_cnt);
		if(d->globalOpenDirectoryInfo->handler_cnt < 1) {
			ListItem* item = List_detach(&(fs->OpenDirectories), (ListItem*)d->globalOpenDirectoryInfo);
			if(DEBUG) assert(item==(ListItem*)d->globalOpenDirectoryInfo, "AssertionError: detatched item does not match");
			free(d->globalOpenDirectoryInfo->dir_start);
			free(d->globalOpenDirectoryInfo);
		}
		
		//3. parent_directory --> globalOpenDirectoryInfo
		d->globalOpenDirectoryInfo = d->parent_directory;
			//note: I don't increase the number of handlre_cnts on parent_directory
				//because the total number of ptrs is always the same (just switching from parent to counter)
				//just temporarily parent_directory has a more ptr
		
		//4. parent of parent_directory --> parent_directory
			// if there is a granda directory, i.e. parent_directory was not root
		if( d->parent_directory->dir_start->fcb.directory_block > 0) {
			FirstDirectoryBlock grandpa_start;
			if(DEBUG) printf("[SimpleFS_changeDir] grandpa found at block : %d\n", d->parent_directory->dir_start->fcb.directory_block);
		
			err = DiskDriver_readBlock(disk, &grandpa_start, d->parent_directory->dir_start->fcb.directory_block);
			check_err(err,"[SimpleFS_changeDir] Error reading Grandpa FirstDirectoryBlock from on-memory image of disk");
		
			OpenDirectoryInfo* grandpa_directory = SimpleFs_findGlobalOpenDirectoryInfo(fs, grandpa_start.fcb.name, grandpa_start.fcb.directory_block);
		
			if( !grandpa_directory ) {
				FirstDirectoryBlock* grandpa_start_ptr = (FirstDirectoryBlock*)calloc(1,sizeof(FirstDirectoryBlock));
				err = DiskDriver_readBlock(disk, grandpa_start_ptr, grandpa_start.fcb.block_in_disk);
				check_err(err,"[SimpleFS_changeDir]Error reading Grandpa FirstDirectoryBlock from on-memory image of disk");
			
				grandpa_directory = (OpenDirectoryInfo*)calloc(1,sizeof(OpenDirectoryInfo));
				grandpa_directory->list.prev = 0;
				grandpa_directory->list.next = 0;
				grandpa_directory->dir_start = grandpa_start_ptr;
				grandpa_directory->handler_cnt = 0;
				List_insert(&(fs->OpenDirectories), fs->OpenDirectories.last, (ListItem*)grandpa_directory);
			}
		
			d->parent_directory = grandpa_directory;
			grandpa_directory->handler_cnt++;
		} else {
			d->parent_directory = NULL;
		}
		
		//5. reset current_block, pos_in_block, pos_in_dir
			//(after if-else)
	} else {
		//go to child directory
		if(DEBUG) printf("[SimpleFS_changeDir] go to child directory\n");
		//1. check child in directory
		int is_dir_flag = 0;
		int child_block_index = SimpleFS_findFile_diskBlockIndex(d, dirname, &is_dir_flag);
		if(child_block_index < 0)	{ //child_dir does not exist
			printf("[SimpleFS_changeDir] directory %s does not exists\n", dirname);
			return -1;
		} else if( !is_dir_flag ) { //file is not dir
			if(DEBUG) printf("[SimpleFS_changeDir] filename is not a directory\n");
			return -1;
		}
		
		if(DEBUG) printf("[SimpleFS_changeDir] child found at block : %d\n", child_block_index);
		
		//2. reduce parent_directory handler_cnt (if directory is not root, ==>> no parent)
		if( d->parent_directory ) {
			d->parent_directory->handler_cnt--;
			if(DEBUG) printf("[SimpleFS_changeDir] handler cnts on global parent OpenDirectories (after) : %d\n", d->parent_directory->handler_cnt);
			if(d->parent_directory->handler_cnt < 1) {
				ListItem* item = List_detach(&(fs->OpenDirectories), (ListItem*)d->parent_directory);
				if(DEBUG) assert(item==(ListItem*)d->parent_directory, "AssertionError: detatched item does not match");
				free(d->parent_directory->dir_start);
				free(d->parent_directory);
			}
		}
		
		//3. globalOpenDirectoryInfo --> parent_directory
		d->parent_directory = d->globalOpenDirectoryInfo;
		
		//4. child --> globalOpenDirectoryInfo		
		OpenDirectoryInfo* child_directory = SimpleFs_findGlobalOpenDirectoryInfo(fs, dirname, d->parent_directory->dir_start->fcb.block_in_disk);
		
		if( !child_directory ) {
			FirstDirectoryBlock* child_start_ptr = (FirstDirectoryBlock*)calloc(1,sizeof(FirstDirectoryBlock));
			err = DiskDriver_readBlock(disk, child_start_ptr, child_block_index);
			check_err(err,"[SimpleFS_changeDir]Error reading Child FirstDirectoryBlock from on-memory image of disk");
			
			child_directory = (OpenDirectoryInfo*)calloc(1,sizeof(OpenDirectoryInfo));
			child_directory->list.prev = 0;
			child_directory->list.next = 0;
			child_directory->dir_start = child_start_ptr;
			child_directory->handler_cnt = 0;
			List_insert(&(fs->OpenDirectories), fs->OpenDirectories.last, (ListItem*)child_directory);
		}
		
		d->globalOpenDirectoryInfo = child_directory;
		child_directory->handler_cnt++;
		//5. reset current_block, pos_in_block, pos_in_dir
			//(after if-else)
	}	
	memcpy(d->current_block, d->globalOpenDirectoryInfo->dir_start, BLOCK_SIZE);
	d->pos_in_dir = sizeof(int);
	d->pos_in_block = sizeof(BlockHeader)+sizeof(FileControlBlock)+sizeof(int);
	
	//printInfo_dir(d->globalOpenDirectoryInfo->dir_start);
	return 0;
}

int SimpleFS_mkDir(DirectoryHandle* d, char* dirname) {
	if(!d || !dirname) {
		printf("[SimpleFS_mkDir] Invalid arguments\n");
		return -1;
	}
	
	if(DEBUG) printf("[SimpleFS_mkDir] create: %s\n",dirname);
	int err;
	
	SimpleFS* fs = d->sfs;
	DiskDriver* disk = fs->disk;
	
	int file_block_index = SimpleFS_findFile_diskBlockIndex(d, dirname, NULL);
	if(file_block_index > 0)	{ //file already exists
		printf("[SimpleFS_mkDir] a file with name %s already exists\n",dirname);
		return -1;
	}
	
	//get a free block on disk for the file
	int new_dir_index = DiskDriver_getFreeBlock(disk, 0);
	check_err(new_dir_index,"[SimpleFS_mkDir]Appears there is no free block in disk.");
	if(DEBUG) printf("\t[SimpleFS_mkDir] free block for dir: %d\n",new_dir_index);
	//aggiungo nell'elenco dei file nella directory
		//l'indice del blocco appena aggiunto

	//creo il file nel blocco libero individuato
	FirstDirectoryBlock dir_start = {0};
		//inizilializzo il BlockHeader
	dir_start.header.previous_block = -1; //this is the first block of directory
	dir_start.header.next_block = -1;//this is the last block of directory
	dir_start.header.block_in_file = 0;
	//inizilializzo il FileControlBlock
	dir_start.fcb.directory_block = d->globalOpenDirectoryInfo->dir_start->fcb.block_in_disk;
	dir_start.fcb.block_in_disk = new_dir_index;
	strncpy(dir_start.fcb.name, dirname, NAME_LEN);
	dir_start.fcb.size_in_bytes = sizeof(int);
	dir_start.fcb.size_in_blocks = (sizeof(BlockHeader)+sizeof(FileControlBlock)+sizeof(int) + BLOCK_SIZE-1)/BLOCK_SIZE;
	dir_start.fcb.is_dir = 1;
	dir_start.num_entries = 0;
	
	if(DEBUG) printf("\t[SimpleFS_mkDir] write dir on block: %d\n",new_dir_index);
	err = DiskDriver_writeBlock(disk, &dir_start, new_dir_index);
	check_err(err,"[SimpleFS_mkDir]Error writing FirstDirectoryBlock to on-memory image of disk");

	SimpleFS_addFileEntry_inDir(fs, d, new_dir_index);

	//creating directory does not open it
/*	OpenDirectoryInfo* open_dir = (OpenDirectoryInfo*)calloc(1,sizeof(OpenDirectoryInfo));*/
/*	open_dir->list.prev = 0;*/
/*	open_dir->list.next = 0;*/
/*	open_dir->file_start = dir_start;*/
/*	open_dir->handler_cnt = 0;*/
/*	List_insert(&(fs->OpenDirectories), fs->OpenDirectories.last, (ListItem*)open_dir);*/
		
	//printResume(fs);
	return 0;
}

int SimpleFS_remove(DirectoryHandle* d, char* filename) {
	if(!d || !filename) {
		printf("[SimpleFS_remove] Invalid arguments\n");
		return -1;
	}
	
	//scorri i file/subdir nella direcotry
		//quando trovi il file cercato,
		//inizia a sovrascrivere ogni entrata con la successiva
	//decrementa num_entries
	if(DEBUG) printf("[SimpleFS_remove] remove : %s\n",filename);
	int err, res=0;
	
	SimpleFS* fs = d->sfs;
	DiskDriver* disk = fs->disk;
	
	int num_files = d->globalOpenDirectoryInfo->dir_start->num_entries;
	int directory_size = d->globalOpenDirectoryInfo->dir_start->fcb.size_in_bytes;

	//scorrerò la directory usando a local block as current_block
	BlockHeader* curr_block = (BlockHeader*)calloc(1,BLOCK_SIZE);
	memcpy(curr_block, d->globalOpenDirectoryInfo->dir_start, BLOCK_SIZE);
	
	int curr_pos_in_block = sizeof(BlockHeader)+sizeof(FileControlBlock)+sizeof(int);
	int curr_pos_in_dir = sizeof(int);
	
	//I'll use prev when to overwrite, to eliminate a file
		//necessary because of the possibility of new index on new block and old on previous block
	BlockHeader* prev_block = (BlockHeader*)calloc(1,BLOCK_SIZE);
	memcpy(prev_block, curr_block, BLOCK_SIZE);
	int prev_index = 0;
	
	int i=0, file_block_index, next_block_index;
	BlockHeader* file_block = (BlockHeader*)calloc(1,BLOCK_SIZE);
	
	int flag_found_to_rem=0, flag_switch_blocks=0;
	
	char current_name[NAME_LEN];
	while(i < num_files) {
		if(curr_pos_in_dir >= directory_size) {
			printf("[SimpleFS_remove] Unexpected EOF(1) while reading directory %s\n",d->globalOpenDirectoryInfo->dir_start->fcb.name);
				res = -1;
				goto end_remove;
		}
		int curr_index;
		//I am accessing *(int*)((void*)(d->current_block)+(d->pos_in_block))
		if( curr_block->previous_block == -1 ) {//if FirstDirectoryBlock
			curr_index = i;
			file_block_index = ((FirstDirectoryBlock*)curr_block)->file_blocks[curr_index];
		}
		else {
			int block_in_file = curr_block->block_in_file;
			curr_index =  i - (BLOCK_SIZE-sizeof(BlockHeader)-sizeof(FileControlBlock)-sizeof(int))/sizeof(int) - (block_in_file - 1)*(BLOCK_SIZE-sizeof(BlockHeader))/sizeof(int);
			file_block_index = ((DirectoryBlock*)curr_block)->file_blocks[curr_index];
		}
		
		if( !flag_found_to_rem ) {
			if(DEBUG) printf("[SimpleFS_remove] %d) next FirstFileBlock at %d\n", i, file_block_index);
			err = DiskDriver_readBlock(disk, file_block, file_block_index);
			check_err(err,"[SimpleFS_remove]Error reading next FirstFileBlock from on-memory image of disk");
		
			strncpy(current_name, ((FirstFileBlock*)file_block)->fcb.name, NAME_LEN);
			if(DEBUG) printf("[SimpleFS_remove] next filename : %s\n",current_name);
			if(strncmp(current_name, filename, NAME_LEN) == 0) {
				if(((FirstDirectoryBlock*)file_block)->fcb.is_dir && ((FirstDirectoryBlock*)file_block)->num_entries > 0) { //if dir and not empty
					printf("[SimpleFS_remove] Operation Impossible: filename is a non-empty directory\n");
				res = -1;
				goto end_remove;
				}
				if(DEBUG) printf("[SimpleFS_remove] found to remove at %d\n", file_block_index);

				flag_found_to_rem = 1;
				
				//free all blocks of file in the bitmap
				int file_block_to_free = file_block_index;
				while(file_block_to_free > 0) {
					err = DiskDriver_freeBlock(disk, file_block_index);
					check_err(err,"[SimpleFS_remove]Error freeing last block of directory");
				
					file_block_to_free = file_block->next_block;
					
					if(file_block_to_free > 0) {
						err = DiskDriver_readBlock(disk, file_block, file_block_to_free);
						check_err(err,"[SimpleFS_remove]Error reading next FirstFileBlock from on-memory image of disk");
					}
				}
				
				if( curr_block->previous_block == -1 ) {//if FirstDirectoryBlock
					memcpy(prev_block, d->globalOpenDirectoryInfo->dir_start, BLOCK_SIZE);
				} else {
					err = DiskDriver_writeBlock(disk, d->globalOpenDirectoryInfo->dir_start, d->globalOpenDirectoryInfo->dir_start->fcb.block_in_disk);
					check_err(err,"[SimpleFS_write]Error writing FirstDirectoryBlock to on-memory image of disk");
				}
				prev_index = curr_index;
			}
		} else { //if found file to remove
			if(DEBUG) printf("[SimpleFS_remove] %d) writing at %d : new file index %d\n", i, prev_index, file_block_index);
			if( prev_block->previous_block == -1 ) //if FirstDirectoryBlock
				 ((FirstDirectoryBlock*)prev_block)->file_blocks[prev_index] = file_block_index;
			else
				((DirectoryBlock*)prev_block)->file_blocks[prev_index] = file_block_index;
			
			prev_index = curr_index;
		}
	
		i++;
		curr_pos_in_block += sizeof(int);
		curr_pos_in_dir += sizeof(int);


		if( flag_switch_blocks ) {
			if(DEBUG) printf("[SimpleFS_remove]  switch blocks\n");
			err = DiskDriver_writeBlock(disk, prev_block, getBlockInDisk(fs, prev_block));
			check_err(err,"[SimpleFS_write]Error writing previous DirectoryBlock to on-memory image of disk");
			
			if(prev_block->block_in_file == 0) {
				if(DEBUG) printf("[SimpleFS_remove] reload first block\n");
				//if this was first_directory_block reload first block
				err = DiskDriver_readBlock(disk, d->globalOpenDirectoryInfo->dir_start, d->globalOpenDirectoryInfo->dir_start->fcb.block_in_disk);
				check_err(err,"[SimpleFS_remove]Error reading FirstDirectoryBlock from on-memory image of disk");
			}
			//start updating next block
			memcpy(prev_block, curr_block, BLOCK_SIZE);
			flag_switch_blocks = 0;
		}
		if(curr_pos_in_block >= BLOCK_SIZE) {
			curr_pos_in_block = sizeof(BlockHeader);
			next_block_index = curr_block->next_block;
			if(DEBUG) printf("[SimpleFS_remove] switch to next block : %d index of next block\n", next_block_index);
			
			if(next_block_index != -1) {
				err = DiskDriver_readBlock(disk, curr_block, next_block_index);
				check_err(err,"[SimpleFS_remove]Error reading next DirectoryBlock from on-memory image of disk");
				flag_switch_blocks = 1;
			} else if(i < num_files) {
				printf("[SimpleFS_remove] Unexpected EOF(2) while reading directory %s\n",d->globalOpenDirectoryInfo->dir_start->fcb.name);
				res = -1;
				goto end_remove;
			}
		}
	}
	
	err = DiskDriver_writeBlock(disk, prev_block, getBlockInDisk(fs, prev_block));
	check_err(err,"[SimpleFS_write]Error writing previous DirectoryBlock to on-memory image of disk");
	if(prev_block->block_in_file == 0) {
		if(DEBUG) printf("[SimpleFS_remove] reload first block\n");
		//if this was first_directory_block reload first block
		err = DiskDriver_readBlock(disk, d->globalOpenDirectoryInfo->dir_start, d->globalOpenDirectoryInfo->dir_start->fcb.block_in_disk);
		check_err(err,"[SimpleFS_remove]Error reading FirstDirectoryBlock from on-memory image of disk");
	}
	
	if( flag_found_to_rem ) {
		int flag_reload_curr_block=0, new_last_block=-1;
		//if I found file to remove, reduce number of files in dir
		d->pos_in_block -= sizeof(int);
		d->pos_in_dir -= sizeof(int);
		if(d->pos_in_block <= 0) {
			d->pos_in_block += BLOCK_SIZE;
			while(d->current_block->next_block > 0) {
				printf("[SimpleFS_remove] Warning: current_block was supposed to be still storing last block!!\n");
				err = DiskDriver_readBlock(disk, d->current_block, d->current_block->next_block);
				check_err(err,"[SimpleFS_remove]Error reading next DirectoryBlock from on-memory image of disk");
			}
			int block_to_free = getBlockInDisk(fs, d->current_block);
			if(DEBUG) printf("[SimpleFS_remove] Index block to free: %d\n", block_to_free);
			
			new_last_block = d->current_block->previous_block;
			
			err = DiskDriver_readBlock(disk, d->current_block, new_last_block);
			check_err(err,"[SimpleFS_remove]Error reading prev DirectoryBlock from on-memory image of disk");
			if(d->current_block->block_in_file == 0) {
				d->globalOpenDirectoryInfo->dir_start->header.next_block = -1;
				flag_reload_curr_block = 1;
			} else {
				d->current_block->next_block = -1;
				err = DiskDriver_writeBlock(disk, d->current_block, getBlockInDisk(fs, d->current_block));
				check_err(err,"[SimpleFS_remove]Error writing DirectoryBlock to on-memory image of disk");
			}
		
			err = DiskDriver_freeBlock(disk, block_to_free);
			check_err(err,"[SimpleFS_remove]Error freeing last block of directory");
			
			d->globalOpenDirectoryInfo->dir_start->fcb.size_in_blocks--;
			
		}
		d->globalOpenDirectoryInfo->dir_start->num_entries--;
		d->globalOpenDirectoryInfo->dir_start->fcb.size_in_bytes -= sizeof(int);
		
		err = DiskDriver_writeBlock(disk, d->globalOpenDirectoryInfo->dir_start, d->globalOpenDirectoryInfo->dir_start->fcb.block_in_disk);
		check_err(err,"[SimpleFS_remove]Error writing FirstDirectoryBlock to on-memory image of disk");
		
		if( flag_reload_curr_block ) {
			err = DiskDriver_readBlock(disk, d->current_block, new_last_block);
			check_err(err,"[SimpleFS_remove]Error reading new Last DirectoryBlock from on-memory image of disk");
		}
	} else {	
		printf("[SimpleFS_remove] File not found in dir\n");
		res = -1;
	} 
	
	end_remove:
		free(curr_block);
		free(prev_block);
		free(file_block);
		
		if(DEBUG) printInfo_dir(d->globalOpenDirectoryInfo->dir_start);
		if(DEBUG) printf("pos_in_dir %d , pos_in_block %d\n",d->pos_in_dir,d->pos_in_block);
		return res;
}

void SimpleFS_closeFile(FileHandle* f) {
	//SimpleFs_printGlobalOpenFiles(f->sfs);
	if(!f){
		printf("[SimpleFS_closeFile] Invalid arguments\n\n");
		return;
	}
	
	if(DEBUG) printf("[SimpleFS_closeFile] closing file handle : block %d\n", f->globalOpenFileInfo->file_start->fcb.block_in_disk);
	SimpleFS* fs = f->sfs;
	
	if(DEBUG) printf("[SimpleFS_closeFile] f->globalOpenFileInfo at %p\n", f->globalOpenFileInfo);
	f->globalOpenFileInfo->handler_cnt--;
	if(DEBUG) printf("[SimpleFS_closeFile] handler cnts on global OpenFile : %d\n", f->globalOpenFileInfo->handler_cnt);
	if(f->globalOpenFileInfo->handler_cnt < 1) {
		if(DEBUG) printf("removing globalOpenFileInfo\n");
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
		printf("[SimpleFS_closeDirectory] Invalid arguments\n\n");
		return;
	}
	
	if(DEBUG) printf("[SimpleFS_closeDirectory] closing directory handle : block %d\n", d->globalOpenDirectoryInfo->dir_start->fcb.block_in_disk);
	SimpleFS* fs = d->sfs;
	
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
		printf("[SimpleFS_close] Invalid arguments\n\n");
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
