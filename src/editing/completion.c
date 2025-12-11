#include "completion.h"
#include "colors.h"
#include "builtins.h"
#include "variables.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>

#define INITIAL_CAPACITY 32

static completion_result_t* result_create(void) {
    completion_result_t* result = malloc(sizeof(completion_result_t));
    if (!result) return NULL;
    
    result->completions = malloc(INITIAL_CAPACITY * sizeof(char*));
    if (!result->completions) {
        free(result);
        return NULL;
    }
    result->count = 0;
    result->capacity = INITIAL_CAPACITY;
    result->common_prefix = NULL;
    return result;
}

static void result_add(completion_result_t* result, const char* completion) {
    if (!result || !completion) return;
    
    if (result->count >= result->capacity) {
        int new_cap = result->capacity * 2;
        char** new_arr = realloc(result->completions, new_cap * sizeof(char*));
        if (!new_arr) return;
        result->completions = new_arr;
        result->capacity = new_cap;
    }
    
    result->completions[result->count++] = strdup(completion);
}

void completion_init(void) {
    /* Nothing to initialize currently */
}

void completion_cleanup(void) {
    /* Nothing to cleanup currently */
}

void completion_free(completion_result_t* result) {
    if (!result) return;
    
    for (int i = 0; i < result->count; i++) {
        free(result->completions[i]);
    }
    free(result->completions);
    free(result->common_prefix);
    free(result);
}

/* Compare for qsort */
static int cmp_strings(const void* a, const void* b) {
    return strcmp(*(const char**)a, *(const char**)b);
}

/* Find common prefix of all completions */
static char* find_common_prefix(completion_result_t* result) {
    if (!result || result->count == 0) return strdup("");
    if (result->count == 1) return strdup(result->completions[0]);
    
    const char* first = result->completions[0];
    size_t prefix_len = strlen(first);
    
    for (int i = 1; i < result->count; i++) {
        size_t j = 0;
        while (j < prefix_len && first[j] == result->completions[i][j]) {
            j++;
        }
        prefix_len = j;
    }
    
    char* prefix = malloc(prefix_len + 1);
    strncpy(prefix, first, prefix_len);
    prefix[prefix_len] = '\0';
    return prefix;
}

/* Get the word being completed and its start position */
static void get_word_to_complete(const char* line, int cursor, 
                                  char** word, int* word_start, int* is_first_word) {
    /* Find start of current word */
    int start = cursor;
    while (start > 0 && line[start - 1] != ' ' && line[start - 1] != '\t' &&
           line[start - 1] != '|' && line[start - 1] != ';' && 
           line[start - 1] != '&') {
        start--;
    }
    
    /* Check if this is the first word in a command */
    int ws = start;
    while (ws > 0 && (line[ws - 1] == ' ' || line[ws - 1] == '\t')) {
        ws--;
    }
    
    *is_first_word = (ws == 0);
    if (!*is_first_word) {
        /* Check if previous char is a command separator */
        if (ws > 0 && (line[ws - 1] == '|' || line[ws - 1] == ';' || line[ws - 1] == '&')) {
            *is_first_word = 1;
        }
    }
    
    /* Extract the word */
    int len = cursor - start;
    *word = malloc(len + 1);
    strncpy(*word, line + start, len);
    (*word)[len] = '\0';
    *word_start = start;
}

/* Complete file/directory paths */
static void complete_files(completion_result_t* result, const char* partial) {
    char* path_copy = strdup(partial);
    char* dir_path;
    char* prefix;
    
    char* last_slash = strrchr(path_copy, '/');
    if (last_slash) {
        *last_slash = '\0';
        dir_path = path_copy[0] ? path_copy : "/";
        prefix = last_slash + 1;
    } else {
        dir_path = ".";
        prefix = path_copy;
    }
    
    size_t prefix_len = strlen(prefix);
    
    DIR* dir = opendir(dir_path);
    if (!dir) {
        free(path_copy);
        return;
    }
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        /* Skip . and .. */
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        /* Skip hidden files unless prefix starts with . */
        if (entry->d_name[0] == '.' && prefix[0] != '.') {
            continue;
        }
        
        if (strncmp(entry->d_name, prefix, prefix_len) == 0) {
            /* Build full path for completion */
            char full_path[4096];
            if (last_slash) {
                if (path_copy[0]) {
                    snprintf(full_path, sizeof(full_path), "%s/%s", path_copy, entry->d_name);
                } else {
                    snprintf(full_path, sizeof(full_path), "/%s", entry->d_name);
                }
            } else {
                snprintf(full_path, sizeof(full_path), "%s", entry->d_name);
            }
            
            /* Check if it's a directory and add trailing slash */
            struct stat st;
            char check_path[4096];
            if (last_slash) {
                snprintf(check_path, sizeof(check_path), "%s/%s", 
                         strcmp(dir_path, "/") == 0 ? "" : dir_path, entry->d_name);
            } else {
                snprintf(check_path, sizeof(check_path), "%s", entry->d_name);
            }
            
            if (stat(check_path, &st) == 0 && S_ISDIR(st.st_mode)) {
                strcat(full_path, "/");
            }
            
            result_add(result, full_path);
        }
    }
    
    closedir(dir);
    free(path_copy);
}

/* Complete command names (builtins + executables in PATH) */
static void complete_commands(completion_result_t* result, const char* partial) {
    size_t partial_len = strlen(partial);
    
    /* Complete builtins */
    for (size_t i = 0; i < builtins_count; i++) {
        if (strncmp(builtins[i].name, partial, partial_len) == 0) {
            result_add(result, builtins[i].name);
        }
    }
    
    /* Complete executables from PATH */
    char* path = getenv("PATH");
    if (!path) return;
    
    char* path_copy = strdup(path);
    char* dir = strtok(path_copy, ":");
    
    while (dir) {
        DIR* dirp = opendir(dir);
        if (dirp) {
            struct dirent* entry;
            while ((entry = readdir(dirp)) != NULL) {
                if (strncmp(entry->d_name, partial, partial_len) == 0) {
                    /* Check if executable */
                    char full_path[4096];
                    snprintf(full_path, sizeof(full_path), "%s/%s", dir, entry->d_name);
                    
                    if (access(full_path, X_OK) == 0) {
                        /* Check if already in results */
                        int found = 0;
                        for (int i = 0; i < result->count; i++) {
                            if (strcmp(result->completions[i], entry->d_name) == 0) {
                                found = 1;
                                break;
                            }
                        }
                        if (!found) {
                            result_add(result, entry->d_name);
                        }
                    }
                }
            }
            closedir(dirp);
        }
        dir = strtok(NULL, ":");
    }
    
    free(path_copy);
}

/* Complete variable names */
static void complete_variables(completion_result_t* result, const char* partial) {
    /* Skip the $ */
    if (partial[0] == '$') partial++;
    size_t partial_len = strlen(partial);
    
    /* Add some common environment variables */
    const char* common_vars[] = {
        "HOME", "USER", "PATH", "PWD", "SHELL", "TERM", "EDITOR",
        "LANG", "LC_ALL", "PS1", "PS2", NULL
    };
    
    for (int i = 0; common_vars[i]; i++) {
        if (strncmp(common_vars[i], partial, partial_len) == 0) {
            char var_with_dollar[256];
            snprintf(var_with_dollar, sizeof(var_with_dollar), "$%s", common_vars[i]);
            result_add(result, var_with_dollar);
        }
    }
}

completion_result_t* get_completions(const char* line, int cursor_pos) {
    completion_result_t* result = result_create();
    if (!result) return NULL;
    
    char* word;
    int word_start;
    int is_first_word;
    
    get_word_to_complete(line, cursor_pos, &word, &word_start, &is_first_word);
    
    if (word[0] == '$') {
        /* Variable completion */
        complete_variables(result, word);
    } else if (is_first_word && strchr(word, '/') == NULL) {
        /* Command completion */
        complete_commands(result, word);
    } else {
        /* File completion */
        complete_files(result, word);
    }
    
    free(word);
    
    /* Sort results */
    if (result->count > 1) {
        qsort(result->completions, result->count, sizeof(char*), cmp_strings);
    }
    
    /* Find common prefix */
    result->common_prefix = find_common_prefix(result);
    
    return result;
}

int complete_line(char* buffer, int* length, int* cursor) {
    completion_result_t* result = get_completions(buffer, *cursor);
    if (!result) return 0;
    
    if (result->count == 0) {
        /* No completions - beep */
        write(STDOUT_FILENO, "\a", 1);
        completion_free(result);
        return 0;
    }
    
    /* Find word start */
    int word_start = *cursor;
    while (word_start > 0 && buffer[word_start - 1] != ' ' && 
           buffer[word_start - 1] != '\t') {
        word_start--;
    }
    
    int word_len = *cursor - word_start;
    
    if (result->count == 1) {
        /* Single completion - insert it */
        const char* completion = result->completions[0];
        int comp_len = strlen(completion);
        
        /* Replace word with completion */
        int new_length = *length - word_len + comp_len;
        if (new_length < 4096) {
            memmove(buffer + word_start + comp_len, 
                    buffer + *cursor, 
                    *length - *cursor + 1);
            memcpy(buffer + word_start, completion, comp_len);
            *length = new_length;
            *cursor = word_start + comp_len;
            
            /* Add space if not a directory */
            if (completion[comp_len - 1] != '/') {
                if (*length < 4095) {
                    memmove(buffer + *cursor + 1, buffer + *cursor, *length - *cursor + 1);
                    buffer[*cursor] = ' ';
                    (*length)++;
                    (*cursor)++;
                }
            }
        }
    } else {
        /* Multiple completions */
        const char* prefix = result->common_prefix;
        int prefix_len = strlen(prefix);
        
        if (prefix_len > word_len) {
            /* Complete to common prefix */
            int new_length = *length - word_len + prefix_len;
            if (new_length < 4096) {
                memmove(buffer + word_start + prefix_len, 
                        buffer + *cursor, 
                        *length - *cursor + 1);
                memcpy(buffer + word_start, prefix, prefix_len);
                *length = new_length;
                *cursor = word_start + prefix_len;
            }
        } else {
            /* Show all completions */
            write(STDOUT_FILENO, "\n", 1);
            
            int term_width = 80;
            int max_len = 0;
            for (int i = 0; i < result->count; i++) {
                int len = strlen(result->completions[i]);
                if (len > max_len) max_len = len;
            }
            
            int cols = term_width / (max_len + 2);
            if (cols < 1) cols = 1;
            
            for (int i = 0; i < result->count; i++) {
                printf("%-*s  ", max_len, result->completions[i]);
                if ((i + 1) % cols == 0 || i == result->count - 1) {
                    printf("\n");
                }
            }
        }
    }
    
    completion_free(result);
    return 1;
}
