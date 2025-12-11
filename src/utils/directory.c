#include "directory.h"
#include "shell.h"

char* get_current_directory(void) {
    char* cwd = getcwd(NULL, 0);
    return cwd;
}

char* get_parent_directory(void) {
    char* cwd = get_current_directory();
    if (!cwd) return NULL;

    char* last_slash = strrchr(cwd, '/');
    if (last_slash) {
        if (last_slash == cwd) {
            last_slash[1] = '\0'; // Keep the leading '/'
        } else {
            *last_slash = '\0'; // Truncate
        }
    }
    return cwd;
}
