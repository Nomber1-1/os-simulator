#include <stdio.h>
#include "memorymanagement.h"

ReadyQueue readyQueue = {NULL, NULL, 0}; // Initialize the ready queue

/**
 * Function to add a process control block (PCB) into the ready queue.
 * The PCB is added to the end of the queue.
 *
 * @param pcb The process control block to add to the queue
 *
 * @return void
 */
void enqueue(PCB *pcb)
{
    if (readyQueue.tail)
    {
        readyQueue.tail->next = pcb;
    }
    else // If empty queue
    {
        readyQueue.head = pcb;
    }
    readyQueue.tail = pcb;
    readyQueue.length += 1;
}
/**
 * Function to remove a process control block (PCB) from the ready queue.
 * The PCB is removed from the front of the queue.
 *
 * @return The process control block removed from the queue
 *        or NULL if the queue is empty
 */
PCB *dequeue()
{
    if (!readyQueue.head) // Check if queue is empty
    {
        return NULL;
    }

    PCB *pcb = readyQueue.head;
    readyQueue.head = readyQueue.head->next;
    pcb->next = NULL;
    readyQueue.length -= 1;
    if (!readyQueue.head) // Check if the queue is empty after current PCB
    {
        readyQueue.tail = NULL;
    }

    return pcb;
}