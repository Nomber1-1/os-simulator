#define _DEFAULT_SOURCE // for my_ls -> scandir
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>
#include <unistd.h>
#include <pthread.h>
#include "shellmemory.h"
#include "shell.h"
#include "memorymanagement.h"

#define MAX_ARGS_SIZE 7
int MAX_WORD_LENGTH = 100;

int lineMemoryCounter = 0; // Counter for memory slots
int nextPid = 1;           // Counter for unique process IDs
int isMultithreaded = 0;
pthread_t workerThreads[2];
pthread_cond_t qu_empty = PTHREAD_COND_INITIALIZER;
pthread_mutex_t printMutex = PTHREAD_MUTEX_INITIALIZER;
char *multithreadingPolicy;
int isTerminated = 0;

PCB backgroundPCBs[3]; // Background process control blocks

/**
 * Function to print a message when a bad command is called.
 *
 * @return 1
 */
int badcommand()
{
    printf("Unknown Command\n");
    return 1;
}

/**
 * Function to print a message when a bad command is not found.
 *
 * @return 3
 */
int badcommandFileDoesNotExist()
{
    printf("Bad command: File not found\n");
    return 3;
}

/**
 * Function to print a message when a command has too many tokens.
 *
 * @return 2
 */
int badcommandTooManyTokens()
{
    printf("Bad command: Too many tokens\n");
    return 2;
}

/**
 * Function to print a message when mydir command is called.
 *
 * @return 4
 */
int badcommandMyMkdir()
{
    printf("Bad command: my_mkdir\n");
    return 4;
}

/**
 * Function to print a message when bad cd command is called.
 *
 * @return 5
 */
int badcommandMyCd()
{
    printf("Bad command: my_cd\n");
    return 5;
}

// Declaring functions
void *workerFunction(void *arg);
void initializeThreadPool();
void joinThreadPool();
int load(FILE *p, PCB *pcb);
int compare(const void *a, const void *b);
int goToNextLineWithFrame(PCB *pcb);
void scheduler(char *policy, int isBackground);
void playQueue();
void playQueueRR();
void playQueueAging();
char *copyFileToBackingStore(FILE *f, char *script);
int countNumberLines(char *backStoreFilePath);
int help();
int quit();
int set(char *var, char *value[], int args_size);
int print(char *var);
int run(char *script);
int exec(char *args[], int args_size);
int echo(char *var);
int my_ls();
int my_mkdir(char *dirname);
int my_touch(char *filename);
int my_cd(char *dirname);

/**
 * Function to interpret a command and execute it if the command
 * matches with a list of available commands.
 *
 * @param command_args The arguments of the command
 * @param args_size The number of arguments
 *
 * @return The return value of the executed command
 */
int interpreter(char *command_args[], int args_size)
{
    int i;

    if (args_size > MAX_ARGS_SIZE)
    {
        return badcommandTooManyTokens();
    }

    if (args_size < 1)
    {
        return badcommand();
    }

    for (i = 0; i < args_size; i++)
    { // terminate args at newlines
        command_args[i][strcspn(command_args[i], "\r\n")] = 0;
    }

    if (strcmp(command_args[0], "help") == 0)
    {
        // help
        if (args_size != 1)
            return badcommand();
        return help();
    }
    else if (strcmp(command_args[0], "quit") == 0)
    {
        // quit
        if (args_size != 1)
            return badcommand();
        return quit();
    }
    else if (strcmp(command_args[0], "set") == 0)
    {
        // set
        if (args_size < 3)
            return badcommand();
        return set(command_args[1], command_args, args_size);
    }
    else if (strcmp(command_args[0], "print") == 0)
    {
        if (args_size != 2)
            return badcommand();
        return print(command_args[1]);
    }
    else if (strcmp(command_args[0], "run") == 0)
    {
        if (args_size != 2)
            return badcommand();
        return run(command_args[1]);
    }
    else if (strcmp(command_args[0], "exec") == 0)
    {
        if (args_size < 3)
            return badcommand();
        return exec(command_args, args_size);
    }
    else if (strcmp(command_args[0], "echo") == 0)
    {
        if (args_size < 2)
            return badcommand();
        return echo(command_args[1]);
    }
    else if (strcmp(command_args[0], "my_ls") == 0)
    {
        if (args_size != 1)
            return badcommand();
        return my_ls();
    }
    else if (strcmp(command_args[0], "my_mkdir") == 0)
    {
        if (args_size != 2)
            return badcommand();
        return my_mkdir(command_args[1]);
    }
    else if (strcmp(command_args[0], "my_touch") == 0)
    {
        if (args_size != 2)
            return badcommand();
        return my_touch(command_args[1]);
    }
    else if (strcmp(command_args[0], "my_cd") == 0)
    {
        if (args_size != 2)
            return badcommand();
        return my_cd(command_args[1]);
    }
    else
        return badcommand();
}

/**
 * Function to display a help message with a list of
 * available commands and their descriptions.
 *
 * @return always returns 0
 */
int help()
{

    // note the literal tab characters here for alignment
    char help_string[] = "COMMAND			DESCRIPTION\n \
help			Displays all the commands\n \
quit			Exits / terminates the shell with “Bye!”\n \
set VAR STRING		Assigns a value to shell memory\n \
print VAR		Displays the STRING assigned to VAR\n \
run SCRIPT.TXT		Executes the file SCRIPT.TXT\n ";
    printf("%s\n", help_string);
    return 0;
}

/**
 * Function to quit the shell by printing a farewell message
 * and exit the shell with a status code of 0.
 *
 * @return 0
 */
int quit()
{
    printf("Bye!\n"); // Farewell message
    if (isMultithreaded == 1)
    {
        if (workerThreads[0] == 0)
        {
            initializeThreadPool();
            pthread_cond_wait(&qu_empty, &printMutex);
        }
        isTerminated = 1;
        pthread_cond_wait(&qu_empty, &printMutex);
    }
    delete_backing_store();
    exit(0); // Exit the shell
}

/**
 * Function to set a value in shell memory. If the variable already
 * exists, the value is updated. If the variable does not
 * exist, a new entry is created.
 *
 * @param var The variable to set in memory
 * @param value The value to assign to the variable
 * @param args_size The number of arguments
 *
 * @return 0 if the variable is set successfully
 */
int set(char *var, char *value[], int args_size)
{
    char *link = "=";

    // Create a buffer to store the value
    char buffer[MAX_USER_INPUT];

    if (args_size > 3)
    {
        int i = 2;
        strcpy(buffer, value[i++]);
        // Concatenate the tokens into the buffer
        while (i < args_size)
        {
            strcat(buffer, link);
            strcat(buffer, value[i]);
            i++;
        }

        // Replace "=" with " " in buffer
        for (i = 0; i < strlen(buffer); i++)
        {
            if (buffer[i] == '=')
            {
                buffer[i] = ' ';
            }
        }
        variable_set_value(var, buffer);
    }
    else
    {
        variable_set_value(var, value[2]);
    }

    return 0;
}

/**
 * Function to print a value from shell memory to the console.
 *
 * @param var The variable to retrieve from memory
 *
 * @return 0
 */
int print(char *var)
{
    char *value = variable_get_value(var);
    if (value)
    {
        printf("%s\n", value);
        free(value);
    }
    else
    {
        printf("Variable does not exist\n");
    }
    return 0;
}

/**
 * Function to initialize the thread pool by creating two worker threads.
 */
void initializeThreadPool()
{
    pthread_mutex_init(&printMutex, NULL);
    for (int i = 0; i < 2; i++)
    {
        pthread_create(&workerThreads[i], NULL, workerFunction, NULL);
    }
}

/**
 * Function to join the threads in the thread pool to the main thread.
 */
void joinThreadPool()
{
    for (int i = 0; i < 2; i++)
    {
        pthread_join(workerThreads[i], NULL);
    }
    pthread_mutex_destroy(&printMutex);
    pthread_cond_signal(&qu_empty);
}

/**
 * Function that the threads in the thread pool execute.
 */
void *workerFunction(void *arg)
{
    while (1)
    {
        pthread_mutex_lock(&printMutex);

        //  Check if the ready queue is empty
        if (readyQueue.length == 0)
        {

            pthread_cond_signal(&qu_empty);
            pthread_mutex_unlock(&printMutex);
            if (isTerminated)
            {
                joinThreadPool();
            }
            continue;
        }

        pthread_mutex_unlock(&printMutex);

        if (strcmp(multithreadingPolicy, "RR30") == 0)
        {
            playQueueRR(30);
        }
        else
        {
            playQueueRR(2);
        }
    }
}

/**
 * Function to load a script into the shell memory.
 *
 * @param script The name of the script file to load
 *
 * @return the length of the script loaded into memory, or -1 if an error occurs
 */
int load(FILE *p, PCB *pcb) // Load source code into the frame memory
{
    int lineCount = 0;
    int frameCount = 0;
    int endPos;
    char line[MAX_USER_INPUT];
    char buf[MAX_USER_INPUT];
    char buf1[MAX_USER_INPUT];

    // Read the file line by line
    fgets(line, MAX_USER_INPUT - 1, p);
    while (1)
    {
        // At start of new frame, find next available space in memory
        sprintf(buf, "line%d", lineMemoryCounter);
        if (lineMemoryCounter % 3 == 0)
        {
            while (strcmp(frame_get_value(buf), " ") != 0 && lineMemoryCounter < FRAME_STORE_SIZE)
            {
                // Skip to start of next frame
                while (lineMemoryCounter % 3 != 0)
                {
                    lineMemoryCounter++;
                };
                sprintf(buf, "line%d", lineMemoryCounter);
            }

            // Check if memory is full
            if (lineMemoryCounter >= FRAME_STORE_SIZE)
            {
                printf("Error - Ran out of shell memory\n");
                return -1; // Return error code if memory is full
            }
        }

        // Remove newline character
        if (line[strlen(line) - 1] == '\n')
        {
            line[strlen(line) - 1] = '\0'; // Remove extra '\n' from end of lines
        }

        // Load the line into memory
        frame_set_value(buf, line); // Load line into memory at next available space

        // Update the page table
        if (lineMemoryCounter % 3 == 0)
        {
            pcb->pageTable[frameCount] = lineMemoryCounter / 3;
            lru_array[lineMemoryCounter / 3] = 0;
            frameCount++;
        }

        memset(line, 0, sizeof(line)); // Reset the line

        lineMemoryCounter++;
        lineCount++;

        if (feof(p) || lineCount >= 6)
        {
            pcb->loadedLines = lineCount;
            endPos = lineMemoryCounter;
            break;
        }

        fgets(line, MAX_USER_INPUT - 1, p);
    }

    // Make sure the frame memory is a multiple of 3
    // To be at the start of the next frame
    while (lineMemoryCounter % 3 != 0)
    {
        lineMemoryCounter++;
    };

    // Return the last line loaded into memory
    return endPos;
}

/**
 * Function to compare two process control blocks (PCBs) based on their job length score.
 *
 * @param a The first process control block
 * @param b The second process control block
 *
 * @return 0 if the job length scores are equal, -1 if a < b, 1 if a > b
 */
int compare(const void *a, const void *b)
{
    const PCB *pcb_a = *(const PCB **)a; // Use a to cast to PCB
    const PCB *pcb_b = *(const PCB **)b; // Use b to cast to PCB

    return ((pcb_a->jobLengthScore > pcb_b->jobLengthScore) - (pcb_a->jobLengthScore < pcb_b->jobLengthScore));
}

/**
 * Function to reorder the ready queue based on job length score.
 *
 * @return void
 */
void reorderReadyQueue()
{
    int readyLength = readyQueue.length;
    if (readyLength <= 1)
        return; // No need to sort if the queue has 0 or 1 element

    // Extract PCB pointers into an array
    PCB *pcbPointers[readyLength];
    PCB *temp = readyQueue.head;
    for (int i = 0; i < readyLength; i++)
    {
        pcbPointers[i] = temp;
        temp = temp->next;
    }

    // Sort the array using qsort
    qsort(pcbPointers, readyLength, sizeof(PCB *), compare);

    // Rebuild the queue from the sorted array
    readyQueue.head = pcbPointers[0];
    for (int i = 0; i < readyLength - 1; i++)
    {
        pcbPointers[i]->next = pcbPointers[i + 1];
    }
    pcbPointers[readyLength - 1]->next = NULL;
}

/**
 * Function to replace a random page in memory
 */
void evictPage(PCB *pcb, FILE *p, char *line0)
{
    int frameToEvict = getLRUFrame(pcb);
    int startOfFrameToEvict = frameToEvict * 3;

    pcb->programCounter = startOfFrameToEvict;

    // Print the content of the victim page
    printf("Page fault! Victim page contents:\n\n");
    for (int i = 0; i < 3; i++)
    {
        char buf[MAX_USER_INPUT];
        sprintf(buf, "line%d", startOfFrameToEvict + i);
        printf("%s\n", frame_get_value(buf));
    }
    printf("\nEnd of victim page contents.\n");

    char buf1[MAX_USER_INPUT];
    char line[MAX_USER_INPUT];

    // Replace the page with the new page
    sprintf(buf1, "line%d", startOfFrameToEvict);
    // Remove newline character
    if (line0[strlen(line0) - 1] == '\n')
    {
        line0[strlen(line0) - 1] = '\0'; // Remove extra '\n' from end of lines
    }
    frame_set_value(buf1, line0);

    int j = 1;

    while (j < 3 && !feof(p))
    {
        fgets(line, MAX_USER_INPUT - 1, p);
        sprintf(buf1, "line%d", startOfFrameToEvict + j);

        // Remove newline character
        if (line[strlen(line) - 1] == '\n')
        {
            line[strlen(line) - 1] = '\0'; // Remove extra '\n' from end of lines
        }

        frame_set_value(buf1, line);

        memset(line, 0, sizeof(line));
        j++;
    }

    pcb->loadedLines += j;
    pcb->endPos = startOfFrameToEvict + j;

    while (j < 3)
    {
        sprintf(buf1, "line%d", startOfFrameToEvict + j);
        frame_set_value(buf1, " ");
        j++;
    }

    // Update the page table of the current PCB and of the PCB that is being replaced
    // Add the new frame to the page table at the first "-1"
    // If the page replaced in the page table, was a page of the current script, update the page table
    for (int i = 0; i < MAX_PAGE_TABLE_SIZE; i++)
    {
        if (pcb->pageTable[i] == frameToEvict)
        {
            pcb->pageTable[i] = -1;
        }
        else if (i > pcb->pageCounter && pcb->pageTable[i] == -1)
        {
            pcb->pageTable[i] = frameToEvict;
            break;
        }
    }

    // Go through the ready queue and update the page table of the PCBs
    PCB *temp = readyQueue.head;
    while (temp != NULL)
    {
        if (strcmp(temp->scriptName, pcb->scriptName) == 0)
        {
            // Update the page table, end position and # of loaded lines in
            // all duplicate pcbs to avoid another page fault
            temp->endPos = pcb->endPos;
            temp->loadedLines = pcb->loadedLines;
            for (int i = 0; i < MAX_PAGE_TABLE_SIZE; i++)
            {
                temp->pageTable[i] = pcb->pageTable[i];
            }
        }
        else
        {
            for (int i = 0; i < MAX_PAGE_TABLE_SIZE; i++)
            {
                if (temp->pageTable[i] == frameToEvict)
                {
                    temp->pageTable[i] = -1;
                }
                break;
            }
        }
        temp = temp->next;
    }

    pcb->pageCounter += 1;

    fclose(p);
}

/**
 * Function to handle a page fault by loading the next page into memory.
 * If there is no free space in memory, a random page is replaced.
 *
 * @param script The name of the script file
 * @param pcb The process control block (PCB) of the script
 *
 * @return int the start position of the new page
 */
int loadNextPage(char *script, PCB *pcb)
{
    char *backStorePath = BACKING_STORE_PATH;
    char backStoreFilePath[MAX_USER_INPUT];
    strcpy(backStoreFilePath, backStorePath);
    strcat(backStoreFilePath, "/");
    strcat(backStoreFilePath, script);
    FILE *p = fopen(backStoreFilePath, "rt");

    int lineCount = 0;
    int frameCount = pcb->pageCounter;
    int newStartPos;
    char line[MAX_USER_INPUT];
    char buf[MAX_USER_INPUT];

    // Check if duplicate has already done the page fault
    if (pcb->loadedLines == pcb->totalLines)
    {
        pcb->programCounter = pcb->startPos;
        int count = goToNextLineWithFrame(pcb);
        return count;
    }

    // Skip the loaded lines
    for (int j = 0; j < pcb->loadedLines; j++)
    {
        fgets(line, MAX_USER_INPUT - 1, p);
    }
    // Read the file line by line
    fgets(line, MAX_USER_INPUT - 1, p);

    // At start of new frame, find next available space in memory
    sprintf(buf, "line%d", lineMemoryCounter);
    if (lineMemoryCounter % 3 == 0)
    {
        while (strcmp(frame_get_value(buf), " ") != 0 && lineMemoryCounter < FRAME_STORE_SIZE)
        {
            // Skip to start of next frame
            while (lineMemoryCounter % 3 != 0)
            {
                lineMemoryCounter++;
            };
            sprintf(buf, "line%d", lineMemoryCounter);
        }
        // Check if memory is full
        if (lineMemoryCounter >= FRAME_STORE_SIZE)
        {
            // Replace a page
            evictPage(pcb, p, line);
            return pcb->programCounter;
        }
    }

    printf("Page fault!\n");
    while (1)
    {
        sprintf(buf, "line%d", lineMemoryCounter);
        // Remove newline character
        if (line[strlen(line) - 1] == '\n')
        {
            line[strlen(line) - 1] = '\0'; // Remove extra '\n' from end of lines
        }

        if (lineCount == 0)
        {
            newStartPos = lineMemoryCounter;
        }

        // Load the line into memory at next available space
        frame_set_value(buf, line);

        // Update the page table
        if (lineMemoryCounter % 3 == 0)
        {
            pcb->pageTable[frameCount] = lineMemoryCounter / 3;
            frameCount++;
        }

        // Reset the line
        memset(line, 0, sizeof(line));

        lineMemoryCounter++;
        lineCount++;

        if (feof(p) || lineCount >= 3)
        {
            pcb->loadedLines += lineCount;
            break;
        }

        fgets(line, MAX_USER_INPUT - 1, p);
    }

    pcb->endPos = lineMemoryCounter;

    // Update duplicate pcbs to avoid another page fault
    PCB *temp = readyQueue.head;
    while (temp != NULL)
    {
        if (strcmp(temp->scriptName, pcb->scriptName) == 0)
        {
            temp->endPos = pcb->endPos;
            temp->loadedLines = pcb->loadedLines;
            for (int i = 0; i < MAX_PAGE_TABLE_SIZE; i++)
            {
                temp->pageTable[i] = pcb->pageTable[i];
            }
        }
        temp = temp->next;
    }

    // Make sure the frame memory is a multiple of 3
    // To be at the start of the next frame
    while (lineMemoryCounter % 3 != 0)
    {
        lineMemoryCounter++;
    };

    fclose(p);

    // Return the first line of the new page
    return newStartPos;
}

/**
 * Function to get the next line to execute based on the page table.
 *
 * @param pcb Current PCB being executed
 * @return void
 */
int goToNextLineWithFrame(PCB *pcb)
{
    int nextLine = pcb->programCounter + 1;
    int nextFrame = nextLine / 3;
    int currentPage = pcb->pageCounter;
    int currentFrame = pcb->pageTable[currentPage];

    // Check if the next line is in the next frame
    // If it is, check if the next page exists and go to the next page
    // If the next page does not exist, but we know more lines exists, trigger a page fault
    // If the next page does not exist and there are no more lines, end of script
    // If the next line is in the same page, move to the next line

    if (nextFrame != currentFrame) // Check if the next line is in the next frame
    {
        // Set the next line to the beginning of the next page (if it exists)
        // If the next page exists, go to the next page
        if (pcb->pageTable[pcb->pageCounter + 1] != -1)
        {
            pcb->pageCounter++;
            return pcb->pageTable[pcb->pageCounter] * 3;
        }
        // If the next page does not exist, but we know more lines exists, trigger a page fault
        else if (pcb->loadedLines != pcb->totalLines)
        {
            // Trigger Page Fault -> Load next page
            pcb->startPos = pcb->programCounter;
            return -1;
        }
        // If the next page does not exist and there are no more lines, end of script
        else
        {
            return pcb->endPos;
        }
    }
    else if (pcb->programCounter == pcb->endPos - 1)
    {
        return pcb->endPos;
    }
    // If the next line is in the same page, move to the next line
    return nextLine;
}

/**
 * Function to schedule processes based on a given policy.
 *
 * @param policy The scheduling policy to use
 * @param pcbs The array of process control blocks (PCBs)
 * @param argsSize The number of arguments
 *
 * @return void
 */
void scheduler(char *policy, int isBackground)
{
    // Check if the policy is valid
    if ((strcmp(policy, "FCFS") == 0 || strcmp(policy, "RR") == 0 || strcmp(policy, "RR30") == 0) && isBackground == 0)
    {

        if (strcmp(policy, "RR") == 0)
        {
            playQueueRR(2);
        }
        else if (strcmp(policy, "RR30") == 0)
        {
            playQueueRR(30);
        }
        else
        {
            playQueue();
        }
    }
    else
    {
        if (isBackground == 0)
        {
            reorderReadyQueue();
            if (strcmp(policy, "SJF") == 0)
            {
                playQueue();
            }
            else
            {
                playQueueAging();
            }
        }
    }
}

/**
 * Function to play the queue of processes in First-Come, First-Served (FCFS) order.
 *
 * @return void
 */
void playQueue()
{
    PCB *pcb = dequeue();
    while (pcb) // While queue not empty
    {
        char buf[MAX_USER_INPUT];
        while (pcb->programCounter != pcb->endPos) // While lines exist
        {
            if (pcb->programCounter == -1)
            {
                pcb->programCounter = loadNextPage(pcb->scriptName, pcb);
                break;
            }
            // Interpret the current line
            sprintf(buf, "line%d", pcb->programCounter);
            parseInput(frame_get_value(buf));
            incrementTimeSinceLastUsed(pcb);
            pcb->programCounter = goToNextLineWithFrame(pcb); // Move to the next line
        }
        if (pcb->programCounter != pcb->endPos) // Check if early switch
        {
            pthread_mutex_lock(&printMutex);
            enqueue(pcb);
            pthread_mutex_unlock(&printMutex);
        }
        pcb = dequeue(); // Move to the next process
    }
}

/**
 * Function to play the queue of processes in Round Robin (RR) order.
 *
 * @return void
 */
void playQueueRR(int limit)
{
    //  Run the pcbs with RR policy
    PCB *pcb;

    pthread_mutex_lock(&printMutex);
    pcb = dequeue();
    pthread_mutex_unlock(&printMutex);

    while (pcb) // While queue not empty
    {
        int count = 0;
        char buf[MAX_USER_INPUT];
        while (pcb->programCounter != pcb->endPos && count < limit) // While lines exist
        {
            if (pcb->programCounter == -1)
            {
                pcb->programCounter = loadNextPage(pcb->scriptName, pcb);
                break;
            }
            // Interpret the current line
            sprintf(buf, "line%d", pcb->programCounter);

            parseInput(frame_get_value(buf));
            incrementTimeSinceLastUsed(pcb);

            // Need to move lines using the page table
            pcb->programCounter = goToNextLineWithFrame(pcb); // Move to the next line
            count++;
        }

        if (pcb->programCounter != pcb->endPos) // Check if early switch
        {
            pthread_mutex_lock(&printMutex);
            enqueue(pcb);
            pthread_mutex_unlock(&printMutex);
        }

        // Move to the next process

        pthread_mutex_lock(&printMutex);
        pcb = dequeue();
        pthread_mutex_unlock(&printMutex);
    }
}

/**
 * Function to play the queue of processes with AGING policy.
 * If a process has not finished executing, it is
 * reordered based on job length score.
 *
 * @param pcbs The array of process control blocks (PCBs)
 * @param argsSize The number of arguments
 *
 * @return void
 */
void playQueueAging()
{
    //  Run the pcbs with AGING policy
    PCB *pcb = dequeue();
    while (pcb) // While queue not empty
    {
        char buf[MAX_USER_INPUT];
        int reorder = 0;

        while (pcb->programCounter < pcb->endPos && reorder == 0)
        {
            reorder = 0;
            // Interpret the current line
            sprintf(buf, "line%d", pcb->programCounter);
            parseInput(frame_get_value(buf));
            incrementTimeSinceLastUsed(pcb);
            pcb->programCounter = goToNextLineWithFrame(pcb); // Move to the next line

            // Age each job except for head
            PCB *temp = readyQueue.head;
            while (temp != NULL)
            {
                if (temp->jobLengthScore != 0)
                {
                    temp->jobLengthScore -= 1;
                }
                temp = temp->next;
            }

            // Check if reorder
            PCB *newHead = readyQueue.head;
            temp = readyQueue.head;
            int minJobLengthScore = pcb->jobLengthScore;

            while (temp != NULL)
            {
                if (temp->jobLengthScore < minJobLengthScore)
                {
                    minJobLengthScore = temp->jobLengthScore;
                    newHead = temp;
                    reorder = 1;
                }
                temp = temp->next;
            }

            // Reorder
            if (reorder == 1)
            {
                readyQueue.head = newHead;
                temp = newHead;
                while (temp->next != NULL && temp->next->jobLengthScore <= pcb->jobLengthScore)
                {
                    temp = temp->next;
                }
                temp->next = pcb;
            }
        }
        pcb = dequeue(); // Move to the next process
    }
}

/**
 * Function to copy a file into the backing store.
 */
char *copyFileToBackingStore(FILE *f, char *script)
{
    // Copy file into backing store
    char *backStorePath = BACKING_STORE_PATH;
    char backStoreFilePath[MAX_USER_INPUT];
    strcpy(backStoreFilePath, backStorePath);
    strcat(backStoreFilePath, "/");
    strcat(backStoreFilePath, script);

    FILE *f2 = fopen(backStoreFilePath, "wt");
    char line[MAX_USER_INPUT];
    while (fgets(line, MAX_USER_INPUT - 1, f))
    {
        fprintf(f2, "%s", line);
    }
    // Close the files after being copied
    fclose(f2);
    char *backStoreFilePathCopy = strdup(backStoreFilePath);
    return backStoreFilePathCopy;
}

/**
 * Function to count the total number of lines in a script
 */
int countNumberLines(char *backStoreFilePath)
{
    FILE *p = fopen(backStoreFilePath, "rt");
    int count = 0;
    char line[MAX_USER_INPUT];
    while (fgets(line, MAX_USER_INPUT - 1, p))
    {
        count++;
    }
    fclose(p);
    return count;
}

/**
 * Function to change the current directory to the specified directory.
 *
 * @param dirname The name of the directory to change to
 *
 * @return 0 if the directory is changed successfully
 */
int run(char *script)
{
    int errCode = 0; // Var to track the error code

    FILE *f = fopen(script, "rt"); // Open the script file in read mode

    // If the file does not exist, return an error
    if (f == NULL)
    {
        return badcommandFileDoesNotExist();
    }

    // Copy the file into the backing store
    char *backStoreFilePath = copyFileToBackingStore(f, script);
    fclose(f);

    FILE *f2 = fopen(backStoreFilePath, "rt");
    int pcbSize = 1;
    PCB pcbs[pcbSize];

    // Initialize Page Table to -1
    for (int i = 0; i < MAX_PAGE_TABLE_SIZE; i++)
    {
        pcbs[0].pageTable[i] = -1;
    }

    // Load the script into memory and get the start and end positions
    int startPos = lineMemoryCounter;
    int endPos = load(f2, &pcbs[0]);

    // If there was an error loading the script, return an error
    if (endPos == -1)
    {
        printf("Error - Error when loading %s.\n", script);
        return -1;
    }

    // Update the PCB for the script
    strcpy(pcbs[0].scriptName, script);
    pcbs[0].startPos = startPos;
    pcbs[0].endPos = endPos;
    pcbs[0].jobLengthScore = endPos - startPos;
    pcbs[0].programCounter = startPos;
    pcbs[0].pageCounter = 0;
    pcbs[0].totalLines = countNumberLines(backStoreFilePath);
    pcbs[0].pid = nextPid++;
    pcbs[0].next = NULL;

    fclose(f2);

    pthread_mutex_lock(&printMutex);
    enqueue(pcbs); // Enqueue the PCB
    pthread_mutex_unlock(&printMutex);

    // Call the scheduler with the FCFS policy
    scheduler("FCFS", 0);

    // Return the error code
    return errCode;
}

/**
 * Function to execute a list of scripts based on a given policy.
 *
 * @param args The list of scripts to execute
 * @param args_size The number of arguments
 *
 * @return 0 if the scripts are executed successfully
 */
int exec(char *args[], int args_size)
{
    int isBackground = 0;

    // Check if "MT" or "#"" are present in args
    if (strcmp(args[args_size - 1], "MT") == 0)
    {
        args_size -= 1;
        isMultithreaded = 1;
    }
    if (strcmp(args[args_size - 1], "#") == 0 || strcmp(args[args_size - 2], "#") == 0)
    {
        args_size -= 1;
        isBackground = 1;
    }

    // Check if the policy is valid
    char *policy = args[args_size - 1];
    if (strcmp(policy, "FCFS") != 0 && strcmp(policy, "SJF") != 0 && strcmp(policy, "RR") != 0 && strcmp(policy, "RR30") != 0 && strcmp(policy, "AGING") != 0)
    {
        printf("Error - '%s' is not a valid policy.\n", policy);
        return 0;
    }

    // Check if the policy is multithreaded
    if (isMultithreaded == 1)
    {
        multithreadingPolicy = policy;
    }

    int startPos, endPos;
    int pcbSize = args_size - 2;
    PCB pcbs[pcbSize];
    char script[MAX_USER_INPUT];
    FILE *f;

    int exists = -1;
    // Load the scripts into memory
    for (int i = 0; i < pcbSize; i++)
    {
        // Check if a pcb already exists for the script
        for (int j = 0; j < i; j++)
        {
            if (strcmp(args[i + 1], pcbs[j].scriptName) == 0)
            {
                exists = j;
                break;
            }
        }

        // If a pcb already exists for the script, copy the values from the existing pcb
        if (exists != -1)
        {
            strcpy(pcbs[i].scriptName, pcbs[exists].scriptName);
            pcbs[i].startPos = pcbs[exists].startPos;
            pcbs[i].endPos = pcbs[exists].endPos;
            pcbs[i].jobLengthScore = pcbs[exists].jobLengthScore;
            pcbs[i].programCounter = pcbs[exists].programCounter;
            pcbs[i].pageCounter = pcbs[exists].pageCounter;
            pcbs[i].loadedLines = pcbs[exists].loadedLines;
            pcbs[i].totalLines = pcbs[exists].totalLines;
            pcbs[i].pid = nextPid++;
            pcbs[i].next = NULL;

            // Copy the page Table
            for (int k = 0; k < MAX_PAGE_TABLE_SIZE; k++)
            {
                pcbs[i].pageTable[k] = pcbs[exists].pageTable[k];
            }

            // Enqueue the PCB
            pthread_mutex_lock(&printMutex);
            enqueue(pcbs + i);
            pthread_mutex_unlock(&printMutex);
            exists = -1;
        }
        else
        {
            strcpy(script, args[i + 1]);
            f = fopen(script, "rt");

            if (f == NULL)
            {
                printf("Error - Error when loading %s.\n", script);
                return -1;
            }

            char *backStoreFilePath = copyFileToBackingStore(f, script);
            fclose(f);

            // Open the file from the backing store
            FILE *f2 = fopen(backStoreFilePath, "rt");

            // Initialize Page Table to -1
            for (int j = 0; j < MAX_PAGE_TABLE_SIZE; j++)
            {
                pcbs[i].pageTable[j] = -1;
            }

            // Load the script into memory and get the start and end positions
            startPos = lineMemoryCounter;
            endPos = load(f2, &pcbs[i]);

            if (endPos == -1)
            {
                printf("Error - Error when loading %s.", script);
                return -1;
            }

            if (isBackground == 0)
            {
                // Update the PCB values in the local array
                strcpy(pcbs[i].scriptName, script);
                pcbs[i].startPos = startPos;
                pcbs[i].endPos = endPos;
                pcbs[i].jobLengthScore = endPos - startPos;
                pcbs[i].programCounter = startPos;
                pcbs[i].pageCounter = 0;
                pcbs[i].totalLines = countNumberLines(backStoreFilePath);
                pcbs[i].pid = nextPid++;
                pcbs[i].next = NULL;

                fclose(f2);

                // Enqueue the PCB
                pthread_mutex_lock(&printMutex);
                enqueue(pcbs + i);
                pthread_mutex_unlock(&printMutex);
            }
            else
            {
                // Save the PCB values in the background PCB array for background processing
                strcpy(backgroundPCBs[i].scriptName, script);
                backgroundPCBs[i].startPos = startPos;
                backgroundPCBs[i].endPos = endPos;
                backgroundPCBs[i].jobLengthScore = endPos - startPos;
                backgroundPCBs[i].programCounter = startPos;
                backgroundPCBs[i].pageCounter = 0;
                backgroundPCBs[i].totalLines = countNumberLines(backStoreFilePath);
                backgroundPCBs[i].pid = nextPid++;
                backgroundPCBs[i].next = NULL;

                fclose(f2);

                // Enqueue the PCB
                pthread_mutex_lock(&printMutex);
                enqueue(backgroundPCBs + i);
                pthread_mutex_unlock(&printMutex);
            }
        }
    }

    // Call the scheduler with the specified policy
    if (isMultithreaded == 0)
    {
        scheduler(policy, isBackground);
    }
    else if (isBackground == 0 && workerThreads[0] == 0)
    {
        initializeThreadPool();
    }

    return 0;
}

/**
 * Function to print a value to the console. If the value is a variable,
 * the function retrieves the value from the shell memory. If the value
 * is a string, the function prints the string to the console.
 *
 * @param var The variable or string to print
 *
 * @return 0
 */
int echo(char *var)
{
    int must_free = 0;
    // is it a var?
    if (var[0] == '$')
    {
        var++; // advance pointer, so that tok is now the stuff after '$'
        var = variable_get_value(var);
        if (var == " ")
        {
            var = ""; // must use empty string, can't pass NULL to printf
        }
        else
        {
            must_free = 1;
        }
    }

    printf("%s\n", var);

    // memory management technically optional for this assignment
    if (must_free)
        free(var);

    return 0;
}

/**
 * Function to list files in the current directory.
 *
 * @return 0
 */
int my_ls()
{
    struct dirent **namelist;                         // Create a list of directory entries
    int n = scandir(".", &namelist, NULL, alphasort); // Scan the current directory and store the entries in the list
    int i = 0;                                        // Initialize a counter for the list

    // Check if the list is empty
    if (n == -1)
        perror("scandir"); // Print an error message if the list is empty
    else
    {
        // Iterate over the directory entries and print the names of the files
        while (i < n)
        {
            if (namelist[i]->d_name[0] != '.')
            {
                printf("%s\n", namelist[i]->d_name);
            }
            free(namelist[i++]);
        }
        free(namelist);
    }
    return 0; // Return 0 once the files have been listed
}

/**
 * Function to create a creates a new directory with the specified name
 * after removing whitespace and make the sure that the directory name
 * starts with '$' character.
 *
 * @param dirname The name of the directory to create
 *
 * @return 0 on success, badcommand() otherwise
 */
int my_mkdir(char *dirname)
{
    // Check if the directory name is a variable
    if (dirname[0] == '$')
    {
        // Create a buffer to store the variable name
        char varToSearch[MAX_WORD_LENGTH];
        strncpy(varToSearch, dirname + 1, sizeof(dirname) - 1);
        strcat(varToSearch, "\0");

        // Check if the variable exists in memory
        if (!isspace(variable_get_value(varToSearch)[0])) // Check if the variable exists in memory
        {
            // Get the value of the variable
            char valueFound[MAX_WORD_LENGTH];
            strcpy(valueFound, variable_get_value(varToSearch));
            strcat(valueFound, "\0");
            for (int i = 0; i < strlen(valueFound); i++) // Check if variable has any whitespaces
            {
                if (isspace(valueFound[i]))
                {
                    return badcommand(4, "Bad command: my_mkdir\n");
                }
            }
            mkdir(valueFound, 0777);
        }
        else
        {
            // Return an error if the variable does not exist
            return badcommand(4, "Bad command: my_mkdir\n");
        }
    }
    else
    {
        // Create the directory with the specified name
        mkdir(dirname, 0777);
    }
    return 0; // Return 0 once the directory has been created
}

/**
 * Function to open a file in write mode or create a
 * new file with the specified name if it doesn't exist.
 *
 * @param filename The name of the file to create
 *
 * @return 0 on success
 */
int my_touch(char *filename)
{
    FILE *file = fopen(filename, "w"); // Open file in write mode
    fclose(file);                      // Close the file
    return 0;                          // Return 0 to indicate success
}

/**
 * Function to change the current directory to the specified directory.
 *
 * @param dirname The name of the directory to change to
 *
 * @return 0 if the directory is changed successfully, badcommand() otherwise
 */
int my_cd(char *dirname)
{
    // Attempt to change the current directory to the specified directory, return an error if unsuccessful
    return chdir(dirname) == -1 ? badcommand(5, "Bad command: my_cd\n") : 0;
}