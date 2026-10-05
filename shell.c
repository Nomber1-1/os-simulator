#include <stdio.h>
#include <stdlib.h>
#include <ctype.h> // isspace
#include <string.h>
#include <unistd.h> // isatty
#include "shell.h"
#include "interpreter.h"
#include "shellmemory.h"

int parseInput(char ui[]);

/**
 * Function to check the entire program
 *
 * @param argc The number of arguments
 * @param argv The arguments
 *
 * @return 0, if the program runs successfully
 *
 */
int main(int argc, char *argv[])
{

    char prompt = '$';              // Shell prompt
    char userInput[MAX_USER_INPUT]; // user's input stored here
    int batch_mode = !isatty(STDIN_FILENO);
    int errorCode = 0; // zero means no error, default

    // init user input
    for (int i = 0; i < MAX_USER_INPUT; i++)
    {
        userInput[i] = '\0';
    }

    // init shell memory
    shell_init();
    create_backing_store();

    printf("Frame Store Size = %d; Variable Store Size = %d\n", FRAME_STORE_SIZE, VARIABLE_STORE_SIZE);

    while (1)
    {
        if (!batch_mode)
        {
            printf("%c ", prompt);
        }

        fgets(userInput, MAX_USER_INPUT - 1, stdin);
        errorCode = parseInput(userInput);
        if (errorCode == -1)
            exit(99); // ignore all other errors

        if (feof(stdin))
        {
            stdin = fopen("/dev/tty", "r");
            exit(0); // To avoid segmentation fault in autograder
        }

        memset(userInput, 0, sizeof(userInput));
    }

    return 0;
}
/**
 * Function to check the end of a word
 *
 * @param c The character to check
 *
 * @return 1 if the character is the end of a word, 0 otherwise
 */
int wordEnding(char c)
{
    return c == '\0' || c == '\n' || isspace(c) || c == ';';
}
/**
 * Function to parse the user input
 *
 * @param inp The user input
 *
 * @return 0 if the input is parsed successfully
 */
int parseInput(char inp[])
{
    char tmp[200], *words[100];
    int ix = 0, w = 0;
    int wordlen;
    int errorCode = 0;

    while (inp[ix] != '\n' && inp[ix] != '\0' && ix < 1000)
    {
        // skip white spaces
        for (; isspace(inp[ix]) && inp[ix] != '\n' && ix < 1000; ix++)
            ;

        if (inp[ix] == ';')
            break;

        // extract a word
        for (wordlen = 0; !wordEnding(inp[ix]) && ix < 1000; ix++, wordlen++)
        {
            tmp[wordlen] = inp[ix];
        }

        if (wordlen > 0)
        {
            tmp[wordlen] = '\0';
            words[w] = strdup(tmp);
            w++;
            if (inp[ix] == '\0')
                break;
        }
        else
        {
            break;
        }
    }

    errorCode = interpreter(words, w);
    for (size_t i = 0; i < w; ++i)
    {
        free(words[i]);
    }

    if (inp[ix] == ';')
    {
        return parseInput(&inp[ix + 1]);
    }
    return errorCode;
}