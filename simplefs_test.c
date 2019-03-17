#include "simplefs.h"
#include <stdio.h>
#include <stdlib.h>

#define NUM_FILES 256
#define DIRECTORIES_IN_ROOT 10
#define NUM_USERS 3

#define FILE_SIZE 2*BLOCK_SIZE

#define MAX_ENTRIES_IN_DIR NUM_FILES+DIRECTORIES_IN_ROOT

char* file_names[256] = {
"file000.txt","file001.txt","file002.txt","file003.txt","file004.txt","file005.txt","file006.txt","file007.txt","file008.txt","file009.txt","file010.txt","file011.txt","file012.txt","file013.txt","file014.txt","file015.txt","file016.txt","file017.txt","file018.txt","file019.txt","file020.txt","file021.txt","file022.txt","file023.txt","file024.txt","file025.txt","file026.txt","file027.txt","file028.txt","file029.txt","file030.txt","file031.txt","file032.txt","file033.txt","file034.txt","file035.txt","file036.txt","file037.txt","file038.txt","file039.txt","file040.txt","file041.txt","file042.txt","file043.txt","file044.txt","file045.txt","file046.txt","file047.txt","file048.txt","file049.txt","file050.txt","file051.txt","file052.txt","file053.txt","file054.txt","file055.txt","file056.txt","file057.txt","file058.txt","file059.txt","file060.txt","file061.txt","file062.txt","file063.txt","file064.txt","file065.txt","file066.txt","file067.txt","file068.txt","file069.txt","file070.txt","file071.txt","file072.txt","file073.txt","file074.txt","file075.txt","file076.txt","file077.txt","file078.txt","file079.txt","file080.txt","file081.txt","file082.txt","file083.txt","file084.txt","file085.txt","file086.txt","file087.txt","file088.txt","file089.txt","file090.txt","file091.txt","file092.txt","file093.txt","file094.txt","file095.txt","file096.txt","file097.txt","file098.txt","file099.txt","file100.txt","file101.txt","file102.txt","file103.txt","file104.txt","file105.txt","file106.txt","file107.txt","file108.txt","file109.txt","file110.txt","file111.txt","file112.txt","file113.txt","file114.txt","file115.txt","file116.txt","file117.txt","file118.txt","file119.txt","file120.txt","file121.txt","file122.txt","file123.txt","file124.txt","file125.txt","file126.txt","file127.txt","file128.txt","file129.txt","file130.txt","file131.txt","file132.txt","file133.txt","file134.txt","file135.txt","file136.txt","file137.txt","file138.txt","file139.txt","file140.txt","file141.txt","file142.txt","file143.txt","file144.txt","file145.txt","file146.txt","file147.txt","file148.txt","file149.txt","file150.txt","file151.txt","file152.txt","file153.txt","file154.txt","file155.txt","file156.txt","file157.txt","file158.txt","file159.txt","file160.txt","file161.txt","file162.txt","file163.txt","file164.txt","file165.txt","file166.txt","file167.txt","file168.txt","file169.txt","file170.txt","file171.txt","file172.txt","file173.txt","file174.txt","file175.txt","file176.txt","file177.txt","file178.txt","file179.txt","file180.txt","file181.txt","file182.txt","file183.txt","file184.txt","file185.txt","file186.txt","file187.txt","file188.txt","file189.txt","file190.txt","file191.txt","file192.txt","file193.txt","file194.txt","file195.txt","file196.txt","file197.txt","file198.txt","file199.txt","file200.txt","file201.txt","file202.txt","file203.txt","file204.txt","file205.txt","file206.txt","file207.txt","file208.txt","file209.txt","file210.txt","file211.txt","file212.txt","file213.txt","file214.txt","file215.txt","file216.txt","file217.txt","file218.txt","file219.txt","file220.txt","file221.txt","file222.txt","file223.txt","file224.txt","file225.txt","file226.txt","file227.txt","file228.txt","file229.txt","file230.txt","file231.txt","file232.txt","file233.txt","file234.txt","file235.txt","file236.txt","file237.txt","file238.txt","file239.txt","file240.txt","file241.txt","file242.txt","file243.txt","file244.txt","file245.txt","file246.txt","file247.txt","file248.txt","file249.txt","file250.txt","file251.txt","file252.txt","file253.txt","file254.txt","file255.txt"
};

char* directory_names[DIRECTORIES_IN_ROOT] = {
"dev","usr","home","bin","lib","media","mnt","opt","proc","var"
};

char* home_folders[NUM_USERS] = {
"filippo","lorenzo","paolo"
};

int main(int argc, char** argv) {
  printf("FirstBlock size %ld\n", sizeof(FirstFileBlock));
  printf("DataBlock size %ld\n", sizeof(FileBlock));
  printf("FirstDirectoryBlock size %ld\n", sizeof(FirstDirectoryBlock));
  printf("DirectoryBlock size %ld\n", sizeof(DirectoryBlock));
  
  
  DiskDriver dd;
	DiskDriver_init(&dd, "fs", 1024);

	SimpleFS fs;
	DirectoryHandle* dir_handle = SimpleFS_init(&fs, &dd);
		
/*	SimpleFS_format(&fs);*/
/*	printf("\tfirst_free_block: %d\n",dd.header->first_free_block);*/


	int i, res;


	char* names[MAX_ENTRIES_IN_DIR];
	for(i=0;i<MAX_ENTRIES_IN_DIR;i++)
		names[i] = (char*)calloc(1,sizeof(char)*(NAME_LEN));
	
	int is_dir[MAX_ENTRIES_IN_DIR] = {0};
	
	FileHandle* files[NUM_FILES];
	FileHandle* fh_aux;
	
	
	char junk[FILE_SIZE+1];
	for(i=0;i<FILE_SIZE;i++)
		junk[i] = 97+i/64; //64 a, 64 b, 64 c, 64 d, 64 e, 64 f, 64 g, 64 h
	junk[FILE_SIZE] = 0;
	//printf("%s\n",junk);
	
	char junk2[FILE_SIZE+1];
	junk2[FILE_SIZE] = 0;
	
	printf("\n\n##############################\ncreate %d Files in dir_handle\n##############################\n\n", NUM_FILES);
	for(i=0; i<NUM_FILES; i++){
		//printf("create %s\n", file_names[i]);
		//printf("\tfirst_free_block: %d\n",dd.header->first_free_block);
		files[i] = SimpleFS_createFile(dir_handle, file_names[i]);
		if(!files[i])
			printf("create %s failed: file already existed\n", file_names[i]);

		fh_aux = SimpleFS_createFile(dir_handle, file_names[i]);
		if(fh_aux)
			printf("create %s didn't failed: but file already existed\n", file_names[i]);
	}

	printf("\n\n##############################\nread Dir : /\n##############################\n\n");
	res = SimpleFS_readDir(names, is_dir, dir_handle);
	printf("%s contains %d files:\n",dir_handle->globalOpenDirectoryInfo->dir_start->fcb.name,res);
	for(i=0;i<res;i++) {
		if(is_dir[i])
			printf("\t d  %s\n",names[i]);
		else
			printf("\t -  %s\n",names[i]);
	}
	
	printf("\n\n##############################\nopen/r/w File : file13.txt\n##############################\n\n");
	//first close file handle for file13.txt, and reopen it
	SimpleFS_closeFile(files[13]);
	printf("[Note on a 2nd run this close will crash because file13.txt already existed and create failed]\n");
	files[13] = SimpleFS_openFile(dir_handle, file_names[13]);
	
	res = SimpleFS_write(files[13], junk, sizeof(junk));
	printf("SimpleFS_write on file13.txt : %d bytes [%lu]\n", res, sizeof(junk));
	
	res = SimpleFS_seek(files[13], 0);
	printf("SimpleFS_seek on file13.txt : %d [0]\n", res);
	
	res = SimpleFS_read(files[13], junk2, sizeof(junk2));
	printf("SimpleFS_read on file13.txt : %d bytes [%lu]\n", res, sizeof(junk2));
	printf("SimpleFS_read on file13.txt :  %s\n",junk2);

	printf("\n\n##############################\nclosing file handles\n##############################\n\n");
	for(i=0; i<NUM_FILES; i++){
		if(files[i]) {
			SimpleFS_closeFile(files[i]);
		}
		files[i] = NULL; //annullo il ptr per non avere problemi dopo
	}

	printf("\n\n##############################\nremove %d Files from dir_handle\n##############################\n\n",NUM_FILES/2);
	for(i=0; i<NUM_FILES; i+=2){
		//printf("removing %s\n", file_names[i]);
		res = SimpleFS_remove(dir_handle, file_names[i]);
		if(res == -1)
			printf("rm %s failed: %d [0]\n", file_names[i], res);
		//printf("\tfirst_free_block: %d\n",dd.header->first_free_block);
	}

	printf("\n\n##############################\nread Dir : /\n##############################\n\n");
	res = SimpleFS_readDir(names, is_dir, dir_handle);
	printf("%s contains %d files:\n",dir_handle->globalOpenDirectoryInfo->dir_start->fcb.name,res);
	for(i=0;i<res;i++) {
		if(is_dir[i])
			printf("\t d  %s\n",names[i]);
		else
			printf("\t -  %s\n",names[i]);
	}

	printf("\n\n##############################\ncreate %d Dir in dir_handle\n##############################\n\n", DIRECTORIES_IN_ROOT);
	for(i=0; i<DIRECTORIES_IN_ROOT; i++){
		//printf("mkDir %s\n", directory_names[i]);
		//printf("\tfirst_free_block: %d\n", dd.header->first_free_block);
		res = SimpleFS_mkDir(dir_handle, directory_names[i]);
		if(res != 0)
			printf("mkDir %s failed\n", directory_names[i]);
		res = SimpleFS_mkDir(dir_handle, directory_names[i]);
		if(res != -1)
			printf("mkDir %s didn't failed: but directory already existed\n", directory_names[i]);
	}

	printf("\n\n##############################\nread Dir %s: /\n##############################\n\n",dir_handle->globalOpenDirectoryInfo->dir_start->fcb.name);
	res = SimpleFS_readDir(names, is_dir, dir_handle);
	printf("%s contains %d files:\n",dir_handle->globalOpenDirectoryInfo->dir_start->fcb.name,res);
	for(i=0;i<res;i++) {
		if(is_dir[i])
			printf("\t d  %s\n",names[i]);
		else
			printf("\t -  %s\n",names[i]);
	}

	printf("\n\n##############################\ncd %s\n##############################\n\n", directory_names[2]);
	res = SimpleFS_changeDir(dir_handle, directory_names[2]);
	printf("changeDir %s %d [0]\n",directory_names[2], res);

	res = SimpleFS_changeDir(dir_handle, directory_names[2]);
	printf("changeDir %s %d [-1]\n",directory_names[2], res);


	printf("\n\n##############################\ncreate %d files in current directory: %s\n##############################\n\n", NUM_FILES/4, dir_handle->globalOpenDirectoryInfo->dir_start->fcb.name);
	for(i=0; i<NUM_FILES; i+=4){
		//printf("create %s\n", file_names[i]);
		//printf("\tfirst_free_block: %d\n",dd.header->first_free_block);
		files[i] = SimpleFS_createFile(dir_handle, file_names[i]);
		if(!files[i])
			printf("create %s failed: file already existed\n", file_names[i]);

		fh_aux = SimpleFS_createFile(dir_handle, file_names[i]);
		if(fh_aux)
			printf("create %s didn't failed: but file already existed\n", file_names[i]);
	}

	printf("\n\n##############################\nread Dir %s: /\n##############################\n\n",dir_handle->globalOpenDirectoryInfo->dir_start->fcb.name);
	res = SimpleFS_readDir(names, is_dir, dir_handle);
	printf("%s contains %d files:\n",dir_handle->globalOpenDirectoryInfo->dir_start->fcb.name,res);
	for(i=0;i<res;i++) {
		if(is_dir[i])
			printf("\t d  %s\n",names[i]);
		else
			printf("\t -  %s\n",names[i]);
	}

	printf("\n\n##############################\ncreate %d directories in current directory: %s\n##############################\n\n", NUM_USERS, dir_handle->globalOpenDirectoryInfo->dir_start->fcb.name);
	for(i=0; i<NUM_USERS; i++){
		//printf("mkDir %s\n", home_folders[i]);
		//printf("\tfirst_free_block: %d\n", dd.header->first_free_block);
		res = SimpleFS_mkDir(dir_handle, home_folders[i]);
		if(res != 0)
			printf("mkDir %s failed\n", home_folders[i]);
		res = SimpleFS_mkDir(dir_handle, home_folders[i]);
		if(res != -1)
			printf("mkDir %s didn't failed: but directory already existed\n", home_folders[i]);
	}

	printf("\n\n##############################\nread Dir %s: /\n##############################\n\n",dir_handle->globalOpenDirectoryInfo->dir_start->fcb.name);
	res = SimpleFS_readDir(names, is_dir, dir_handle);
	printf("%s contains %d files:\n",dir_handle->globalOpenDirectoryInfo->dir_start->fcb.name,res);
	for(i=0;i<res;i++) {
		if(is_dir[i])
			printf("\t d  %s\n",names[i]);
		else
			printf("\t -  %s\n",names[i]);
	}

	printf("\n\n##############################\ncd ..\n##############################\n\n");
	res = SimpleFS_changeDir(dir_handle, "..");
	printf("changeDir ../ %d [0]\n", res);

	res = SimpleFS_changeDir(dir_handle,  "..");
	printf("changeDir ../ %d [-1]\n", res);

	printf("\n\n##############################\nremove 2 Dirs from current_directory : %s\n##############################\n\n",dir_handle->globalOpenDirectoryInfo->dir_start->fcb.name);
	res = SimpleFS_remove(dir_handle, directory_names[2]);
	printf("rm %s failed: %d [-1]\n", directory_names[2], res);
	
	
	res = SimpleFS_remove(dir_handle, directory_names[0]);
	printf("rm %s success: %d [0]\n", directory_names[0], res);

	printf("\n\n##############################\nread Dir %s: /\n##############################\n\n",dir_handle->globalOpenDirectoryInfo->dir_start->fcb.name);
	res = SimpleFS_readDir(names, is_dir, dir_handle);
	printf("%s contains %d files:\n",dir_handle->globalOpenDirectoryInfo->dir_start->fcb.name,res);
	for(i=0;i<res;i++) {
		if(is_dir[i])
			printf("\t d  %s\n",names[i]);
		else
			printf("\t -  %s\n",names[i]);
	}

	printf("\n\n##############################\nclosing file handles\n##############################\n\n");
	for(i=0; i<NUM_FILES; i++){
		if(files[i]) {
			SimpleFS_closeFile(files[i]);
		}
	}

	for(i=0;i<MAX_ENTRIES_IN_DIR;i++)
		free(names[i]);

	SimpleFS_closeDirectory(dir_handle);
	SimpleFS_close(&fs);
	DiskDriver_flush(&dd);
}
