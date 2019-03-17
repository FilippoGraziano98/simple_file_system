# Simple File System
In this project we have implemented a Simple File System. This File System interfaces with a custom Disk Driver which manages a disk, simulated with a binary file.


## How

### Disk Driver
At initialization, the Disk Driver gets (as input parameter) the desired number of blocks and creates a binary file having enough space for the requested blocks and for the extra memory needed for storing additional information (DiskHeader and Bitmap). Thus the Disk Driver reserves the first part of the binary file to store the bitmap, indicating if blocks are free or not.

### File System
Our File Systems handles directories and files. A file can contain any type of data, while a directory contains the list of files present in the directory storing their first block in the disk.
The File System stores the root directory block in the first block of the disk.\
Moreover the File System stores a list of opened files and directories and keeps track of how many handlers/pointers have been opened for each file/directory in order to free associated memory only when all handlers have been closed.


## How to run
```
    make
    ./bitmap_test	# to test the bitmap
    ./disk_driver_test	# to test the disk driver
    ./simplefs_test	# to test the file system
```
In order to fully test this implementation, simplefs_test should be run twice. In fact reopening the binary file you should be able to see file created earlier.


## Authors
* **Filippo Graziano** - [FilippoGraziano98](https://github.com/FilippoGraziano98)\
* **Lorenzo Bianchi** - [Lollez](https://github.com/Lollez)
