#include "simplefs.h"
#include <stdio.h>
#include <stdlib.h>

int main(int agc, char** argv) {
  printf("FirstBlock size %ld\n", sizeof(FirstFileBlock));
  printf("DataBlock size %ld\n", sizeof(FileBlock));
  printf("FirstDirectoryBlock size %ld\n", sizeof(FirstDirectoryBlock));
  printf("DirectoryBlock size %ld\n", sizeof(DirectoryBlock));
  
  DiskDriver dd;
	DiskDriver_init(&dd, "fs", 200);
	printf("disk at: %p\n",dd.header);
	printf("\tfirst_free_block: %d\n",dd.header->first_free_block);

	SimpleFS fs;
	DirectoryHandle* root_dir = SimpleFS_init(&fs, &dd);
	//printf("root_dir at: %d\n",root_dir->first_directory_block);
	printf("\tfirst_free_block: %d\n",dd.header->first_free_block);
	//free(root_dir);
	
/*	SimpleFS_format(&fs);*/
/*	printf("\tfirst_free_block: %d\n",dd.header->first_free_block);*/

	printf("\n\n##############################\ncreate File : file1.txt\n##############################\n\n");
	FileHandle* f1 = SimpleFS_createFile(root_dir, "file1.txt");
	
	
	
	int i;
	char* names[10];
	for(i=0;i<10;i++)
		names[i] = (char*)malloc(sizeof(char)*NAME_LEN);
	
	printf("\n\n##############################\nread Dir : /\n##############################\n\n");
	int n = SimpleFS_readDir(names, root_dir);
	printf("n files: %d\n",n);
	for(i=0;i<n;i++) {
		printf(" - %s\n",names[i]);
	}
	
	printf("\n\n##############################\ncreate File : file2.jpg\n##############################\n\n");
	FileHandle* f2 = SimpleFS_createFile(root_dir, "file2.jpg");
	SimpleFS_closeFile(f2);
	
	printf("\n\n##############################\nread Dir : /\n##############################\n\n");
	n = SimpleFS_readDir(names, root_dir);
	printf("n files: %d\n",n);
	for(i=0;i<n;i++) {
		printf(" - %s\n",names[i]);
	}
	
	
	printf("\n\n##############################\nopen/r/w File : file1.txt\n##############################\n\n");
	SimpleFS_closeFile(f1);
	FileHandle* fh;
	fh = SimpleFS_openFile(root_dir, "file1.txt");
	SimpleFS_closeFile(fh);
	fh = SimpleFS_openFile(root_dir, "file1.txt");
	SimpleFS_closeFile(fh);
	f1 = SimpleFS_openFile(root_dir, "tmp2");
	
	
	char junk[21];
	for(i=0;i<20;i++)
		junk[i] = 100;
	junk[20]=0;
	
	printf("[test] %s\n",junk);
	
	fh = SimpleFS_openFile(root_dir, "file1.txt");
	int res = SimpleFS_write(fh, junk, 20);
	printf("written %d [20]\n",res);
	SimpleFS_closeFile(fh);


	char junk2[21];
	junk2[20]=0;

	fh = SimpleFS_openFile(root_dir, "file1.txt");
	res = SimpleFS_read(fh, junk2, 20);
	printf("read %d [20]\n",res);
	SimpleFS_closeFile(fh);
	printf("[test] %s\n",junk2);
	
	fh = SimpleFS_openFile(root_dir, "file1.txt");
	res = SimpleFS_write(fh, junk, 20);
	printf("written %d [20]\n",res);

	char junk3[11];
	for(i=0;i<10;i++)
		junk3[i] = 97;
	junk3[10]=0;
	res = SimpleFS_seek(fh, 10);
	printf("seek %d [10]\n",res);
	res = SimpleFS_write(fh, junk3, 10);
	printf("written %d [10]\n",res);
	
	res = SimpleFS_seek(fh, 0);
	printf("seek %d [0]\n",res);
	res = SimpleFS_read(fh, junk2, 20);
	printf("read %d [20]\n",res);
	printf("[test] %s\n",junk2);
	SimpleFS_closeFile(fh);
	

	printf("\n\n##############################\ncreate Dir : tmp\n##############################\n\n");
	res = SimpleFS_mkDir(root_dir, "tmp");
	printf("mkDir tmp %d [0]\n",res);

	printf("\n\n##############################\nread Dir : /\n##############################\n\n");
	n = SimpleFS_readDir(names, root_dir);
	printf("n files: %d\n",n);
	for(i=0;i<n;i++) {
		printf(" - %s\n",names[i]);
	}
	
	printf("\n\n##############################\ncreate Dir : tmp\n##############################\n\n");
	res = SimpleFS_mkDir(root_dir, "tmp");
	printf("mkDir tmp %d [-1]\n",res);
	
	printf("\n\n##############################\nread Dir : /\n##############################\n\n");
	n = SimpleFS_readDir(names, root_dir);
	printf("n files: %d\n",n);
	for(i=0;i<n;i++) {
		printf(" - %s\n",names[i]);
	}

	
	printf("\n\n##############################\ncd tmp\n##############################\n\n");
	res = SimpleFS_changeDir(root_dir, "tmp");
	printf("changeDir tmp %d [0]\n",res);
	
	printf("\n\n##############################\ncd ..\n##############################\n\n");
	res = SimpleFS_changeDir(root_dir, "..");
	printf("changeDir .. %d [0]\n",res);

	printf("\n\n##############################\nrm tmp2.jpg\n##############################\n\n");
	res = SimpleFS_remove(root_dir, "tmp2.jpg");
	printf("rm tmp2.jpg %d [-1]\n",res);

	printf("\n\n##############################\nrm file2.jpg\n##############################\n\n");
	res = SimpleFS_remove(root_dir, "file2.jpg");
	printf("rm file2.jpg %d [0]\n",res);

	printf("\n\n##############################\nread Dir : /\n##############################\n\n");
	n = SimpleFS_readDir(names, root_dir);
	printf("n files: %d\n",n);
	for(i=0;i<n;i++) {
		printf(" - %s\n",names[i]);
	}
	
	for(i=0;i<10;i++)
		free(names[i]);


	SimpleFS_closeDirectory(root_dir);
	SimpleFS_close(&fs);
	DiskDriver_flush(&dd);
}
