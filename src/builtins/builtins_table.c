/**
 * @file builtins_table.c
 * @brief Builtin command table and lookup functions
 * 
 * Contains the central registry of all builtin commands and the
 * is_builtin() lookup function.
 */

#include "builtins.h"

/* Builtin command table - central registry of all builtins */
const builtin_entry_t builtins[] = {
    /* Directory navigation */
    { "hop",        builtin_hop,        "Change directory (alias: cd)" },
    { "cd",         builtin_cd,         "Change directory" },
    
    /* File listing */
    { "reveal",     builtin_reveal,     "List directory contents (alias: ls)" },
    { "ls",         builtin_ls,         "List directory contents" },
    
    /* History */
    { "log",        builtin_log,        "Show command history (alias: history)" },
    { "history",    builtin_history,    "Show command history" },
    
    /* Core builtins */
    { "echo",       builtin_echo,       "Display a line of text" },
    { "pwd",        builtin_pwd,        "Print working directory" },
    { "exit",       builtin_exit,       "Exit the shell" },
    { "quit",       builtin_quit,       "Exit the shell (alias: exit)" },
    { "clear",      builtin_clear,      "Clear the terminal screen" },
    
    /* Variable management */
    { "export",     builtin_export,     "Set environment variable" },
    { "unset",      builtin_unset,      "Unset a variable" },
    { "env",        builtin_env,        "Print environment variables" },
    { "set",        builtin_set,        "Set shell options or show variables" },
    
    /* Alias management */
    { "alias",      builtin_alias,      "Define or display aliases" },
    { "unalias",    builtin_unalias,    "Remove alias definitions" },
    
    /* Command information */
    { "type",       builtin_type,       "Indicate how a command would be interpreted" },
    { "which",      builtin_which,      "Locate a command" },
    { "help",       builtin_help,       "Display help for builtins" },
    
    /* Job control */
    { "activities", builtin_activities, "List background jobs (alias: jobs)" },
    { "jobs",       builtin_jobs,       "List background jobs" },
    { "ping",       builtin_ping,       "Send signal to process" },
    { "kill",       builtin_kill,       "Send signal to process" },
    { "fg",         builtin_fg,         "Move job to foreground" },
    { "bg",         builtin_bg,         "Move job to background" },
    
    /* File operations */
    { "source",     builtin_source,     "Execute commands from a file" },
    { ".",          builtin_dot,        "Execute commands from a file" },
    
    /* Test/condition */
    { "test",       builtin_test,       "Evaluate conditional expression" },
    { "[",          builtin_bracket,    "Evaluate conditional expression" },
    
    /* Miscellaneous */
    { "true",       builtin_true,       "Return success" },
    { "false",      builtin_false,      "Return failure" },
    { ":",          builtin_colon,      "Null command (no-op)" },
    
    /* AI Commands - AIshA (Advanced Intelligent Shell Assistant) */
    { "ai",         builtin_ai,         "Chat with AI assistant" },
    { "ask",        builtin_ask,        "Translate natural language to command" },
    { "explain",    builtin_explain,    "Explain what a command does" },
    { "aifix",      builtin_aifix,      "Get AI fix for last error" },
    { "aiconfig",   builtin_aiconfig,   "Show AI configuration" },
    { "aikey",      builtin_aikey,      "Set Gemini API key" },
};

const size_t builtins_count = sizeof(builtins) / sizeof(builtins[0]);

/**
 * Check if a command is a built-in
 * 
 * @param command Command name to check
 * @return Index in builtins array if found, -1 otherwise
 */
int is_builtin(const char* command) {
    if (!command) return -1;
    for (size_t i = 0; i < builtins_count; ++i) {
        if (strcmp(command, builtins[i].name) == 0) {
            return (int)i;
        }
    }
    return -1;
}
