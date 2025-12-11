/**
 * @file variables.h
 * @brief Shell variable management system
 * 
 * Provides a comprehensive variable management system supporting:
 * - Regular shell variables
 * - Environment variable synchronization
 * - Special variables ($?, $$, $!, $#, $0-$9, $@, $*)
 * - Variable expansion with modifiers (${VAR:-default}, ${VAR:=default}, ${#VAR})
 * - Variable flags (exported, readonly, local, integer)
 */

#ifndef VARIABLES_H
#define VARIABLES_H

#include <stdlib.h>
#include <sys/types.h>

/*============================================================================
 * Constants
 *============================================================================*/

/** Maximum number of variables in hash table */
#define MAX_VARIABLES 1024

/** Maximum length of variable name */
#define MAX_VAR_NAME_LENGTH 256

/*============================================================================
 * Variable Flags
 *============================================================================*/

/** Variable is exported to child processes */
#define VAR_FLAG_EXPORTED  0x01

/** Variable cannot be modified or unset */
#define VAR_FLAG_READONLY  0x02

/** Variable is local to current function scope */
#define VAR_FLAG_LOCAL     0x04

/** Variable value should be treated as integer */
#define VAR_FLAG_INTEGER   0x08

/*============================================================================
 * Variable Structure
 *============================================================================*/

/**
 * Shell variable entry
 */
typedef struct {
    char* name;           /**< Variable name */
    char* value;          /**< Variable value (always stored as string) */
    unsigned int flags;   /**< Combination of VAR_FLAG_* constants */
} shell_var_t;

/*============================================================================
 * Special Variables (Global State)
 *============================================================================*/

/** Exit status of last executed command ($?) */
extern int g_last_exit_status;

/** PID of the shell process ($$) */
extern pid_t g_shell_pid;

/** PID of most recent background process ($!) */
extern pid_t g_last_background_pid;

/** Number of positional arguments ($#) */
extern int g_arg_count;

/** Positional arguments array ($0, $1, $2, ...) */
extern char** g_positional_args;

/*============================================================================
 * Initialization and Cleanup
 *============================================================================*/

/**
 * Initialize the variable subsystem
 * 
 * Sets up special variables and imports environment variables.
 * Must be called during shell startup.
 */
void variables_init(void);

/**
 * Clean up variable subsystem
 * 
 * Frees all variable storage. Called during shell shutdown.
 */
void variables_cleanup(void);

/*============================================================================
 * Variable Operations
 *============================================================================*/

/**
 * Get the value of a variable
 * 
 * Handles regular variables, environment variables, and special variables
 * ($?, $$, $!, $#, $0-$9).
 * 
 * @param name Variable name (without $ prefix)
 * @return Variable value string, or NULL if not found.
 *         For special variables, returns pointer to static buffer.
 */
const char* get_variable(const char* name);

/**
 * Set a variable value
 * 
 * @param name Variable name
 * @param value Variable value
 * @param flags Combination of VAR_FLAG_* constants
 * @return 0 on success, -1 on failure (e.g., readonly variable)
 */
int set_variable(const char* name, const char* value, unsigned int flags);

/**
 * Unset (remove) a variable
 * 
 * @param name Variable name
 * @return 0 on success, -1 if readonly or error
 */
int unset_variable(const char* name);

/**
 * Export a variable to the environment
 * 
 * @param name Variable name
 * @return 0 on success
 */
int export_variable(const char* name);

/**
 * Check if a variable is exported
 * 
 * @param name Variable name
 * @return Non-zero if exported
 */
int is_variable_exported(const char* name);

/**
 * Check if a variable is readonly
 * 
 * @param name Variable name
 * @return Non-zero if readonly
 */
int is_variable_readonly(const char* name);

/**
 * Check if a variable exists
 * 
 * @param name Variable name
 * @return Non-zero if variable exists
 */
int variable_exists(const char* name);

/**
 * List all variables
 * 
 * @param exported_only If non-zero, only list exported variables
 */
void list_variables(int exported_only);

/*============================================================================
 * Variable Expansion
 *============================================================================*/

/**
 * Expand all variable references in a string
 * 
 * Processes $VAR, ${VAR}, and ${VAR:-default} syntax.
 * Handles escape sequences (\$).
 * 
 * @param input String containing variable references
 * @return Newly allocated string with expansions applied.
 *         Caller must free.
 */
char* expand_variables(const char* input);

/**
 * Expand a single variable reference
 * 
 * Parses and expands a reference starting with $ at the given position.
 * Supports: $VAR, ${VAR}, ${VAR:-default}, ${VAR:=default}, ${#VAR}
 * 
 * @param ref Pointer to $ character starting the reference
 * @param consumed Output: number of characters consumed from input
 * @return Newly allocated string with the expanded value.
 *         Caller must free.
 */
char* expand_variable_reference(const char* ref, size_t* consumed);

/*============================================================================
 * Positional Arguments
 *============================================================================*/

/**
 * Set positional arguments ($0, $1, $2, ...)
 * 
 * @param argc Number of arguments
 * @param argv Array of argument strings
 */
void set_positional_args(int argc, char** argv);

/** Save current positional arguments (for function calls) */
void save_positional_args(void);

/** Restore previously saved positional arguments */
void restore_positional_args(void);

/*============================================================================
 * Special Variable Updates
 *============================================================================*/

/**
 * Update the exit status variable ($?)
 * 
 * @param status New exit status value
 */
void update_exit_status(int status);

/**
 * Update the last background PID variable ($!)
 * 
 * @param pid PID of most recent background process
 */
void update_last_background_pid(pid_t pid);

/*============================================================================
 * Environment Synchronization
 *============================================================================*/

/** Import all environment variables into shell variables */
void sync_from_environment(void);

/** Export all marked variables to the environment */
void sync_to_environment(void);

#endif /* VARIABLES_H */
