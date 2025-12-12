#include "execute.h"
#include "builtins.h"
#include "background.h"
#include "command.h"
#include "signals.h"
#include "variables.h"
#include "glob.h"
#include "colors.h"
#include <errno.h>
#include <string.h>

/* Check if tokens contain && or || */
int has_and_or(const token_t* tokens, int token_count) {
    for (int i = 0; i < token_count; i++) {
        if (tokens[i].type == TOKEN_AND || tokens[i].type == TOKEN_OR) {
            return 1;
        }
    }
    return 0;
}

/* Execute a pipeline of commands */
int execute_pipeline(pipeline_t* pipeline) {
    if (!pipeline || pipeline->command_count == 0) return SHELL_FAILURE;

    if (pipeline->command_count == 1) {
        return execute_single_command(pipeline->commands[0]);
    }

    int pipe_fds[pipeline->command_count - 1][2];
    pid_t pids[pipeline->command_count];
    memset(pids, 0, sizeof(pids));

    /* Create all pipes */
    for (int i = 0; i < pipeline->command_count - 1; i++) {
        if (pipe(pipe_fds[i]) < 0) {
            print_error("pipe: %s\n", strerror(errno));
            for (int j = 0; j < i; j++) {
                close(pipe_fds[j][0]);
                close(pipe_fds[j][1]);
            }
            return SHELL_FAILURE;
        }
    }

    /* Fork and execute each command */
    for (int i = 0; i < pipeline->command_count; i++) {
        command_t* cmd = pipeline->commands[i];
        
        pids[i] = fork();
        if (pids[i] < 0) {
            print_error("fork: %s\n", strerror(errno));
            for (int j = 0; j < pipeline->command_count - 1; j++) {
                close(pipe_fds[j][0]);
                close(pipe_fds[j][1]);
            }
            for (int j = 0; j < i; j++) {
                kill(pids[j], SIGTERM);
            }
            return SHELL_FAILURE;
        } 
        
        if (pids[i] == 0) { /* Child process */
            /* Set up input */
            if (i == 0) {
                if (cmd->input_file) {
                    int input_fd = open(cmd->input_file, O_RDONLY);
                    if (input_fd < 0) {
                        print_error("%s: %s\n", cmd->input_file, strerror(errno));
                        exit(SHELL_FAILURE);
                    }
                    dup2(input_fd, STDIN_FILENO);
                    close(input_fd);
                }
            } else {
                dup2(pipe_fds[i-1][0], STDIN_FILENO);
            }

            /* Set up output */
            if (i == pipeline->command_count - 1) {
                if (cmd->output_file) {
                    int flags = O_WRONLY | O_CREAT;
                    flags |= cmd->append_output ? O_APPEND : O_TRUNC;
                    
                    int output_fd = open(cmd->output_file, flags, 0644);
                    if (output_fd < 0) {
                        print_error("%s: %s\n", cmd->output_file, strerror(errno));
                        exit(SHELL_FAILURE);
                    }
                    dup2(output_fd, STDOUT_FILENO);
                    close(output_fd);
                }
            } else {
                dup2(pipe_fds[i][1], STDOUT_FILENO);
            }
            
            /* Close all pipe fds in child */
            for (int j = 0; j < pipeline->command_count - 1; j++) {
                close(pipe_fds[j][0]);
                close(pipe_fds[j][1]);
            }
            
            /* Execute */
            int builtin_idx = is_builtin(cmd->argv[0]);
            if (builtin_idx >= 0) {
                exit(builtins[builtin_idx].func(cmd->argv, cmd->argc));
            } else {
                execvp(cmd->argv[0], cmd->argv);
                print_error("%s: command not found\n", cmd->argv[0]);
                exit(127);
            }
        }
    }
    
    /* Parent: close all pipes */
    for (int i = 0; i < pipeline->command_count - 1; i++) {
        close(pipe_fds[i][0]);
        close(pipe_fds[i][1]);
    }

    g_foreground_pid = pids[pipeline->command_count - 1];
    
    int exit_status = SHELL_SUCCESS;
    for (int i = 0; i < pipeline->command_count; i++) {
        int status;
        if (waitpid(pids[i], &status, WUNTRACED) < 0) {
            print_error("waitpid: %s\n", strerror(errno));
            exit_status = SHELL_FAILURE;
        } else if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            exit_status = WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            exit_status = 128 + WTERMSIG(status);
        }
    }
    
    g_foreground_pid = -1;
    update_exit_status(exit_status);
    return exit_status;
}

/* Execute a single command */
int execute_single_command(command_t* cmd) {
    if (!cmd || !cmd->argv[0]) return SHELL_FAILURE;

    int input_fd, output_fd;
    if (setup_redirections(cmd, &input_fd, &output_fd) != SHELL_SUCCESS) {
        return SHELL_FAILURE;
    }

    /* Check for variable assignment (VAR=value with no command) */
    if (cmd->argc == 1 && strchr(cmd->argv[0], '=') != NULL) {
        char* eq = strchr(cmd->argv[0], '=');
        if (eq > cmd->argv[0]) {
            *eq = '\0';
            set_variable(cmd->argv[0], eq + 1, 0);
            *eq = '=';
            cleanup_fds(input_fd, output_fd);
            update_exit_status(0);
            return SHELL_SUCCESS;
        }
    }

    /* Check for builtin */
    int builtin_idx = is_builtin(cmd->argv[0]);
    if (builtin_idx >= 0) {
        int saved_stdin = dup(STDIN_FILENO);
        int saved_stdout = dup(STDOUT_FILENO);
        
        if (saved_stdin < 0 || saved_stdout < 0) {
            print_error("dup: %s\n", strerror(errno));
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

        update_exit_status(result);
        return result;
    }

    /* External command */
    pid_t pid = fork();
    if (pid < 0) {
        print_error("fork: %s\n", strerror(errno));
        cleanup_fds(input_fd, output_fd);
        return SHELL_FAILURE;
    } 
    
    if (pid == 0) { /* Child process */
        /* Reset signals */
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
        print_error("%s: command not found\n", cmd->argv[0]);
        exit(127);
    } 
    
    /* Parent process */
    cleanup_fds(input_fd, output_fd);

    g_foreground_pid = pid;

    int status;
    if (waitpid(pid, &status, WUNTRACED) < 0) {
        print_error("waitpid: %s\n", strerror(errno));
        g_foreground_pid = -1;
        return SHELL_FAILURE;
    }

    g_foreground_pid = -1;

    if (WIFSTOPPED(status)) {
        char command_str[SHELL_MAX_INPUT_LENGTH] = {0};
        for (int i = 0; i < cmd->argc; i++) {
            if (i > 0) strcat(command_str, " ");
            strcat(command_str, cmd->argv[i]);
        }
        int jid = add_background_job(pid, command_str, PROCESS_STOPPED);
        printf("\n[%d] Stopped                 %s\n", jid, command_str);
        update_exit_status(148); /* 128 + SIGTSTP(20) ish */
        return SHELL_SUCCESS;
    }

    int exit_status;
    if (WIFEXITED(status)) {
        exit_status = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        exit_status = 128 + WTERMSIG(status);
    } else {
        exit_status = SHELL_FAILURE;
    }
    
    update_exit_status(exit_status);
    return exit_status;
}

/* Execute a simple command from tokens */
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

/* Execute an && or || list */
int execute_and_or_list(const token_t* tokens, int token_count) {
    int start = 0;
    int last_result = SHELL_SUCCESS;
    
    for (int i = 0; i <= token_count; i++) {
        int is_end = (i == token_count || tokens[i].type == TOKEN_EOF);
        int is_and = (!is_end && tokens[i].type == TOKEN_AND);
        int is_or = (!is_end && tokens[i].type == TOKEN_OR);
        
        if (is_end || is_and || is_or) {
            int segment_length = i - start;
            
            if (segment_length > 0) {
                /* Execute this segment */
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
            
            if (is_end) break;
            
            /* Handle && and || short-circuit logic */
            if (is_and && last_result != 0) {
                /* && failed, skip until || or end */
                i++;
                while (i < token_count && tokens[i].type != TOKEN_OR && 
                       tokens[i].type != TOKEN_SEMICOLON && 
                       tokens[i].type != TOKEN_AMPERSAND) {
                    i++;
                }
                if (i < token_count && tokens[i].type == TOKEN_OR) {
                    start = i + 1;
                    continue;
                }
            } else if (is_or && last_result == 0) {
                /* || succeeded, skip until && or end */
                i++;
                while (i < token_count && tokens[i].type != TOKEN_AND &&
                       tokens[i].type != TOKEN_SEMICOLON && 
                       tokens[i].type != TOKEN_AMPERSAND) {
                    i++;
                }
                if (i < token_count && tokens[i].type == TOKEN_AND) {
                    start = i + 1;
                    continue;
                }
            }
            
            start = i + 1;
        }
    }
    
    return last_result;
}

/* Execute sequential commands with ; and & */
int execute_sequential_commands(const token_t* tokens, int token_count) {
    int start = 0;
    int last_result = SHELL_SUCCESS;
    
    for (int i = 0; i <= token_count; i++) {
        int is_end = (i == token_count || tokens[i].type == TOKEN_EOF);
        int is_semi = (!is_end && tokens[i].type == TOKEN_SEMICOLON);
        int is_bg = (!is_end && tokens[i].type == TOKEN_AMPERSAND);
        
        if (is_end || is_semi || is_bg) {
            int segment_length = i - start;
            
            if (segment_length > 0) {
                if (is_bg) {
                    last_result = execute_background_command(&tokens[start], segment_length);
                } else if (has_and_or(&tokens[start], segment_length)) {
                    last_result = execute_and_or_list(&tokens[start], segment_length);
                } else if (has_pipes(&tokens[start], segment_length)) {
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
            
            start = i + 1;
        }
    }
    
    return last_result;
}

/* Execute a background command */
int execute_background_command(const token_t* tokens, int token_count) {
    char command_str[SHELL_MAX_INPUT_LENGTH] = {0};
    for (int i = 0; i < token_count; i++) {
        if (i > 0) strcat(command_str, " ");
        strcat(command_str, tokens[i].value);
    }
    
    pid_t pid = fork();
    if (pid < 0) {
        print_error("fork: %s\n", strerror(errno));
        return SHELL_FAILURE;
    }
    
    if (pid == 0) {
        /* Child: redirect stdin to /dev/null */
        int dev_null = open("/dev/null", O_RDONLY);
        if (dev_null >= 0) {
            dup2(dev_null, STDIN_FILENO);
            close(dev_null);
        }
        
        /* Reset signals */
        signal(SIGINT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        
        int result;
        if (has_and_or(tokens, token_count)) {
            result = execute_and_or_list(tokens, token_count);
        } else if (has_pipes(tokens, token_count)) {
            pipeline_t* pipeline = parse_pipeline_from_tokens(tokens, token_count);
            if (pipeline) {
                result = execute_pipeline(pipeline);
                free_pipeline(pipeline);
            } else {
                result = SHELL_FAILURE;
            }
        } else {
            command_t* cmd = parse_command_from_tokens(tokens, token_count);
            if (cmd) {
                result = execute_single_command(cmd);
                free_command(cmd);
            } else {
                result = SHELL_FAILURE;
            }
        }
        exit(result);
    }
    
    /* Parent */
    add_background_job(pid, command_str, PROCESS_RUNNING);
    update_last_background_pid(pid);
    return SHELL_SUCCESS;
}

/* Execute subshell (commands in parentheses) */
int execute_subshell(const token_t* tokens, int token_count) {
    pid_t pid = fork();
    if (pid < 0) {
        print_error("fork: %s\n", strerror(errno));
        return SHELL_FAILURE;
    }
    
    if (pid == 0) {
        /* Child: execute the commands */
        int result = execute_shell_command_with_operators(tokens, token_count);
        exit(result);
    }
    
    /* Parent: wait for subshell */
    int status;
    if (waitpid(pid, &status, 0) < 0) {
        print_error("waitpid: %s\n", strerror(errno));
        return SHELL_FAILURE;
    }
    
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return SHELL_FAILURE;
}

/* Main entry point for command execution */
int execute_shell_command_with_operators(const token_t* tokens, int token_count) {
    if (!tokens || token_count == 0) return SHELL_FAILURE;
    
    /* Check for various operators and dispatch appropriately */
    if (has_sequential_or_background(tokens, token_count)) {
        return execute_sequential_commands(tokens, token_count);
    } else if (has_and_or(tokens, token_count)) {
        return execute_and_or_list(tokens, token_count);
    } else {
        return execute_shell_command(tokens, token_count);
    }
}
