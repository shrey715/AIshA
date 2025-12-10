#ifndef SHELL_H
#define SHELL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <pwd.h>

// Constants
#define SHELL_MAX_INPUT_LENGTH 4096
#define SHELL_MAX_PATH_LENGTH 1024
#define SHELL_PROMPT_SIZE 512

#define SHELL_SUCCESS 0
#define SHELL_FAILURE 1

// Global variables
extern char* g_home_directory;
extern char* g_username;
extern char* g_system_name;

// Function declarations
void shell_init(void);
void shell_show_prompt(void);
char* shell_read_input(void);
void shell_cleanup(void);

#endif
