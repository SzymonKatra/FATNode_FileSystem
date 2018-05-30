#include "parser.h"

#include <string.h>

size_t parser_parse(char* input, char** output, size_t max_outputs)
{
    size_t len = strlen(input);
    if (input[len - 1] == '\n') input[len - 1] = 0;
    
    size_t current = 0;
    
    char* token = strtok(input, " ");
    while (token != NULL && current < max_outputs)
    {
        output[current++] = token;
        token = strtok(NULL, " ");
    }
    
    //if (current == 0)
    //{
    //    output[current++] = input;
    //}
    
    return current;
}