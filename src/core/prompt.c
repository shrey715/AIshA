#include "shell.h"
#include "directory.h"
#include "colors.h"
#include <time.h>

/* Generate prompt from format string with escape codes */
char* shell_generate_prompt(const char* format) {
    if (!format) {
        format = "$ ";
    }
    
    char* result = malloc(SHELL_PROMPT_SIZE);
    if (!result) return strdup("$ ");
    
    int pos = 0;
    
    for (const char* p = format; *p && pos < SHELL_PROMPT_SIZE - 1; p++) {
        if (*p == '\\' && *(p + 1) == '[') {
            /* Start of non-printing sequence (ANSI codes) - skip markers */
            p++;
            continue;
        }
        if (*p == '\\' && *(p + 1) == ']') {
            /* End of non-printing sequence - skip markers */
            p++;
            continue;
        }
        
        if (*p == '\\' && *(p + 1)) {
            p++;
            switch (*p) {
                case 'u': {
                    /* Username */
                    const char* user = g_username ? g_username : "user";
                    while (*user && pos < SHELL_PROMPT_SIZE - 1) {
                        result[pos++] = *user++;
                    }
                    break;
                }
                case 'h': {
                    /* Hostname (short) */
                    const char* host = g_system_name ? g_system_name : "localhost";
                    while (*host && *host != '.' && pos < SHELL_PROMPT_SIZE - 1) {
                        result[pos++] = *host++;
                    }
                    break;
                }
                case 'H': {
                    /* Hostname (full) */
                    const char* host = g_system_name ? g_system_name : "localhost";
                    while (*host && pos < SHELL_PROMPT_SIZE - 1) {
                        result[pos++] = *host++;
                    }
                    break;
                }
                case 'w': {
                    /* Working directory (with ~ for home) */
                    char* cwd = get_current_directory();
                    if (cwd) {
                        const char* display = cwd;
                        if (g_home_directory && strncmp(cwd, g_home_directory, strlen(g_home_directory)) == 0) {
                            result[pos++] = '~';
                            display = cwd + strlen(g_home_directory);
                        }
                        while (*display && pos < SHELL_PROMPT_SIZE - 1) {
                            result[pos++] = *display++;
                        }
                        free(cwd);
                    }
                    break;
                }
                case 'W': {
                    /* Working directory (basename only) */
                    char* cwd = get_current_directory();
                    if (cwd) {
                        char* base = strrchr(cwd, '/');
                        const char* display = base ? base + 1 : cwd;
                        if (*display == '\0' && base == cwd) display = "/";
                        while (*display && pos < SHELL_PROMPT_SIZE - 1) {
                            result[pos++] = *display++;
                        }
                        free(cwd);
                    }
                    break;
                }
                case '$': {
                    /* $ for user, # for root */
                    result[pos++] = (getuid() == 0) ? '#' : '$';
                    break;
                }
                case 't': {
                    /* Time HH:MM:SS */
                    time_t now = time(NULL);
                    struct tm* tm = localtime(&now);
                    char timebuf[16];
                    snprintf(timebuf, sizeof(timebuf), "%02d:%02d:%02d",
                             tm->tm_hour, tm->tm_min, tm->tm_sec);
                    for (const char* t = timebuf; *t && pos < SHELL_PROMPT_SIZE - 1; t++) {
                        result[pos++] = *t;
                    }
                    break;
                }
                case 'T': {
                    /* Time HH:MM */
                    time_t now = time(NULL);
                    struct tm* tm = localtime(&now);
                    char timebuf[16];
                    snprintf(timebuf, sizeof(timebuf), "%02d:%02d",
                             tm->tm_hour, tm->tm_min);
                    for (const char* t = timebuf; *t && pos < SHELL_PROMPT_SIZE - 1; t++) {
                        result[pos++] = *t;
                    }
                    break;
                }
                case 'd': {
                    /* Date */
                    time_t now = time(NULL);
                    struct tm* tm = localtime(&now);
                    char datebuf[32];
                    strftime(datebuf, sizeof(datebuf), "%a %b %d", tm);
                    for (const char* d = datebuf; *d && pos < SHELL_PROMPT_SIZE - 1; d++) {
                        result[pos++] = *d;
                    }
                    break;
                }
                case 'n': {
                    result[pos++] = '\n';
                    break;
                }
                case 'r': {
                    result[pos++] = '\r';
                    break;
                }
                case 'e': {
                    result[pos++] = '\033';
                    break;
                }
                case 'a': {
                    result[pos++] = '\007';
                    break;
                }
                case '\\': {
                    result[pos++] = '\\';
                    break;
                }
                case 'v': {
                    /* Shell version (short) */
                    const char* ver = SHELL_VERSION;
                    while (*ver && *ver != '.' && pos < SHELL_PROMPT_SIZE - 1) {
                        result[pos++] = *ver++;
                    }
                    break;
                }
                case 'V': {
                    /* Shell version (full) */
                    const char* ver = SHELL_VERSION;
                    while (*ver && pos < SHELL_PROMPT_SIZE - 1) {
                        result[pos++] = *ver++;
                    }
                    break;
                }
                default:
                    result[pos++] = '\\';
                    if (pos < SHELL_PROMPT_SIZE - 1) {
                        result[pos++] = *p;
                    }
                    break;
            }
        } else {
            result[pos++] = *p;
        }
    }
    
    result[pos] = '\0';
    return result;
}

void shell_show_prompt(void) {
    char* prompt = shell_generate_prompt(g_ps1);
    printf("%s", prompt);
    fflush(stdout);
    free(prompt);
}
