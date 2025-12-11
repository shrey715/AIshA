/**
 * @file builtins_vars.c
 * @brief Variable and alias management builtin commands
 * 
 * Implements: export, unset, env, set, alias, unalias, type, which, help
 */

#include "builtins.h"
#include "variables.h"
#include "alias.h"
#include "colors.h"

/*============================================================================
 * Variable Management
 *============================================================================*/

/**
 * export - Set or display exported environment variables
 */
int builtin_export(char** args, int argc) {
    if (argc == 1) {
        list_variables(1);
        return 0;
    }
    
    for (int i = 1; i < argc; i++) {
        char* eq = strchr(args[i], '=');
        if (eq) {
            *eq = '\0';
            set_variable(args[i], eq + 1, VAR_FLAG_EXPORTED);
            *eq = '=';
        } else {
            export_variable(args[i]);
        }
    }
    return 0;
}

/**
 * unset - Remove variables
 */
int builtin_unset(char** args, int argc) {
    if (argc < 2) {
        print_error("unset: usage: unset NAME...\n");
        return 1;
    }
    
    int ret = 0;
    for (int i = 1; i < argc; i++) {
        if (unset_variable(args[i]) != 0) {
            ret = 1;
        }
    }
    return ret;
}

/**
 * env - Print all environment variables
 */
int builtin_env(char** args, int argc) {
    (void)args;
    (void)argc;
    
    extern char** environ;
    for (char** env = environ; *env; env++) {
        printf("%s\n", *env);
    }
    return 0;
}

/**
 * set - Display or set shell options and variables
 */
int builtin_set(char** args, int argc) {
    if (argc == 1) {
        list_variables(0);
        return 0;
    }
    /* TODO: Implement shell options (-e, -x, etc.) */
    return 0;
}

/*============================================================================
 * Alias Management
 *============================================================================*/

/**
 * alias - Define or display aliases
 */
int builtin_alias(char** args, int argc) {
    if (argc == 1) {
        list_aliases();
        return 0;
    }
    
    for (int i = 1; i < argc; i++) {
        char* eq = strchr(args[i], '=');
        if (eq) {
            *eq = '\0';
            set_alias(args[i], eq + 1);
            *eq = '=';
        } else {
            const char* value = get_alias(args[i]);
            if (value) {
                printf("alias %s='%s'\n", args[i], value);
            } else {
                print_error("alias: %s: not found\n", args[i]);
                return 1;
            }
        }
    }
    return 0;
}

/**
 * unalias - Remove alias definitions
 */
int builtin_unalias(char** args, int argc) {
    if (argc < 2) {
        print_error("unalias: usage: unalias NAME...\n");
        return 1;
    }
    
    int ret = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(args[i], "-a") == 0) {
            alias_cleanup();
            alias_init();
        } else if (unset_alias(args[i]) != 0) {
            print_error("unalias: %s: not found\n", args[i]);
            ret = 1;
        }
    }
    return ret;
}

/*============================================================================
 * Command Information
 *============================================================================*/

/**
 * type - Show how a command would be interpreted
 */
int builtin_type(char** args, int argc) {
    if (argc < 2) {
        print_error("type: usage: type NAME...\n");
        return 1;
    }
    
    int ret = 0;
    for (int i = 1; i < argc; i++) {
        const char* alias_val = get_alias(args[i]);
        if (alias_val) {
            printf("%s is aliased to '%s'\n", args[i], alias_val);
            continue;
        }
        
        if (is_builtin(args[i]) >= 0) {
            printf("%s is a shell builtin\n", args[i]);
            continue;
        }
        
        char* path = getenv("PATH");
        if (path) {
            char* path_copy = strdup(path);
            char* dir = strtok(path_copy, ":");
            int found = 0;
            
            while (dir) {
                char full_path[4096];
                snprintf(full_path, sizeof(full_path), "%s/%s", dir, args[i]);
                if (access(full_path, X_OK) == 0) {
                    printf("%s is %s\n", args[i], full_path);
                    found = 1;
                    break;
                }
                dir = strtok(NULL, ":");
            }
            
            free(path_copy);
            if (found) continue;
        }
        
        print_error("type: %s: not found\n", args[i]);
        ret = 1;
    }
    return ret;
}

/**
 * which - Locate a command in PATH
 */
int builtin_which(char** args, int argc) {
    if (argc < 2) {
        print_error("which: usage: which NAME...\n");
        return 1;
    }
    
    int ret = 0;
    for (int i = 1; i < argc; i++) {
        char* path = getenv("PATH");
        int found = 0;
        
        if (path) {
            char* path_copy = strdup(path);
            char* dir = strtok(path_copy, ":");
            
            while (dir) {
                char full_path[4096];
                snprintf(full_path, sizeof(full_path), "%s/%s", dir, args[i]);
                if (access(full_path, X_OK) == 0) {
                    printf("%s\n", full_path);
                    found = 1;
                    break;
                }
                dir = strtok(NULL, ":");
            }
            
            free(path_copy);
        }
        
        if (!found) {
            print_error("which: %s: not found\n", args[i]);
            ret = 1;
        }
    }
    return ret;
}

/**
 * help - Display help for builtin commands
 */
int builtin_help(char** args, int argc) {
    if (argc == 1) {
        printf("\n");
        printf("  %sAIshA%s - Advanced Intelligent Shell Assistant\n\n", COLOR_BOLD_CYAN, COLOR_RESET);
        
        printf("  %sAI Commands:%s\n", COLOR_BOLD, COLOR_RESET);
        printf("    %sai%s         Chat with AI assistant\n", COLOR_GREEN, COLOR_RESET);
        printf("    %sask%s        Translate natural language to command\n", COLOR_GREEN, COLOR_RESET);
        printf("    %sexplain%s    Explain what a command does\n", COLOR_GREEN, COLOR_RESET);
        printf("    %saifix%s      Get AI fix for last error\n", COLOR_GREEN, COLOR_RESET);
        printf("    %saiconfig%s   Show AI configuration\n", COLOR_GREEN, COLOR_RESET);
        printf("    %saikey%s      Set Gemini API key\n", COLOR_GREEN, COLOR_RESET);
        printf("\n");
        
        printf("  %sNavigation:%s\n", COLOR_BOLD, COLOR_RESET);
        printf("    %scd%s         Change directory\n", COLOR_CYAN, COLOR_RESET);
        printf("    %spwd%s        Print working directory\n", COLOR_CYAN, COLOR_RESET);
        printf("    %sls%s         List directory contents\n", COLOR_CYAN, COLOR_RESET);
        printf("\n");
        
        printf("  %sShell:%s\n", COLOR_BOLD, COLOR_RESET);
        printf("    %shistory%s    Show command history\n", COLOR_CYAN, COLOR_RESET);
        printf("    %salias%s      Define command aliases\n", COLOR_CYAN, COLOR_RESET);
        printf("    %sexport%s     Set environment variables\n", COLOR_CYAN, COLOR_RESET);
        printf("    %ssource%s     Execute script file\n", COLOR_CYAN, COLOR_RESET);
        printf("    %sexit%s       Exit the shell\n", COLOR_CYAN, COLOR_RESET);
        printf("\n");
        
        printf("  %sJobs:%s\n", COLOR_BOLD, COLOR_RESET);
        printf("    %sjobs%s       List background jobs\n", COLOR_CYAN, COLOR_RESET);
        printf("    %sfg%s         Bring job to foreground\n", COLOR_CYAN, COLOR_RESET);
        printf("    %sbg%s         Continue job in background\n", COLOR_CYAN, COLOR_RESET);
        printf("    %skill%s       Send signal to process\n", COLOR_CYAN, COLOR_RESET);
        printf("\n");
        
        printf("  %sTip:%s Use %shelp <command>%s for detailed help\n\n", 
               COLOR_DIM, COLOR_RESET, COLOR_BOLD, COLOR_RESET);
    } else {
        for (int i = 1; i < argc; i++) {
            int idx = is_builtin(args[i]);
            if (idx >= 0) {
                printf("%s%s%s: %s\n", COLOR_BOLD, args[i], COLOR_RESET, builtins[idx].help);
            } else {
                print_error("help: %s: not a builtin\n", args[i]);
            }
        }
    }
    return 0;
}
