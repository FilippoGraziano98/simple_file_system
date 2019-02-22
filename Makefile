CCOPTS= -Wall -g -std=gnu99 -Wstrict-prototypes
LIBS= 
CC=gcc
AR=ar


BINS= simplefs_test

OBJS = simplefs.o disk_driver.o bitmap.o linked_list.o #add here your object files

HEADERS=bitmap.h\
	disk_driver.h\
	simplefs.h

%.o:	%.c $(HEADERS)
	$(CC) $(CCOPTS) -c -o $@  $<

.phony: clean all


all:	$(BINS) 

simplefs_test: simplefs_test.c $(OBJS) 
	$(CC) $(CCOPTS)  -o $@ $^ $(LIBS)

clean:
	rm -rf *.o *~  $(BINS) fs
