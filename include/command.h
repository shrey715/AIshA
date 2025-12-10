#ifndef COMMAND_H
#define COMMAND_H

#include "parser.h"

// Command execution structures
typedef struct {
    char** argv;
    int argc;
    char* input_file;
    char* output_file;
    int append_output;
} command_t;

typedef struct {
    command_t** commands;
    int command_count;
} pipeline_t;

// Command parsing functions
command_t* parse_command_from_tokens(const token_t* tokens, int token_count);
pipeline_t* parse_pipeline_from_tokens(const token_t* tokens, int token_count);

// Memory management functions
void free_command(command_t* cmd);
void free_pipeline(pipeline_t* pipeline);

// Helper functions
int has_pipes(const token_t* tokens, int token_count);
int validate_all_redirections(const token_t* tokens, int token_count);
int setup_redirections(command_t* cmd, int* input_fd, int* output_fd);
void cleanup_fds(int input_fd, int output_fd);

#endif
