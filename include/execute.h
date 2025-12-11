#ifndef EXECUTE_H
#define EXECUTE_H

#include "shell.h"
#include "parser.h"
#include "command.h"
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

/* Core execution functions */
int execute_shell_command(const token_t* tokens, int token_count);
int execute_single_command(command_t* cmd);
int execute_pipeline(pipeline_t* pipeline);

/* Sequential and background execution */
int execute_shell_command_with_operators(const token_t* tokens, int token_count);
int execute_sequential_commands(const token_t* tokens, int token_count);
int execute_background_command(const token_t* tokens, int token_count);

/* Logical operators */
int execute_and_or_list(const token_t* tokens, int token_count);

/* Helper functions */
int has_sequential_or_background(const token_t* tokens, int token_count);
int has_and_or(const token_t* tokens, int token_count);

/* Subshell execution */
int execute_subshell(const token_t* tokens, int token_count);

#endif /* EXECUTE_H */
