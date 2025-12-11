/**
 * @file builtins_core.c
 * @brief Core shell builtin commands
 * 
 * Implements: echo, pwd, exit, quit, clear
 */

#include "builtins.h"
#include "variables.h"
#include "alias.h"
#include "background.h"
#include "colors.h"
#include "directory.h"
#include <ctype.h>

/**
 * echo - Print text to stdout
 * 
 * Usage: echo [-neE] [text...]
 *   -n  Do not output trailing newline
 *   -e  Enable interpretation of escape sequences
 *   -E  Disable interpretation of escape sequences (default)
 */
int builtin_echo(char** args, int argc) {
    int interpret_escapes = 0;
    int no_newline = 0;
    int start = 1;
    
    /* Parse options */
    for (int i = 1; i < argc; i++) {
        if (args[i][0] == '-' && args[i][1] != '\0') {
            int is_option = 1;
            for (int j = 1; args[i][j]; j++) {
                if (args[i][j] != 'n' && args[i][j] != 'e' && args[i][j] != 'E') {
                    is_option = 0;
                    break;
                }
            }
            
            if (is_option) {
                for (int j = 1; args[i][j]; j++) {
                    switch (args[i][j]) {
                        case 'n': no_newline = 1; break;
                        case 'e': interpret_escapes = 1; break;
                        case 'E': interpret_escapes = 0; break;
                    }
                }
                start = i + 1;
            } else {
                break;
            }
        } else {
            break;
        }
    }
    
    for (int i = start; i < argc; i++) {
        if (i > start) printf(" ");
        
        if (interpret_escapes) {
            for (const char* p = args[i]; *p; p++) {
                if (*p == '\\' && *(p + 1)) {
                    p++;
                    switch (*p) {
                        case 'n': putchar('\n'); break;
                        case 't': putchar('\t'); break;
                        case 'r': putchar('\r'); break;
                        case 'a': putchar('\a'); break;
                        case 'b': putchar('\b'); break;
                        case 'f': putchar('\f'); break;
                        case 'v': putchar('\v'); break;
                        case '\\': putchar('\\'); break;
                        case '0': {
                            int val = 0;
                            for (int j = 0; j < 3 && p[1] >= '0' && p[1] <= '7'; j++) {
                                p++;
                                val = val * 8 + (*p - '0');
                            }
                            putchar(val);
                            break;
                        }
                        case 'x': {
                            int val = 0;
                            p++;
                            for (int j = 0; j < 2 && isxdigit(p[1]); j++) {
                                p++;
                                if (isdigit(*p)) val = val * 16 + (*p - '0');
                                else val = val * 16 + (tolower(*p) - 'a' + 10);
                            }
                            putchar(val);
                            break;
                        }
                        case 'e': printf("\033"); break;
                        default: putchar('\\'); putchar(*p); break;
                    }
                } else {
                    putchar(*p);
                }
            }
        } else {
            printf("%s", args[i]);
        }
    }
    
    if (!no_newline) {
        printf("\n");
    }
    
    fflush(stdout);
    return 0;
}

/**
 * pwd - Print working directory
 */
int builtin_pwd(char** args, int argc) {
    (void)args;
    (void)argc;
    
    char* cwd = get_current_directory();
    if (cwd) {
        printf("%s\n", cwd);
        free(cwd);
        return 0;
    } else {
        print_error("pwd: error retrieving current directory\n");
        return 1;
    }
}

/**
 * exit - Exit the shell
 * 
 * Usage: exit [N]
 *   N - Exit with status N (default 0)
 */
int builtin_exit(char** args, int argc) {
    int exit_code = 0;
    
    if (argc > 1) {
        char* endptr;
        long val = strtol(args[1], &endptr, 10);
        if (*endptr == '\0') {
            exit_code = (int)(val & 0xFF);
        } else {
            print_error("exit: %s: numeric argument required\n", args[1]);
            exit_code = 2;
        }
    }
    
    /* Cleanup */
    cleanup_hop();
    cleanup_background_jobs();
    shell_cleanup();
    variables_cleanup();
    alias_cleanup();
    
    exit(exit_code);
    return exit_code;  /* Never reached */
}

int builtin_quit(char** args, int argc) {
    return builtin_exit(args, argc);
}

/**
 * clear - Clear the terminal screen
 */
int builtin_clear(char** args, int argc) {
    (void)args;
    (void)argc;
    
    printf("\033[2J\033[H");
    fflush(stdout);
    return 0;
}
