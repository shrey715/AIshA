/**
 * @file builtins_fs.c
 * @brief Filesystem-related builtin commands
 * 
 * Implements: hop/cd, reveal/ls, source/.
 */

#include "builtins.h"
#include "background.h"
#include "colors.h"
#include "directory.h"
#include "execute.h"
#include "variables.h"
#include <time.h>
#include <grp.h>

/* Previous directory for cd - */
static char* g_previous_directory = NULL;

/*============================================================================
 * Directory Navigation (hop/cd)
 *============================================================================*/

/**
 * hop/cd - Change current working directory
 */
int builtin_hop(char** args, int argc) {
    char* target_directory = NULL;

    if (argc == 1) {
        target_directory = g_home_directory;
        char* previous_directory = get_current_directory();

        if (chdir(target_directory) != 0) {
            print_error("hop: %s: No such directory\n", target_directory);
            free(previous_directory);
            return 1;
        }
        if (g_previous_directory) free(g_previous_directory);
        g_previous_directory = previous_directory;
    } else {
        for (int i = 1; i < argc; i++) {
            char* previous_directory = get_current_directory();

            if (strcmp(args[i], "~") == 0) {
                target_directory = g_home_directory;
            } else if (strcmp(args[i], ".") == 0) {
                target_directory = get_current_directory();
            } else if (strcmp(args[i], "..") == 0) {
                target_directory = get_parent_directory();
            } else if (strcmp(args[i], "-") == 0) {
                target_directory = g_previous_directory;
                if (target_directory) {
                    printf("%s\n", target_directory);
                }
            } else {
                target_directory = args[i];
            }

            if (!target_directory || chdir(target_directory) != 0) {
                print_error("hop: %s: No such directory\n", args[i]);
                free(previous_directory);
                return 1;
            } else {
                if (g_previous_directory) free(g_previous_directory);
                g_previous_directory = previous_directory;
            }
        }
    }
    return 0;
}

int builtin_cd(char** args, int argc) {
    return builtin_hop(args, argc);
}

void cleanup_hop(void) {
    if (g_previous_directory) {
        free(g_previous_directory);
        g_previous_directory = NULL;
    }
}

/*============================================================================
 * File Listing (reveal/ls)
 *============================================================================*/

static int compare_entries(const void* a, const void* b) {
    return strcmp(*(const char**)a, *(const char**)b);
}

static void print_permissions(mode_t mode) {
    char perms[11];
    
    perms[0] = S_ISDIR(mode) ? 'd' : S_ISLNK(mode) ? 'l' : S_ISCHR(mode) ? 'c' : 
               S_ISBLK(mode) ? 'b' : S_ISFIFO(mode) ? 'p' : S_ISSOCK(mode) ? 's' : '-';
    perms[1] = (mode & S_IRUSR) ? 'r' : '-';
    perms[2] = (mode & S_IWUSR) ? 'w' : '-';
    perms[3] = (mode & S_IXUSR) ? ((mode & S_ISUID) ? 's' : 'x') : ((mode & S_ISUID) ? 'S' : '-');
    perms[4] = (mode & S_IRGRP) ? 'r' : '-';
    perms[5] = (mode & S_IWGRP) ? 'w' : '-';
    perms[6] = (mode & S_IXGRP) ? ((mode & S_ISGID) ? 's' : 'x') : ((mode & S_ISGID) ? 'S' : '-');
    perms[7] = (mode & S_IROTH) ? 'r' : '-';
    perms[8] = (mode & S_IWOTH) ? 'w' : '-';
    perms[9] = (mode & S_IXOTH) ? ((mode & S_ISVTX) ? 't' : 'x') : ((mode & S_ISVTX) ? 'T' : '-');
    perms[10] = '\0';
    
    printf("%s", perms);
}

static void print_size_human(off_t size) {
    const char* units[] = {"B", "K", "M", "G", "T"};
    int unit = 0;
    double dsize = size;
    
    while (dsize >= 1024 && unit < 4) {
        dsize /= 1024;
        unit++;
    }
    
    if (unit == 0) {
        printf("%5lld ", (long long)size);
    } else {
        printf("%4.1f%s ", dsize, units[unit]);
    }
}

/**
 * reveal/ls - List directory contents
 */
int builtin_reveal(char** args, int argc) {
    int show_all = 0;
    int long_format = 0;
    int human_readable = 0;
    char* target_directory = ".";
    int arg_count = 0;
    
    for (int i = 1; i < argc; i++) {
        if (args[i][0] == '-') {
            for (size_t j = 1; args[i][j] != '\0'; j++) {
                switch (args[i][j]) {
                    case 'a': show_all = 1; break;
                    case 'l': long_format = 1; break;
                    case 'h': human_readable = 1; break;
                    default:
                        print_error("reveal: invalid option -- '%c'\n", args[i][j]);
                        return 1;
                }
            }
        } else {
            arg_count++;
            if (arg_count > 1) {
                print_error("reveal: too many arguments\n");
                return 1;
            }
            target_directory = args[i];
        }
    }

    /* Resolve special paths */
    char* resolved_path = NULL;
    if (strcmp(target_directory, "~") == 0) {
        resolved_path = strdup(g_home_directory);
    } else if (strcmp(target_directory, "-") == 0) {
        if (!g_previous_directory) {
            print_error("reveal: -: No such directory\n");
            return 1;
        }
        resolved_path = strdup(g_previous_directory);
    } else {
        resolved_path = strdup(target_directory);
    }
    
    if (!resolved_path) {
        print_error("reveal: memory allocation error\n");
        return 1;
    }

    DIR* dir = opendir(resolved_path);
    if (!dir) {
        print_error("reveal: cannot access '%s': %s\n", resolved_path, strerror(errno));
        free(resolved_path);
        return 1;
    }

    /* Count and collect entries */
    struct dirent* entry;
    int entry_cnt = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (!show_all && entry->d_name[0] == '.') continue;
        entry_cnt++;
    }

    char** entries = malloc(entry_cnt * sizeof(char*));
    if (!entries) {
        print_error("reveal: memory allocation error\n");
        closedir(dir);
        free(resolved_path);
        return 1;
    }

    rewinddir(dir);
    int idx = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (!show_all && entry->d_name[0] == '.') continue;
        entries[idx++] = strdup(entry->d_name);
    }

    qsort(entries, entry_cnt, sizeof(char*), compare_entries);

    /* Print entries */
    for (int i = 0; i < entry_cnt; i++) {
        char full_path[4096];
        snprintf(full_path, sizeof(full_path), "%s/%s", resolved_path, entries[i]);
        
        struct stat st;
        int stat_ok = (lstat(full_path, &st) == 0);
        
        if (long_format) {
            if (stat_ok) {
                print_permissions(st.st_mode);
                printf(" %3lu", (unsigned long)st.st_nlink);
                
                struct passwd* pw = getpwuid(st.st_uid);
                struct group* gr = getgrgid(st.st_gid);
                printf(" %-8s %-8s", pw ? pw->pw_name : "?", gr ? gr->gr_name : "?");
                
                if (human_readable) {
                    print_size_human(st.st_size);
                } else {
                    printf(" %8lld ", (long long)st.st_size);
                }
                
                char timebuf[64];
                struct tm* tm = localtime(&st.st_mtime);
                strftime(timebuf, sizeof(timebuf), "%b %d %H:%M", tm);
                printf("%s ", timebuf);
            } else {
                printf("?????????? ? ? ? ? ?     ?         ");
            }
        }
        
        /* Print colored filename */
        const char* color = "";
        if (COLORS_SUPPORTED && stat_ok) {
            color = get_file_color(st.st_mode, entries[i]);
        }
        
        if (COLORS_SUPPORTED) {
            printf("%s%s%s", color, entries[i], COLOR_RESET);
        } else {
            printf("%s", entries[i]);
        }
        
        if (long_format) {
            printf("\n");
        } else {
            printf("  ");
        }
        
        free(entries[i]);
    }

    if (!long_format && entry_cnt > 0) {
        printf("\n");
    }

    free(entries);
    closedir(dir);
    free(resolved_path);
    return 0;
}

int builtin_ls(char** args, int argc) {
    return builtin_reveal(args, argc);
}

/*============================================================================
 * Script Execution (source/.)
 *============================================================================*/

/**
 * source/. - Execute commands from a file
 */
int builtin_source(char** args, int argc) {
    if (argc < 2) {
        print_error("source: usage: source FILENAME [ARGS...]\n");
        return 1;
    }
    
    FILE* file = fopen(args[1], "r");
    if (!file) {
        print_error("source: %s: %s\n", args[1], strerror(errno));
        return 1;
    }
    
    char line[4096];
    int last_status = 0;
    
    while (fgets(line, sizeof(line), file)) {
        /* Skip empty lines and comments */
        char* p = line;
        while (*p && isspace(*p)) p++;
        if (!*p || *p == '#') continue;
        
        /* Remove trailing newline */
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }
        
        token_t* tokens = malloc(MAX_TOKENS * sizeof(token_t));
        if (!tokens) continue;
        
        int token_count = tokenize_input(line, tokens, MAX_TOKENS);
        if (token_count > 0) {
            last_status = execute_shell_command_with_operators(tokens, token_count);
        }
        free(tokens);
    }
    
    fclose(file);
    return last_status;
}

int builtin_dot(char** args, int argc) {
    return builtin_source(args, argc);
}
