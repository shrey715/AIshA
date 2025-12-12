#include "directory.h"
#include "shell.h"
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

char* get_current_directory(void) {
    char* cwd = getcwd(NULL, 0);
    if (cwd) {
        return cwd;
    }
    
    /* getcwd() failed - try /proc/self/cwd on Linux */
    char buf[PATH_MAX];
    ssize_t len = readlink("/proc/self/cwd", buf, sizeof(buf) - 1);
    if (len > 0) {
        buf[len] = '\0';
        return strdup(buf);
    }
    
    /* Final fallback: use home directory or "/" */
    if (g_home_directory) {
        return strdup(g_home_directory);
    }
    
    return strdup("/");
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
