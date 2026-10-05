#ifndef SHELLMEMORY_H
#define SHELLMEMORY_H
#include "memorymanagement.h"
#define MEM_SIZE 1000
#define MAX_LINE_LENGTH 100
#define FRAME_SIZE 3

#ifndef VARMEMSIZE
#define VARMEMSIZE 0
#endif

#ifndef FRAMESIZE
#define FRAMESIZE 0
#endif

#define VARIABLE_STORE_SIZE VARMEMSIZE
#define FRAME_STORE_SIZE FRAMESIZE
#define BACKING_STORE_PATH "my_backing_store"

extern int lru_array[FRAME_STORE_SIZE / 3];

// 1. Backing Store
void create_backing_store();
void delete_backing_store();

// 3. Memory Seperation
void shell_init();
void frame_set_value(char *var_in, char *value_in);
void variable_set_value(char *var_in, char *value_in);
char *frame_get_value(char *var_in);
char *variable_get_value(char *var_in);

int getLRUFrame(PCB *pcb);
void incrementTimeSinceLastUsed(PCB *pcb);

#endif