/**
 * @file shell.h
 * @brief Core shell definitions and global state
 * 
 * This header defines the fundamental types, constants, and global variables
 * used throughout the C-Shell. It provides the main shell lifecycle functions
 * and core configuration.
 * 
 * @author CShell Team
 * @version 2.0.0
 */

#ifndef SHELL_H
#define SHELL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <pwd.h>

/*============================================================================
 * Version Information
 *============================================================================*/

/** Shell version string */
#define SHELL_VERSION "3.0.0"

/** Shell name used in prompts and identification */
#define SHELL_NAME "aisha"

/** Shell full name */
#define SHELL_FULL_NAME "AIshA - Advanced Intelligent Shell Assistant"

/*============================================================================
 * Buffer Size Constants
 *============================================================================*/

/** Maximum length of user input line */
#define SHELL_MAX_INPUT_LENGTH 4096

/** Maximum path length for file operations */
#define SHELL_MAX_PATH_LENGTH 4096

/** Maximum size of the generated prompt string */
#define SHELL_PROMPT_SIZE 1024

/*============================================================================
 * Return Codes
 *============================================================================*/

/** Successful operation return code */
#define SHELL_SUCCESS 0

/** Failed operation return code */
#define SHELL_FAILURE 1

/*============================================================================
 * Global State Variables
 *============================================================================*/

/** User's home directory path (from $HOME or getpwuid) */
extern char* g_home_directory;

/** Current user's username */
extern char* g_username;

/** System hostname */
extern char* g_system_name;

/** Shell executable name */
extern char* g_shell_name;

/** Primary prompt string (PS1) - supports escape sequences */
extern char* g_ps1;

/** Secondary prompt string (PS2) - used for continuations */
extern char* g_ps2;

/** Flag indicating if shell is running interactively (has a TTY) */
extern int g_interactive;

/*============================================================================
 * Shell Lifecycle Functions
 *============================================================================*/

/**
 * Initialize the shell environment
 * 
 * Sets up global variables (home directory, username, hostname),
 * initializes the command history, and configures default prompts.
 * Must be called before entering the main shell loop.
 */
void shell_init(void);

/**
 * Display the shell prompt
 * 
 * Generates and prints the prompt based on PS1 configuration,
 * expanding escape sequences like \u, \h, \w, etc.
 */
void shell_show_prompt(void);

/**
 * Read a line of input from stdin
 * 
 * For non-interactive mode, reads a line using fgets.
 * For interactive mode, use shell_readline() instead.
 * 
 * @return Dynamically allocated string containing the input line,
 *         or NULL on EOF/error. Caller must free the returned string.
 */
char* shell_read_input(void);

/**
 * Clean up shell resources before exit
 * 
 * Frees all dynamically allocated global variables,
 * saves command history, and performs any necessary cleanup.
 */
void shell_cleanup(void);

/*============================================================================
 * Configuration Functions
 *============================================================================*/

/**
 * Load shell configuration from ~/.aisharc
 * 
 * Reads and executes commands from the user's configuration file.
 * Typically called during shell initialization.
 */
void shell_load_config(void);

/**
 * Save shell configuration (reserved for future use)
 */
void shell_save_config(void);

/*============================================================================
 * Prompt Generation
 *============================================================================*/

/**
 * Generate a prompt string from a format specification
 * 
 * Expands escape sequences in the format string:
 *   \u - Username
 *   \h - Hostname (short)
 *   \H - Hostname (full)
 *   \w - Working directory (with ~ substitution)
 *   \W - Working directory basename
 *   \$ - $ for user, # for root
 *   \t - Time HH:MM:SS
 *   \T - Time HH:MM
 *   \d - Date
 *   \n - Newline
 *   \e - Escape character (for ANSI codes)
 *   \v - Shell version (short)
 *   \V - Shell version (full)
 * 
 * @param format The format string (typically PS1)
 * @return Dynamically allocated prompt string. Caller must free.
 */
char* shell_generate_prompt(const char* format);

#endif /* SHELL_H */
