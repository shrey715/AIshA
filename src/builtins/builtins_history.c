/**
 * @file builtins_history.c
 * @brief History management builtin commands
 * 
 * Implements: log/history
 */

#include "builtins.h"
#include "log.h"
#include "colors.h"
#include "execute.h"
#include "variables.h"

/**
 * log/history - Display or manipulate command history
 * 
 * Usage:
 *   history          - Display all history entries
 *   history N        - Display last N entries  
 *   history -c       - Clear all history
 *   history purge    - Clear all history
 *   history !N       - Re-execute command number N
 *   history execute N - Re-execute command number N
 */
int builtin_log(char** args, int argc) {
    int entries = g_command_log.count < LOG_MAX_ENTRIES ? g_command_log.count : LOG_MAX_ENTRIES;
    int start = g_command_log.count < LOG_MAX_ENTRIES ? 0 : g_command_log.head;
    
    if (argc == 1) {
        /* Show all history entries */
        for (int i = 0; i < entries; i++) {
            int idx = (start + i) % LOG_MAX_ENTRIES;
            printf("%5d  %s\n", i + 1, g_command_log.commands[idx]);
        }
        return 0;
    }
    
    /* Check for numeric argument (show last N entries) */
    char* endptr;
    long num = strtol(args[1], &endptr, 10);
    if (*endptr == '\0' && num > 0) {
        int show_count = (int)num;
        if (show_count > entries) show_count = entries;
        
        int offset = entries - show_count;
        for (int i = offset; i < entries; i++) {
            int idx = (start + i) % LOG_MAX_ENTRIES;
            printf("%5d  %s\n", i + 1, g_command_log.commands[idx]);
        }
        return 0;
    }
    
    /* Clear command */
    if (strcmp(args[1], "purge") == 0 || strcmp(args[1], "-c") == 0) {
        g_command_log.count = 0;
        g_command_log.head = 0;
        log_save_history();
        print_success("History cleared\n");
        return 0;
    }
    
    /* Re-execute command */
    int exec_index = -1;
    if (args[1][0] == '!' && args[1][1] != '\0') {
        exec_index = atoi(args[1] + 1);
    } else if (argc == 3 && strcmp(args[1], "execute") == 0) {
        exec_index = atoi(args[2]);
    }
    
    if (exec_index > 0) {
        if (exec_index > entries) {
            print_error("history: %d: event not found\n", exec_index);
            return 1;
        }
        
        int idx = (start + exec_index - 1) % LOG_MAX_ENTRIES;
        char* command_to_execute = g_command_log.commands[idx];
        
        printf("%s\n", command_to_execute);
        
        char* expanded = expand_variables(command_to_execute);
        if (!expanded) {
            print_error("history: expansion failed\n");
            return 1;
        }
        
        token_t tokens[MAX_TOKENS];
        int token_count = tokenize_input(expanded, tokens, MAX_TOKENS);
        int result = 0;
        if (token_count > 0) {
            result = execute_shell_command_with_operators(tokens, token_count);
        }
        
        free(expanded);
        return result;
    }
    
    /* Usage message */
    print_error("history: usage:\n");
    print_error("  history          - Show all history\n");
    print_error("  history N        - Show last N entries\n");
    print_error("  history -c       - Clear history\n");
    print_error("  history !N       - Re-execute entry N\n");
    return 1;
}

int builtin_history(char** args, int argc) {
    return builtin_log(args, argc);
}
