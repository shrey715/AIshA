#include "command.h"
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

int has_pipes(const token_t* tokens, int token_count) {
    for (int i = 0; i < token_count; i++) {
        if (tokens[i].type == TOKEN_PIPE) {
            return 1;
        }
    }
    return 0;
}

int validate_all_redirections(const token_t* tokens, int token_count) {
    for (int i = 0; i < token_count; i++) {
        if (tokens[i].type == TOKEN_INPUT_REDIRECT) {
            if (i + 1 < token_count && tokens[i + 1].type == TOKEN_WORD) {
                int fd = open(tokens[i + 1].value, O_RDONLY);
                if (fd < 0) {
                    close(fd);
                    fprintf(stderr, "No such file or directory\n");
                    return SHELL_FAILURE;
                }
                close(fd);
            }
        }
    }

    for (int i = 0; i < token_count; i++) {
        if (tokens[i].type == TOKEN_OUTPUT_REDIRECT || tokens[i].type == TOKEN_OUTPUT_APPEND) {
            if (i + 1 < token_count && tokens[i + 1].type == TOKEN_WORD) {
                int flags = O_WRONLY | O_CREAT;
                if (tokens[i].type == TOKEN_OUTPUT_APPEND) {
                    flags |= O_APPEND;
                } else {
                    flags |= O_TRUNC;
                }
                int fd = open(tokens[i + 1].value, flags, 0644);
                if (fd < 0) {
                    close(fd);
                    fprintf(stderr, "Unable to create file for writing\n");
                    return SHELL_FAILURE;
                }
                close(fd);
            }
        }
    }

    return SHELL_SUCCESS;
}

int setup_redirections(command_t* cmd, int* input_fd, int* output_fd) {
    *input_fd = STDIN_FILENO;
    *output_fd = STDOUT_FILENO;

    if (cmd->input_file) {
        *input_fd = open(cmd->input_file, O_RDONLY);
        if (*input_fd < 0) {
            fprintf(stderr, "No such file or directory\n");
            return SHELL_FAILURE;
        }
    }

    if (cmd->output_file) {
        int flags = O_WRONLY | O_CREAT;
        flags |= cmd->append_output ? O_APPEND : O_TRUNC;
        
        *output_fd = open(cmd->output_file, flags, 0644);
        if (*output_fd < 0) {
            fprintf(stderr, "Unable to create file for writing\n");
            if (*input_fd != STDIN_FILENO) close(*input_fd);
            return SHELL_FAILURE;
        }
    }

    return SHELL_SUCCESS;
}

void cleanup_fds(int input_fd, int output_fd) {
    if (input_fd != STDIN_FILENO) close(input_fd);
    if (output_fd != STDOUT_FILENO) close(output_fd);
}

command_t* parse_command_from_tokens(const token_t* tokens, int token_count) {
    if (token_count == 0) return NULL;
    if (validate_all_redirections(tokens, token_count) != SHELL_SUCCESS) return NULL;

    command_t* cmd = malloc(sizeof(command_t));
    if (!cmd) return NULL;

    cmd->argv = NULL;
    cmd->argc = 0;
    cmd->input_file = NULL;
    cmd->output_file = NULL;
    cmd->append_output = 0;

    int arg_cnt = 0;
    for (int i = 0; i < token_count; i++) {
        if (tokens[i].type == TOKEN_WORD) {
            if (i > 0 && (tokens[i-1].type == TOKEN_INPUT_REDIRECT || 
                         tokens[i-1].type == TOKEN_OUTPUT_REDIRECT || 
                         tokens[i-1].type == TOKEN_OUTPUT_APPEND)) {
                continue; // Skip redirection filenames
            }
            arg_cnt++;
        }
    }

    cmd->argv = malloc((arg_cnt + 1) * sizeof(char*));
    if (!cmd->argv) {
        free(cmd);
        return NULL;
    }

    int arg_idx = 0;
    for (int i = 0; i < token_count; i++) {
        switch (tokens[i].type) {
            case TOKEN_WORD:
                if (i > 0 && (tokens[i-1].type == TOKEN_INPUT_REDIRECT || tokens[i-1].type == TOKEN_OUTPUT_REDIRECT || tokens[i-1].type == TOKEN_OUTPUT_APPEND)) {
                    continue; 
                }
                cmd->argv[arg_idx++] = strdup(tokens[i].value);
                break;
            case TOKEN_INPUT_REDIRECT:
                if (i + 1 < token_count && tokens[i + 1].type == TOKEN_WORD) {
                    if (cmd->input_file) free(cmd->input_file);
                    cmd->input_file = strdup(tokens[i + 1].value);
                }
                break;
            case TOKEN_OUTPUT_REDIRECT:
                if (i + 1 < token_count && tokens[i + 1].type == TOKEN_WORD) {
                    if (cmd->output_file) free(cmd->output_file);
                    cmd->output_file = strdup(tokens[i + 1].value);
                    cmd->append_output = 0;
                }
                break;
            case TOKEN_OUTPUT_APPEND:
                if (i + 1 < token_count && tokens[i + 1].type == TOKEN_WORD) {
                    if (cmd->output_file) free(cmd->output_file);
                    cmd->output_file = strdup(tokens[i + 1].value);
                    cmd->append_output = 1;
                }
                break;
            default:
                break;
        }
    }

    cmd->argv[arg_idx] = NULL;
    cmd->argc = arg_cnt;
    return cmd;
}

pipeline_t* parse_pipeline_from_tokens(const token_t* tokens, int token_count) {
    if (token_count == 0) return NULL;

    pipeline_t* pipeline = malloc(sizeof(pipeline_t));
    if (!pipeline) return NULL;

    pipeline->commands = NULL;
    pipeline->command_count = 0;

    int start = 0;
    int cmd_count = 0;

    for (int i = 0; i <= token_count; i++) {
        if (i == token_count || tokens[i].type == TOKEN_PIPE) {
            int segment_length = i - start;
            if (segment_length > 0) {
                command_t* cmd = parse_command_from_tokens(&tokens[start], segment_length);
                if (cmd) {
                    command_t** new_commands = realloc(pipeline->commands, (cmd_count + 1) * sizeof(command_t*));
                    if (!new_commands) {
                        free_command(cmd);
                        free_pipeline(pipeline);
                        return NULL;
                    }
                    pipeline->commands = new_commands;
                    pipeline->commands[cmd_count++] = cmd;
                }
            }
            start = i + 1;
        }
    }

    pipeline->command_count = cmd_count;
    return pipeline;
}

void free_command(command_t* cmd) {
    if (!cmd) return;

    if (cmd->argv) {
        for (int i = 0; i < cmd->argc; i++) {
            free(cmd->argv[i]);
        }
        free(cmd->argv);
    }

    free(cmd->input_file);
    free(cmd->output_file);
    free(cmd);
}

void free_pipeline(pipeline_t* pipeline) {
    if (!pipeline) return;

    if (pipeline->commands) {
        for (int i = 0; i < pipeline->command_count; i++) {
            free_command(pipeline->commands[i]);
        }
        free(pipeline->commands);
    }

    free(pipeline);
}
