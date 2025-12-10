#include "execute.h"
#include "builtins.h"
#include "background.h"
#include "command.h"
#include "signals.h"

int execute_pipeline(pipeline_t* pipeline) {
    if (!pipeline || pipeline->command_count == 0) return SHELL_FAILURE;

    if (pipeline->command_count == 1) {
        return execute_single_command(pipeline->commands[0]);
    }

    int pipe_fds[pipeline->command_count - 1][2];
    pid_t pids[pipeline->command_count];

    for (int i = 0; i < pipeline->command_count - 1; i++) {
        if (pipe(pipe_fds[i]) < 0) {
            fprintf(stderr, "Pipe creation failed: %s\n", strerror(errno));
            for (int j = 0; j < i; j++) {
                close(pipe_fds[j][0]);
                close(pipe_fds[j][1]);
            }
            return SHELL_FAILURE;
        }
    }

    for (int i = 0; i < pipeline->command_count; i++) {
        command_t* cmd = pipeline->commands[i];
        
        pids[i] = fork();
        if (pids[i] < 0) {
            fprintf(stderr, "Fork failed: %s\n", strerror(errno));
            for (int j = 0; j < pipeline->command_count - 1; j++) {
                close(pipe_fds[j][0]);
                close(pipe_fds[j][1]);
            }
            for (int j = 0; j < i; j++) {
                kill(pids[j], SIGTERM);
            }
            return SHELL_FAILURE;
        } 
        
        if (pids[i] == 0) { // Child process
            if (i == 0) { // first command
                if (cmd->input_file) {
                    int input_fd = open(cmd->input_file, O_RDONLY);
                    if (input_fd < 0) {
                        fprintf(stderr, "Error opening input file %s: %s\n", cmd->input_file, strerror(errno));
                        exit(SHELL_FAILURE);
                    }
                    dup2(input_fd, STDIN_FILENO);
                    close(input_fd);
                }
            } else { // connect to previous pipe
                dup2(pipe_fds[i-1][0], STDIN_FILENO);
            }

            if (i == pipeline->command_count - 1) { // last command
                if (cmd->output_file) {
                    int flags = O_WRONLY | O_CREAT;
                    flags |= cmd->append_output ? O_APPEND : O_TRUNC;
                    
                    int output_fd = open(cmd->output_file, flags, 0644);
                    if (output_fd < 0) {
                        fprintf(stderr, "Error opening output file %s: %s\n", cmd->output_file, strerror(errno));
                        exit(SHELL_FAILURE);
                    }
                    dup2(output_fd, STDOUT_FILENO);
                    close(output_fd);
                }
            } else { // connect to next pipe
                dup2(pipe_fds[i][1], STDOUT_FILENO);
            }
            
            for (int j = 0; j < pipeline->command_count - 1; j++) {
                close(pipe_fds[j][0]);
                close(pipe_fds[j][1]);
            }
            
            int builtin_idx = is_builtin(cmd->argv[0]);
            if (builtin_idx >= 0) {
                exit(builtins[builtin_idx].func(cmd->argv, cmd->argc));
            } else {
                execvp(cmd->argv[0], cmd->argv);
                fprintf(stderr, "Command not found!\n");
                exit(SHELL_FAILURE);
            }
        }
    }
    
    // Parent process - close all pipes
    for (int i = 0; i < pipeline->command_count - 1; i++) {
        close(pipe_fds[i][0]);
        close(pipe_fds[i][1]);
    }

    g_foreground_pid = pids[pipeline->command_count - 1];
    
    int exit_status = SHELL_SUCCESS;
    for (int i = 0; i < pipeline->command_count; i++) {
        int status;
        if (waitpid(pids[i], &status, WUNTRACED) < 0) {
            fprintf(stderr, "Error waiting for child process: %s\n", strerror(errno));
            exit_status = SHELL_FAILURE;
        } else if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            exit_status = WEXITSTATUS(status);
        }
    }
    
    g_foreground_pid = -1;
    return exit_status;
}

int execute_single_command(command_t* cmd) {
    if (!cmd || !cmd->argv[0]) return SHELL_FAILURE;

    int input_fd, output_fd;
    if (setup_redirections(cmd, &input_fd, &output_fd) != SHELL_SUCCESS) {
        return SHELL_FAILURE;
    }

    int builtin_idx = is_builtin(cmd->argv[0]);
    if (builtin_idx >= 0) {
        int saved_stdin = dup(STDIN_FILENO);
        int saved_stdout = dup(STDOUT_FILENO);
        
        if (saved_stdin < 0 || saved_stdout < 0) {
            fprintf(stderr, "Failed to save stdin/stdout: %s\n", strerror(errno));
            cleanup_fds(input_fd, output_fd);
            if (saved_stdin >= 0) close(saved_stdin);
            if (saved_stdout >= 0) close(saved_stdout);
            return SHELL_FAILURE;
        }

        if (input_fd != STDIN_FILENO) {
            dup2(input_fd, STDIN_FILENO);
            close(input_fd);
        }
        if (output_fd != STDOUT_FILENO) {
            dup2(output_fd, STDOUT_FILENO);
            close(output_fd);
        }

        int result = builtins[builtin_idx].func(cmd->argv, cmd->argc);

        dup2(saved_stdin, STDIN_FILENO);
        dup2(saved_stdout, STDOUT_FILENO);
        close(saved_stdin);
        close(saved_stdout);

        return result;
    }

    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "Fork failed: %s\n", strerror(errno));
        cleanup_fds(input_fd, output_fd);
        return SHELL_FAILURE;
    } 
    
    if (pid == 0) { // Child process
        signal(SIGINT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);

        if (input_fd != STDIN_FILENO) {
            dup2(input_fd, STDIN_FILENO);
            close(input_fd);
        }
        if (output_fd != STDOUT_FILENO) {
            dup2(output_fd, STDOUT_FILENO);
            close(output_fd);
        }

        execvp(cmd->argv[0], cmd->argv);
        fprintf(stderr, "Command not found!\n");
        exit(SHELL_FAILURE);
    } 
    
    // Parent process
    cleanup_fds(input_fd, output_fd);

    g_foreground_pid = pid;

    int status;
    if (waitpid(pid, &status, WUNTRACED) < 0) {
        fprintf(stderr, "Error waiting for child process: %s\n", strerror(errno));
        g_foreground_pid = -1;
        return SHELL_FAILURE;
    }

    g_foreground_pid = -1;

    if (WIFSTOPPED(status)) { // ctrl+Z, sigstop
        char command_str[SHELL_MAX_INPUT_LENGTH] = {0};
        for (int i = 0; i < cmd->argc; i++) {
            if (i > 0) strcat(command_str, " ");
            strcat(command_str, cmd->argv[i]);
        }
        int jid = add_background_job(pid, command_str, PROCESS_STOPPED);
        printf("[%d] Stopped %s\n", jid, command_str);
        return SHELL_SUCCESS;
    }

    return WIFEXITED(status) ? WEXITSTATUS(status) : SHELL_FAILURE;
}

int execute_shell_command(const token_t* tokens, int token_count) {
    if (!tokens || token_count == 0) return SHELL_FAILURE;

    if (has_pipes(tokens, token_count)) {
        pipeline_t* pipeline = parse_pipeline_from_tokens(tokens, token_count);
        if (!pipeline) return SHELL_FAILURE;

        int result = execute_pipeline(pipeline);
        free_pipeline(pipeline);
        return result;
    } else {
        command_t* cmd = parse_command_from_tokens(tokens, token_count);
        if (!cmd) return SHELL_FAILURE;

        int result = execute_single_command(cmd);
        free_command(cmd);
        return result;
    }
}

int has_sequential_or_background(const token_t* tokens, int token_count) {
    for (int i = 0; i < token_count; i++) {
        if (tokens[i].type == TOKEN_SEMICOLON || tokens[i].type == TOKEN_AMPERSAND) {
            return 1;
        }
    }
    return 0;
}

int execute_sequential_commands(const token_t* tokens, int token_count) {
    int start = 0;
    int last_result = SHELL_SUCCESS;
    
    for (int i = 0; i <= token_count; i++) {
        if (i == token_count || tokens[i].type == TOKEN_SEMICOLON || tokens[i].type == TOKEN_AMPERSAND) {
            int segment_length = i - start;
            
            if (segment_length > 0) {
                if (i < token_count && tokens[i].type == TOKEN_AMPERSAND) {
                    // Background execution
                    last_result = execute_background_command(&tokens[start], segment_length);
                } else {
                    // Foreground execution
                    if (has_pipes(&tokens[start], segment_length)) {
                        pipeline_t* pipeline = parse_pipeline_from_tokens(&tokens[start], segment_length);
                        if (pipeline) {
                            last_result = execute_pipeline(pipeline);
                            free_pipeline(pipeline);
                        }
                    } else {
                        command_t* cmd = parse_command_from_tokens(&tokens[start], segment_length);
                        if (cmd) {
                            last_result = execute_single_command(cmd);
                            free_command(cmd);
                        }
                    }
                }
            }
            
            start = i + 1;
        }
    }
    
    return last_result;
}

int execute_background_command(const token_t* tokens, int token_count) {
    // Create command string for job tracking
    char command_str[SHELL_MAX_INPUT_LENGTH] = {0};
    for (int i = 0; i < token_count; i++) {
        if (i > 0) strcat(command_str, " ");
        strcat(command_str, tokens[i].value);
    }
    
    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "Fork failed: %s\n", strerror(errno));
        return SHELL_FAILURE;
    }
    
    if (pid == 0) {
        // Child process - redirect stdin to /dev/null for background processes
        int dev_null = open("/dev/null", O_RDONLY);
        if (dev_null >= 0) {
            dup2(dev_null, STDIN_FILENO);
            close(dev_null);
        }
        
        // Execute the command
        if (has_pipes(tokens, token_count)) {
            pipeline_t* pipeline = parse_pipeline_from_tokens(tokens, token_count);
            if (pipeline) {
                int result = execute_pipeline(pipeline);
                free_pipeline(pipeline);
                exit(result);
            }
        } else {
            command_t* cmd = parse_command_from_tokens(tokens, token_count);
            if (cmd) {
                int result = execute_single_command(cmd);
                free_command(cmd);
                exit(result);
            }
        }
        exit(SHELL_FAILURE);
    }
    
    // Parent process - add to background jobs
    add_background_job(pid, command_str, PROCESS_RUNNING);
    return SHELL_SUCCESS;
}

int execute_shell_command_with_operators(const token_t* tokens, int token_count) {
    if (!tokens || token_count == 0) return SHELL_FAILURE;
    
    if (has_sequential_or_background(tokens, token_count)) {
        return execute_sequential_commands(tokens, token_count);
    } else {
        return execute_shell_command(tokens, token_count);
    }
}
