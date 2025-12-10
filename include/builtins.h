#ifndef BUILTINS_H
#define BUILTINS_H

#include "shell.h"
#include "parser.h"
#include "execute.h"
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>

// Descriptor for a builtin entry
typedef struct {
    const char* name;
    int (*func)(char** args, int argc);
} builtin_entry_t;

extern const builtin_entry_t builtins[];
extern const size_t builtins_count;

// Built-in command implementations
int builtin_hop(char** args, int argc);
int builtin_reveal(char** args, int argc);
int builtin_log(char** args, int argc);

// Extrinsic commands
int builtin_activities(char** args, int argc);
int builtin_ping(char** args, int argc);
int builtin_fg(char** args, int argc);
int builtin_bg(char** args, int argc);

// Builtin utility functions
int is_builtin(const char* command);

// Clean-up routines
void cleanup_hop(void);

#endif
