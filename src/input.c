#include "shell.h"
#include "parser.h"
#include <string.h>

char* shell_read_input(void) {
    char* input = malloc(SHELL_MAX_INPUT_LENGTH);
    if(!input) {
        fprintf(stderr, "Memory allocation during input reading failed\n");
        exit(SHELL_FAILURE);
    }

    if(fgets(input, SHELL_MAX_INPUT_LENGTH, stdin) == NULL){
        free(input);
        return NULL;
    }

    size_t len = strlen(input);
    if(len > 0 && input[len-1] == '\n'){
        input[len-1]='\0';
    }

    if(shell_validate_syntax(input) == PARSE_SYNTAX_ERROR){
        fprintf(stderr, "Invalid Syntax!\n");
        free(input);
        char* empty = malloc(1);
        if (empty) empty[0] = '\0';
        return empty;
    }

    return input;
}
