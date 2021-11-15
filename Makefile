CC = gcc
CCFLAGS = -Wall -fextended-identifiers

all: crackvim


pkzip_crypto.o: pkzip_crypto.c pkzip_crypto.h
	$(CC) $(CCFLAGS) -c -o $@ $<

crackvim.o: crackvim.c crc32.h pkzip_crypto.h
	$(CC) $(CCFLAGS) -g -c -o $@ $<

crc32.o: crc32.c
	$(CC) $(CCFLAGS) -c -o $@ $<

crackvim: crackvim.o crc32.o pkzip_crypto.o
	$(CC) $(CCFLAGS) -g -o $@ $^

.PHONY: clean

clean:
	del *.o
	del crackvim.exe
