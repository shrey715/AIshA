/**
 * @file builtins.h
 * @brief Built-in command definitions and function declarations
 * 
 * This header defines all shell built-in commands. Built-ins are commands
 * that are executed directly by the shell process rather than forking
 * an external process. This is necessary for commands like cd, exit,
 * and export that need to modify shell state.
 * 
 * The shell supports both original command names (hop, reveal, log, etc.)
 * and standard Unix names (cd, ls, history, etc.) for compatibility.
 */

#ifndef BUILTINS_H
#define BUILTINS_H

#include "shell.h"
#include "parser.h"
#include "execute.h"
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>

/*============================================================================
 * Builtin Entry Structure
 *============================================================================*/

/**
 * Descriptor for a built-in command
 */
typedef struct {
    const char* name;                      /**< Command name */
    int (*func)(char** args, int argc);    /**< Handler function */
    const char* help;                      /**< Short help description */
} builtin_entry_t;

/** Array of all built-in commands */
extern const builtin_entry_t builtins[];

/** Number of built-in commands */
extern const size_t builtins_count;

/*============================================================================
 * Directory Navigation Commands
 *============================================================================*/

/**
 * Change current working directory
 * 
 * Usage: hop [directory]
 *   hop       - Go to home directory
 *   hop ~     - Go to home directory
 *   hop -     - Go to previous directory
 *   hop ..    - Go to parent directory
 *   hop path  - Go to specified path
 * 
 * @param args Command arguments (args[0] is command name)
 * @param argc Number of arguments
 * @return 0 on success, 1 on failure
 */
int builtin_hop(char** args, int argc);

/** Alias for builtin_hop - standard cd command */
int builtin_cd(char** args, int argc);

/*============================================================================
 * File Listing Commands
 *============================================================================*/

/**
 * List directory contents with colors and formatting
 * 
 * Usage: reveal [-alh] [path]
 *   -a  Show all files including hidden
 *   -l  Long format with details
 *   -h  Human-readable sizes
 * 
 * @param args Command arguments
 * @param argc Number of arguments
 * @return 0 on success, 1 on failure
 */
int builtin_reveal(char** args, int argc);

/** Alias for builtin_reveal - standard ls command */
int builtin_ls(char** args, int argc);

/*============================================================================
 * History Commands
 *============================================================================*/

/**
 * Display or manipulate command history
 * 
 * Usage:
 *   history          - Display all history entries
 *   history N        - Display last N entries
 *   history -c       - Clear history
 *   history purge    - Clear history
 *   history !N       - Re-execute entry N
 *   history execute N - Re-execute entry N
 * 
 * @param args Command arguments
 * @param argc Number of arguments
 * @return 0 on success, 1 on failure
 */
int builtin_log(char** args, int argc);

/** Alias for builtin_log - standard history command */
int builtin_history(char** args, int argc);

/*============================================================================
 * Core Shell Commands
 *============================================================================*/

/**
 * Print text to stdout
 * 
 * Usage: echo [-neE] [text...]
 *   -n  Do not output trailing newline
 *   -e  Enable interpretation of escape sequences
 *   -E  Disable interpretation of escape sequences (default)
 * 
 * Escape sequences (with -e):
 *   \n newline, \t tab, \r carriage return, \\ backslash,
 *   \e escape, \0NNN octal, \xHH hex
 */
int builtin_echo(char** args, int argc);

/** Print current working directory */
int builtin_pwd(char** args, int argc);

/**
 * Exit the shell
 * 
 * Usage: exit [N]
 *   N - Exit with status N (default 0)
 */
int builtin_exit(char** args, int argc);

/** Alias for builtin_exit */
int builtin_quit(char** args, int argc);

/** Clear the terminal screen */
int builtin_clear(char** args, int argc);

/*============================================================================
 * Variable Management Commands
 *============================================================================*/

/**
 * Set or display exported environment variables
 * 
 * Usage:
 *   export           - List all exported variables
 *   export NAME=val  - Set and export variable
 *   export NAME      - Export existing variable
 */
int builtin_export(char** args, int argc);

/** Unset shell variables */
int builtin_unset(char** args, int argc);

/** Print all environment variables */
int builtin_env(char** args, int argc);

/** Display or set shell options and variables */
int builtin_set(char** args, int argc);

/*============================================================================
 * Alias Management Commands
 *============================================================================*/

/**
 * Define or display command aliases
 * 
 * Usage:
 *   alias            - List all aliases
 *   alias name       - Show specific alias
 *   alias name=value - Define alias
 */
int builtin_alias(char** args, int argc);

/**
 * Remove alias definitions
 * 
 * Usage:
 *   unalias name  - Remove specific alias
 *   unalias -a    - Remove all aliases
 */
int builtin_unalias(char** args, int argc);

/*============================================================================
 * Command Information Commands
 *============================================================================*/

/**
 * Show how a command would be interpreted
 * 
 * Usage: type name...
 * Reports whether each name is an alias, builtin, or external command.
 */
int builtin_type(char** args, int argc);

/**
 * Locate a command in PATH
 * 
 * Usage: which name...
 * Prints the full path of each command.
 */
int builtin_which(char** args, int argc);

/**
 * Display help for built-in commands
 * 
 * Usage:
 *   help       - List all built-ins
 *   help name  - Show help for specific command
 */
int builtin_help(char** args, int argc);

/*============================================================================
 * Job Control Commands
 *============================================================================*/

/** List background jobs (original name) */
int builtin_activities(char** args, int argc);

/** List background jobs (standard name) */
int builtin_jobs(char** args, int argc);

/** Send signal to process (original: ping PID SIGNAL) */
int builtin_ping(char** args, int argc);

/** Send signal to process (standard: kill [-SIGNAL] PID...) */
int builtin_kill(char** args, int argc);

/** Bring job to foreground: fg JOB_ID */
int builtin_fg(char** args, int argc);

/** Resume job in background: bg JOB_ID */
int builtin_bg(char** args, int argc);

/*============================================================================
 * Script Execution Commands
 *============================================================================*/

/**
 * Execute commands from a file
 * 
 * Usage: source filename [args...]
 * Reads and executes commands from filename in current environment.
 */
int builtin_source(char** args, int argc);

/** Alias for source (. filename) */
int builtin_dot(char** args, int argc);

/*============================================================================
 * Conditional Commands
 *============================================================================*/

/**
 * Evaluate conditional expression
 * 
 * Usage: test EXPRESSION
 * 
 * File tests: -e exists, -f regular, -d directory, -r readable, -w writable, -x executable
 * String tests: -z empty, -n non-empty, = equal, != not equal
 * Numeric tests: -eq, -ne, -lt, -le, -gt, -ge
 * 
 * @return 0 if condition is true, 1 if false
 */
int builtin_test(char** args, int argc);

/** Same as test but requires closing ] */
int builtin_bracket(char** args, int argc);

/*============================================================================
 * Utility Commands
 *============================================================================*/

/** Always return success (exit code 0) */
int builtin_true(char** args, int argc);

/** Always return failure (exit code 1) */
int builtin_false(char** args, int argc);

/** No-op command, always succeeds */
int builtin_colon(char** args, int argc);

/*============================================================================
 * Utility Functions
 *============================================================================*/

/**
 * Check if a command is a built-in
 * 
 * @param command Command name to check
 * @return Index in builtins array if found, -1 otherwise
 */
int is_builtin(const char* command);

/** Cleanup function for hop command (frees previous directory) */
void cleanup_hop(void);

/*============================================================================
 * AI Commands (AIshA - Advanced Intelligent Shell Assistant)
 *============================================================================*/

/** Chat with AI assistant: ai <message> */
int builtin_ai(char** args, int argc);

/** Translate natural language to command: ask <query> */
int builtin_ask(char** args, int argc);

/** Explain what a command does: explain <command> */
int builtin_explain(char** args, int argc);

/** Get AI fix for last error: aifix */
int builtin_aifix(char** args, int argc);

/** Show AI configuration: aiconfig */
int builtin_aiconfig(char** args, int argc);

/** Set Gemini API key: aikey [-s] KEY */
int builtin_aikey(char** args, int argc);

/** Set last command for aifix tracking */
void ai_set_last_command(const char* cmd);

/** Set last error for aifix tracking */
void ai_set_last_error(const char* err);

#endif /* BUILTINS_H */
