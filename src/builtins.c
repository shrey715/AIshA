#include "builtins.h"
#include "background.h"
#include "log.h"
#include "directory.h"
#include "execute.h"
#include "signals.h"

static char* g_previous_directory = NULL;

const builtin_entry_t builtins[] = {
    { "hop",    builtin_hop },
    { "reveal", builtin_reveal },
    { "log",    builtin_log },
    { "activities", builtin_activities },
    { "ping",       builtin_ping },
    { "fg",        builtin_fg },
    { "bg",        builtin_bg }
};

const size_t builtins_count = sizeof(builtins)/sizeof(builtins[0]);

int is_builtin(const char* command) {
    if (!command) return -1;
    for (size_t i = 0; i < builtins_count; ++i) {
        if (strcmp(command, builtins[i].name) == 0) {
            return (int)i;
        }
    }
    return -1;
}

int builtin_hop(char** args, int argc) {
    // args[0] is the command name, args[i] is the i-th argument
    char* target_directory = NULL;

    if (argc == 1) {
        // no arguments, send to home
        target_directory = g_home_directory;
        char* previous_directory = get_current_directory();

        if (chdir(target_directory) != 0) {
            fprintf(stderr, "No such directory!");
        }
        g_previous_directory = previous_directory;
    } else {
        // ~ : change CWD to HOME
        // . : stay in same CWD
        // .. : change CWD to PARENT
        // - : change CWD to PREVIOUS
        // <name> : change CWD to <name>
        // output "No such directory!" if not found
        // all this should be executed sequentially for each passed argument
        for (int i = 1; i < argc; i++) {
            char* previous_directory = get_current_directory();

            if (strcmp(args[i], "~") == 0) {
                target_directory = g_home_directory;
            } else if (strcmp(args[i], ".") == 0) {
                target_directory = get_current_directory();
            } else if (strcmp(args[i], "..") == 0) {
                target_directory = get_parent_directory();
            } else if (strcmp(args[i], "-") == 0) {
                target_directory = g_previous_directory;
            } else {
                target_directory = args[i];
            }

            if (chdir(target_directory) != 0) {
                fprintf(stderr, "No such directory!\n");
            } else {
                if (g_previous_directory) {
                    free(g_previous_directory);
                }
                g_previous_directory = previous_directory;
            }
        }
    }
    return 0;
}

void cleanup_hop(void) {
    // Clean up resources allocated by the hop command
    if (g_previous_directory) {
        free(g_previous_directory);
        g_previous_directory = NULL;
    }
}

static int compare_entries(const void* a, const void* b) {
    return strcmp(*(const char**)a, *(const char**)b);
}

int builtin_reveal(char** args, int argc) {
    // args[0] is the command name, args[i] is the i-th argument
    int show_all = 0;
    int list_format = 0;
    char* target_directory = ".";
    int arg_count = 0;
    
    for (int i = 1; i < argc; i++) {
        if (args[i][0] == '-') {
            for (size_t j = 1; args[i][j] != '\0'; j++) {
                if (args[i][j] == 'a') {
                    show_all = 1;
                } else if (args[i][j] == 'l') {
                    list_format = 1;
                } else {
                    fprintf(stderr, "reveal: Invalid flag -%c\n", args[i][j]);
                    return 1;
                }
            }
        } else {
            arg_count++;
            if (arg_count > 1) {
                fprintf(stderr, "reveal: Invalid Syntax!\n");
                return 1;
            }
            target_directory = args[i];
        }
    }

    char* resolved_path = NULL;
    if (strcmp(target_directory, "~") == 0) {
        resolved_path = strdup(g_home_directory);
    } else if (strcmp(target_directory, ".") == 0) {
        resolved_path = get_current_directory();
    } else if (strcmp(target_directory, "..") == 0) {
        resolved_path = get_parent_directory();
    } else if (strcmp(target_directory, "-") == 0) {
        if (!g_previous_directory) {
            fprintf(stderr, "No such directory!\n");
            return 1;
        }
        resolved_path = strdup(g_previous_directory);
    } else {
        resolved_path = strdup(target_directory);
    }
    
    if (!resolved_path) {
        fprintf(stderr, "No such directory!\n");
        return 1;
    }

    DIR* dir = opendir(resolved_path);
    if (!dir) {
        if (errno == ENOENT) {
            fprintf(stderr, "No such directory!\n");
        }
        free(resolved_path);
        return 1;
    }

    struct dirent* entry;
    int entry_cnt = 0;

    // first pass to count entries
    while ((entry = readdir(dir)) != NULL) {
        if (!show_all && entry->d_name[0] == '.') {
            continue;
        }
        entry_cnt++;
    }

    char** entries = malloc(entry_cnt * sizeof(char*));
    if (!entries) {
        fprintf(stderr, "Memory allocation failed!\n");
        closedir(dir);
        free(resolved_path);
        return 1;
    }

    rewinddir(dir);
    int idx = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (!show_all && entry->d_name[0] == '.') {
            continue;
        }
        entries[idx++] = strdup(entry->d_name);
    }

    qsort(entries, entry_cnt, sizeof(char*), compare_entries);

    for (int i = 0; i < entry_cnt; i++) {
        if (list_format) {
            printf("%s\n", entries[i]);
        } else {
            printf("%s ", entries[i]);
        }
        free(entries[i]);
    }

    if (!list_format) {
        printf("\n");
    }

    free(entries);
    closedir(dir);
    free(resolved_path);
    return 0;
}

int builtin_log(char** args, int argc) {
    // args[0] is the command name, args[i] is the i-th argument
    if (argc == 1) {
        int start = g_command_log.count < LOG_MAX_ENTRIES ? 0 : g_command_log.head;
        int entries = g_command_log.count < LOG_MAX_ENTRIES ? g_command_log.count : LOG_MAX_ENTRIES;
        
        for (int i = 0; i < entries; i++) {
            int idx = (start + i) % LOG_MAX_ENTRIES;
            printf("%s\n", g_command_log.commands[idx]);
        }
    } else if (argc == 2 && strcmp(args[1], "purge") == 0) {
        g_command_log.count = 0;
        g_command_log.head = 0;
        log_save_history();
    } else if (argc == 3 && strcmp(args[1], "execute") == 0) {
        int index = atoi(args[2]);
        if (index < 1 || index > g_command_log.count) {
            return 1;
        }
        
        int newest_idx = g_command_log.count < LOG_MAX_ENTRIES ? g_command_log.count - 1 : (g_command_log.head - 1 + LOG_MAX_ENTRIES) % LOG_MAX_ENTRIES;
        
        int target_idx = (newest_idx - index + 1 + LOG_MAX_ENTRIES) % LOG_MAX_ENTRIES;
        
        char* command_to_execute = g_command_log.commands[target_idx];
        
        token_t tokens[MAX_TOKENS];
        int token_count = tokenize_input(command_to_execute, tokens, MAX_TOKENS);
        if (token_count > 0) {
            execute_shell_command_with_operators(tokens, token_count);
        }
    } else {
        fprintf(stderr, "log: Invalid Syntax!\n");
        return 1;
    }
    
    return 0;
}

int builtin_activities(char** args, int argc) {
    if (argc != 1) {
        fprintf(stderr, "activities: Invalid Syntax!\n");
        return 1;
    }

    list_activities();
    return 0;
}

int builtin_ping(char** args, int argc) {
    if (argc != 3) {
        fprintf(stderr, "ping: Invalid Syntax!\n");
        return 1;
    }

    pid_t pid = (pid_t)atoi(args[1]);
    int signal = atoi(args[2]);
    int res = ping_process(pid, signal%32);

    if (res == 0) {
        printf("Sent signal %d to process with pid %d\n", signal, pid);
    } else if (res == JOB_NOT_FOUND) {
        fprintf(stderr, "No such process found\n");
    } else {
        fprintf(stderr, "Invalid syntax\n");
    }

    return 0;
}

int builtin_fg(char** args, int argc) {
    background_job_t* job = NULL;
    
    if (argc == 1) {
        fprintf(stderr, "fg: Invalid Syntax!\n");
        return 1;
    } else if (argc == 2) {
        char* endptr;
        long job_id = strtol(args[1], &endptr, 10);
        
        if (*endptr != '\0' || endptr == args[1] || job_id <= 0) {
            fprintf(stderr, "No such job\n");
            return 1;
        }
        
        job = find_job_by_id((int)job_id);
        if (!job) {
            fprintf(stderr, "No such job\n");
            return 1;
        }
    } else {
        fprintf(stderr, "fg: Invalid Syntax!\n");
        return 1;
    }
    
    printf("%s\n", job->command);
    
    if (job->status == PROCESS_STOPPED) {
        if (kill(job->pid, SIGCONT) == -1) {
            if (errno == ESRCH) {
                fprintf(stderr, "No such job\n");
                remove_job_by_pid(job->pid);
                return 1;
            }
            fprintf(stderr, "Failed to continue job\n");
            return 1;
        }
    }
    
    g_foreground_pid = job->pid;
    remove_job_by_pid(job->pid);
    
    int status;
    if (waitpid(job->pid, &status, WUNTRACED) == -1) {
        fprintf(stderr, "waitpid failed\n");
        g_foreground_pid = -1;
        return 1;
    }
    
    g_foreground_pid = -1;
    
    if (WIFSTOPPED(status)) {
        int jid = add_background_job(job->pid, job->command, PROCESS_STOPPED);
        printf("\n[%d] Stopped %s\n", jid, job->command);
    }
    
    return 0;
}

int builtin_bg(char** args, int argc) {
    background_job_t* job = NULL;
    
    if (argc == 1) {
        fprintf(stderr, "bg: Invalid Syntax!\n");
        return 1;
    } else if (argc == 2) {
        char* endptr;
        long job_id = strtol(args[1], &endptr, 10);
        
        if (*endptr != '\0' || endptr == args[1] || job_id <= 0) {
            fprintf(stderr, "No such job\n");
            return 1;
        }
        
        job = find_job_by_id((int)job_id);
        if (!job) {
            fprintf(stderr, "No such job\n");
            return 1;
        }
    } else {
        fprintf(stderr, "bg: Invalid Syntax!\n");
        return 1;
    }
    
    if (job->status == PROCESS_RUNNING) {
        fprintf(stderr, "Job already running\n");
        return 1;
    }
    
    if (job->status != PROCESS_STOPPED) {
        fprintf(stderr, "Job already running\n");
        return 1;
    }
    
    if (kill(job->pid, SIGCONT) == -1) {
        if (errno == ESRCH) {
            fprintf(stderr, "No such job\n");
            remove_job_by_pid(job->pid);
            return 1;
        }
        fprintf(stderr, "Failed to continue job\n");
        return 1;
    }
    
    job->status = PROCESS_RUNNING;
    printf("[%d] %s\n", job->job_id, job->command);
        
    return 0;
}