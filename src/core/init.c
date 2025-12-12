#include "shell.h"
#include "directory.h"
#include "log.h"
#include "colors.h"
#include "builtins.h"
#include "ai.h"
#include <limits.h>
#include <sys/utsname.h>

void shell_init(void) {
    /* Get home directory */
    struct passwd* pw = getpwuid(getuid());
    if (pw && pw->pw_dir) {
        g_home_directory = strdup(pw->pw_dir);
    } else {
        char* home = getenv("HOME");
        if (home) {
            g_home_directory = strdup(home);
        } else {
            g_home_directory = strdup("/");
        }
    }
    
    /* Get username */
    if (pw && pw->pw_name) {
        g_username = strdup(pw->pw_name);
    } else {
        char* user = getenv("USER");
        if (user) {
            g_username = strdup(user);
        } else {
            g_username = strdup("user");
        }
    }
    
    /* Get system name */
    struct utsname uts;
    if (uname(&uts) == 0) {
        g_system_name = strdup(uts.nodename);
    } else {
        g_system_name = strdup("localhost");
    }
    
    /* Set shell name */
    g_shell_name = strdup(SHELL_NAME);
    
    /* Set default prompts */
    g_ps1 = strdup("\\[" COLOR_BOLD_GREEN "\\]\\u@\\h\\[" COLOR_RESET "\\]:"
                   "\\[" COLOR_BOLD_BLUE "\\]\\w\\[" COLOR_RESET "\\]$ ");
    g_ps2 = strdup("> ");
    
    /* Check if interactive */
    g_interactive = isatty(STDIN_FILENO);
    
    /* Initialize command log */
    log_init();
    
    /* Initialize AI features */
    ai_init();
}

void shell_cleanup(void) {
    log_save_history();
    
    if (g_home_directory) {
        free(g_home_directory);
        g_home_directory = NULL;
    }
    if (g_username) {
        free(g_username);
        g_username = NULL;
    }
    if (g_system_name) {
        free(g_system_name);
        g_system_name = NULL;
    }
    if (g_shell_name) {
        free(g_shell_name);
        g_shell_name = NULL;
    }
    if (g_ps1) {
        free(g_ps1);
        g_ps1 = NULL;
    }
    if (g_ps2) {
        free(g_ps2);
        g_ps2 = NULL;
    }
    
    cleanup_hop();
}
