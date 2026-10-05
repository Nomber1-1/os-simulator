#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "shellmemory.h"
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

char *backStorePath = BACKING_STORE_PATH; // Path to the backing store directory

/**
 * Function to create the backing store directory if it does not exist.
 * If the directory already exists, all contents are removed.
 */
void create_backing_store()
{
    struct stat st;
    if (stat(backStorePath, &st) == -1) // Check if the directory does not exist
    {
        mkdir(backStorePath, 0700);
    }
    else
    {
        // Remove all contents if the directory already exists
        DIR *dir = opendir(backStorePath);
        struct dirent *directory;
        while ((directory = readdir(dir)) != NULL)
        {
            if (directory->d_name[0] != '.') // Skip hidden files
            {
                char filepath[273];
                snprintf(filepath, sizeof(filepath), "%s/%s", backStorePath, directory->d_name);
                remove(filepath);
            }
        }
        closedir(dir);
    }
}

/**
 * Function to delete the backing store directory
 */
void delete_backing_store()
{
    // Remove all contents of the directory
    DIR *dir = opendir(backStorePath);
    struct dirent *directory;
    while ((directory = readdir(dir)) != NULL)
    {
        if (directory->d_name[0] != '.') // Skip hidden files
        {
            char filepath[273];
            snprintf(filepath, sizeof(filepath), "%s/%s", backStorePath, directory->d_name);
            remove(filepath);
        }
    }
    closedir(dir);
    rmdir(backStorePath);
}

struct frame_store
{
    char *var;   // Variable name (key)
    char *value; // Associated value (value)
};
struct frame_store frameStore[FRAME_STORE_SIZE]; // Array to store key-value pairs of code lines in memory
struct variable_store
{
    char *var;   // Variable name (key)
    char *value; // Associated value (value)
};
struct variable_store variableStore[VARIABLE_STORE_SIZE]; // Array to store key-value pairs of variables in memory

int lru_array[FRAME_STORE_SIZE / 3]; // Array used to keep track of LRU

/**
 * Function to initialize the frame store and variable store
 * by setting all values to "none".
 */
void shell_init()
{
    // Loop through the frame store array and set all values to "none"
    for (int i = 0; i < FRAME_STORE_SIZE; i++)
    {
        frameStore[i].var = "none";   // Set variable name to "none"
        frameStore[i].value = "none"; // Set value to "none"
    }

    // Loop through the variable store array and set all values to "none"
    for (int i = 0; i < VARIABLE_STORE_SIZE; i++)
    {
        variableStore[i].var = "none";   // Set variable name to "none"
        variableStore[i].value = "none"; // Set value to "none"
    }

    // Loop through the array used to keep track of LRU and set all values to -1
    for (int i = 0; i < sizeof(lru_array); i++)
    {
        lru_array[i] = -1;
    }
}

/**
 * Function to set a code line in the frame store based on a key.
 *
 * @param var_in The key to set in memory
 * @param value_in The value to assign to the key
 */
void frame_set_value(char *var_in, char *value_in)
{
    int i;

    // Check if the value already exists in memory
    for (i = 0; i < FRAME_STORE_SIZE; i++)
    {
        // If the key already exists, update the value
        if (strcmp(frameStore[i].var, var_in) == 0)
        {
            frameStore[i].value = strdup(value_in);
            return;
        }
    }

    // Value does not exist, need to find a free spot in memory
    for (i = 0; i < FRAME_STORE_SIZE; i++)
    {
        if (strcmp(frameStore[i].var, "none") == 0)
        {
            // Set the key-value pair in the first available free slot
            frameStore[i].var = strdup(var_in);
            frameStore[i].value = strdup(value_in);
            return;
        }
    }

    return;
}

/**
 * Function to set a variable in the variable store based on a key.
 *
 * @param var_in The variable to set in memory
 * @param value_in The value to assign to the variable
 */
void variable_set_value(char *var_in, char *value_in)
{
    int i;

    // Check if the value already exists in memory
    for (i = 0; i < VARIABLE_STORE_SIZE; i++)
    {
        // If the key already exists, update the value
        if (strcmp(variableStore[i].var, var_in) == 0)
        {
            variableStore[i].value = strdup(value_in);
            return;
        }
    }

    // Value does not exist, need to find a free spot in memory
    for (i = 0; i < VARIABLE_STORE_SIZE; i++)
    {
        if (strcmp(variableStore[i].var, "none") == 0)
        {
            // Set the key-value pair in the first available free slot
            variableStore[i].var = strdup(var_in);
            variableStore[i].value = strdup(value_in);
            return;
        }
    }

    return;
}

/**
 * Function to get a code line from the frame store based on a key.
 *
 * @param var_in The key to search for in the frame store
 *
 * @return char* The value associated with the key
 */
char *frame_get_value(char *var_in)
{
    int i;

    // Loop through the frame store array to find the key
    for (i = 0; i < FRAME_STORE_SIZE; i++)
    {
        if (strcmp(frameStore[i].var, var_in) == 0)
        {
            // Return the value associated with the key
            return strdup(frameStore[i].value);
        }
    }

    // If key was not found, return an empty string
    return " ";
}

/**
 * Function to get a variable from the variable store based on a key.
 *
 * @param var_in The variable to retrieve from memory
 *
 * @return char* The value associated with the variable
 */
char *variable_get_value(char *var_in)
{
    int i;

    // Loop through the variable store array to find the key
    for (i = 0; i < VARIABLE_STORE_SIZE; i++)
    {
        if (strcmp(variableStore[i].var, var_in) == 0)
        {
            // Return the value associated with the key
            return strdup(variableStore[i].value);
        }
    }

    // If key was not found, return an empty string
    return " ";
}

/**
 * Function to increment the time since a page was last used.
 *
 * @param pcb The process control block (PCB) of the script
 */
void incrementTimeSinceLastUsed(PCB *pcb)
{
    int currentFrame = pcb->pageTable[pcb->pageCounter];

    // Increment the time since last used for all frames except the current frame
    for (int i = 0; i < sizeof(lru_array); i++)
    {
        if (lru_array[i] != -1 && i != currentFrame)
        {
            lru_array[i] += 1;
        }
    }

    lru_array[currentFrame] = 0; // Reset the time since last used for the current frame
}

/**
 * Function to get the least recently used (LRU) frame in memory.
 *
 * @param pcb The process control block (PCB) of the script
 *
 * @return int the least recently used frame
 */
int getLRUFrame(PCB *pcb)
{
    int index = 0;
    int eldest_frame = lru_array[0];

    // Find the least recently used frame in memory
    for (int i = 1; i < sizeof(lru_array); i++)
    {
        if (lru_array[i] > eldest_frame)
        {
            index = i;
            eldest_frame = lru_array[i];
        }
    }

    lru_array[index] = 0; // Reset the time since last used for the least recently used frame

    return index; // Return the least recently used frame
}