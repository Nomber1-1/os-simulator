.PHONY: clean all

framesize ?= 30
varmemsize ?= 30
CC=gcc
CFLAGS= -D FRAMESIZE=$(framesize) -D VARMEMSIZE=$(varmemsize)

mysh: memorymanagement.c shell.c interpreter.c shellmemory.c
	$(CC) $(CFLAGS) -c memorymanagement.c shell.c interpreter.c shellmemory.c
	$(CC) $(CFLAGS) -o mysh memorymanagement.o shell.o interpreter.o shellmemory.o 
	

clean:
	rm -f mysh *.o