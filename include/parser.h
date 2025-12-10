#ifndef PARSER_H
#define PARSER_H

#include "shell.h"
#include <ctype.h>

#define MAX_TOKENS 1024
#define MAX_TOKEN_LENGTH 1024

typedef enum {
    TOKEN_WORD,
    TOKEN_PIPE, // |
    TOKEN_SEMICOLON, // ;
    TOKEN_AMPERSAND, // &
    TOKEN_INPUT_REDIRECT, // <
    TOKEN_OUTPUT_REDIRECT, // >
    TOKEN_OUTPUT_APPEND, // >>
    TOKEN_EOF
} token_type_t;

typedef struct {
    token_type_t type;
    char value[MAX_TOKEN_LENGTH];
} token_t;

typedef enum {
    PARSE_SUCCESS = 0,
    PARSE_SYNTAX_ERROR = 1,
    PARSE_TOO_MANY_TOKENS = 2
} parse_result_t;

// Tokenizer functions
int tokenize_input(const char* input, token_t tokens[], int max_tokens);

// Validation functions
int shell_validate_syntax(const char* input);

#endif
