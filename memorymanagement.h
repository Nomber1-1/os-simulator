#ifndef MEMORYMANAGEMENT_H
#define MEMORYMANAGEMENT_H
#define MAX_PAGE_TABLE_SIZE 10

typedef struct PCB // Structure to keep track of source code in memory
{
    int pid;                            // Unique process ID
    char scriptName[100];               // The name of script to be executed
    int startPos;                       // Start position of script in shell memory
    int endPos;                         // End position (start + length of the script)
    int programCounter;                 // Keeps track of the current line being executed
    int pageCounter;                    // Keeps track of the current page being executed
    int jobLengthScore;                 // Keeps track of the job length score
    int pageTable[MAX_PAGE_TABLE_SIZE]; // Page table for the PCB -> Default -1 for unallocated
    int totalLines;                     // Total number of lines in the script
    int loadedLines;                    // Number of lines loaded into memory
    struct PCB *next;                   // Pointer for ready queue
} PCB;

typedef struct ReadyQueue
{
    PCB *head;  // Pointer to the first PCB in the queue
    PCB *tail;  // Pointer to the last PCB in the queue
    int length; // The number of PCBs in the queue
} ReadyQueue;

extern ReadyQueue readyQueue;

void enqueue(PCB *pcb);
PCB *dequeue();

#endif