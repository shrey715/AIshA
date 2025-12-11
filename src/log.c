#include "log.h"
#include "shell.h"
#include "directory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

command_log_t g_command_log = {0};
static char* g_log_file_path = NULL;

static void init_log_path(void) {
    if (g_log_file_path) return;

    char* current_directory = get_current_directory();
    size_t path_len = strlen(current_directory) + 20;
    g_log_file_path = malloc(path_len);
    snprintf(g_log_file_path, path_len, "%s/.shell_history", current_directory);
    free(current_directory);
}

void log_init(void) {
    g_command_log.count = 0;
    g_command_log.head = 0;
    log_load_history();
}

void log_load_history(void) {
    init_log_path();
    
    FILE* file = fopen(g_log_file_path, "r");
    if (!file) return;
    
    g_command_log.count = 0;
    g_command_log.head = 0;
    
    char line[LOG_MAX_COMMAND_LENGTH];
    while (fgets(line, sizeof(line), file) && g_command_log.count < LOG_MAX_ENTRIES) {
        // Remove newline
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') {
            line[len-1] = '\0';
        }
        
        strncpy(g_command_log.commands[g_command_log.count], line, LOG_MAX_COMMAND_LENGTH - 1);
        g_command_log.commands[g_command_log.count][LOG_MAX_COMMAND_LENGTH - 1] = '\0';
        g_command_log.count++;
    }
    
    fclose(file);
}

void log_save_history(void) {
    init_log_path();
    
    FILE* file = fopen(g_log_file_path, "w");
    if (!file) return;
    
    for (int i = 0; i < g_command_log.count; i++) {
        fprintf(file, "%s\n", g_command_log.commands[i]);
    }
    
    fclose(file);
}

void log_add_command(const char* command) {
    if (!command || strlen(command) == 0) return;
    
    if (g_command_log.count > 0) {
        int last_idx = (g_command_log.count - 1) % LOG_MAX_ENTRIES;
        if (strcmp(g_command_log.commands[last_idx], command) == 0) {
            return;
        }
    }
    
    if (g_command_log.count < LOG_MAX_ENTRIES) {
        strncpy(g_command_log.commands[g_command_log.count], command, LOG_MAX_COMMAND_LENGTH - 1);
        g_command_log.commands[g_command_log.count][LOG_MAX_COMMAND_LENGTH - 1] = '\0';
        g_command_log.count++;
    } else {
        strncpy(g_command_log.commands[g_command_log.head], command, LOG_MAX_COMMAND_LENGTH - 1);
        g_command_log.commands[g_command_log.head][LOG_MAX_COMMAND_LENGTH - 1] = '\0';
        g_command_log.head = (g_command_log.head + 1) % LOG_MAX_ENTRIES;
    }
    
    log_save_history();
}

void cleanup_log(void) {
    if (g_log_file_path) {
        free(g_log_file_path);
        g_log_file_path = NULL;
    }
}
