/**
 * @file builtins_test.c
 * @brief Test/conditional builtin commands
 * 
 * Implements: test, [, true, false, :
 */

#include "builtins.h"
#include "colors.h"
#include <sys/stat.h>

/**
 * test - Evaluate conditional expression
 * 
 * File tests: -e exists, -f regular, -d directory, -r readable, -w writable, -x executable, -s non-empty
 * String tests: -z empty, -n non-empty, = equal, != not equal
 * Numeric tests: -eq, -ne, -lt, -le, -gt, -ge
 */
int builtin_test(char** args, int argc) {
    if (argc == 1) return 1;  /* No args = false */
    
    /* Single argument (string non-empty test) */
    if (argc == 2) {
        return args[1][0] == '\0' ? 1 : 0;
    }
    
    /* Unary operators */
    if (argc == 3) {
        const char* op = args[1];
        const char* arg = args[2];
        
        struct stat st;
        
        if (strcmp(op, "-e") == 0) return stat(arg, &st) == 0 ? 0 : 1;
        if (strcmp(op, "-f") == 0) return (stat(arg, &st) == 0 && S_ISREG(st.st_mode)) ? 0 : 1;
        if (strcmp(op, "-d") == 0) return (stat(arg, &st) == 0 && S_ISDIR(st.st_mode)) ? 0 : 1;
        if (strcmp(op, "-r") == 0) return access(arg, R_OK) == 0 ? 0 : 1;
        if (strcmp(op, "-w") == 0) return access(arg, W_OK) == 0 ? 0 : 1;
        if (strcmp(op, "-x") == 0) return access(arg, X_OK) == 0 ? 0 : 1;
        if (strcmp(op, "-s") == 0) return (stat(arg, &st) == 0 && st.st_size > 0) ? 0 : 1;
        if (strcmp(op, "-z") == 0) return arg[0] == '\0' ? 0 : 1;
        if (strcmp(op, "-n") == 0) return arg[0] != '\0' ? 0 : 1;
        if (strcmp(op, "!") == 0) return arg[0] == '\0' ? 0 : 1;
    }
    
    /* Binary operators */
    if (argc == 4) {
        const char* left = args[1];
        const char* op = args[2];
        const char* right = args[3];
        
        /* String comparisons */
        if (strcmp(op, "=") == 0 || strcmp(op, "==") == 0) 
            return strcmp(left, right) == 0 ? 0 : 1;
        if (strcmp(op, "!=") == 0) 
            return strcmp(left, right) != 0 ? 0 : 1;
        
        /* Numeric comparisons */
        long l = strtol(left, NULL, 10);
        long r = strtol(right, NULL, 10);
        
        if (strcmp(op, "-eq") == 0) return l == r ? 0 : 1;
        if (strcmp(op, "-ne") == 0) return l != r ? 0 : 1;
        if (strcmp(op, "-lt") == 0) return l < r ? 0 : 1;
        if (strcmp(op, "-le") == 0) return l <= r ? 0 : 1;
        if (strcmp(op, "-gt") == 0) return l > r ? 0 : 1;
        if (strcmp(op, "-ge") == 0) return l >= r ? 0 : 1;
    }
    
    print_error("test: unrecognized condition\n");
    return 2;
}

/**
 * [ - Same as test but requires closing ]
 */
int builtin_bracket(char** args, int argc) {
    if (argc < 2 || strcmp(args[argc - 1], "]") != 0) {
        print_error("[: missing ']'\n");
        return 2;
    }
    
    args[argc - 1] = NULL;
    return builtin_test(args, argc - 1);
}

/**
 * true - Always return success
 */
int builtin_true(char** args, int argc) {
    (void)args;
    (void)argc;
    return 0;
}

/**
 * false - Always return failure
 */
int builtin_false(char** args, int argc) {
    (void)args;
    (void)argc;
    return 1;
}

/**
 * : - Null command (no-op), always succeeds
 */
int builtin_colon(char** args, int argc) {
    (void)args;
    (void)argc;
    return 0;
}
