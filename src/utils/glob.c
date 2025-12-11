#include "glob.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#define INITIAL_CAPACITY 16

glob_result_t* glob_create(void) {
    glob_result_t* result = malloc(sizeof(glob_result_t));
    if (!result) return NULL;
    
    result->matches = malloc(INITIAL_CAPACITY * sizeof(char*));
    if (!result->matches) {
        free(result);
        return NULL;
    }
    
    result->count = 0;
    result->capacity = INITIAL_CAPACITY;
    return result;
}

void glob_free(glob_result_t* result) {
    if (!result) return;
    
    for (int i = 0; i < result->count; i++) {
        free(result->matches[i]);
    }
    free(result->matches);
    free(result);
}

void glob_add_match(glob_result_t* result, const char* match) {
    if (!result || !match) return;
    
    if (result->count >= result->capacity) {
        int new_capacity = result->capacity * 2;
        char** new_matches = realloc(result->matches, new_capacity * sizeof(char*));
        if (!new_matches) return;
        result->matches = new_matches;
        result->capacity = new_capacity;
    }
    
    result->matches[result->count++] = strdup(match);
}

int has_glob_chars(const char* str) {
    if (!str) return 0;
    
    while (*str) {
        if (*str == '*' || *str == '?' || *str == '[') {
            return 1;
        }
        str++;
    }
    return 0;
}

/* Internal pattern matching function */
static int match_pattern(const char* pattern, const char* str) {
    while (*pattern && *str) {
        if (*pattern == '*') {
            /* Skip consecutive stars */
            while (*pattern == '*') pattern++;
            
            if (!*pattern) return 1;  /* Trailing * matches everything */
            
            /* Try matching rest of pattern at each position */
            while (*str) {
                if (match_pattern(pattern, str)) return 1;
                str++;
            }
            return match_pattern(pattern, str);
            
        } else if (*pattern == '?') {
            /* ? matches any single character except nothing */
            if (!*str) return 0;
            pattern++;
            str++;
            
        } else if (*pattern == '[') {
            /* Character class */
            pattern++;
            int negated = 0;
            int matched = 0;
            
            if (*pattern == '!' || *pattern == '^') {
                negated = 1;
                pattern++;
            }
            
            while (*pattern && *pattern != ']') {
                if (pattern[1] == '-' && pattern[2] && pattern[2] != ']') {
                    /* Range: [a-z] */
                    if (*str >= pattern[0] && *str <= pattern[2]) {
                        matched = 1;
                    }
                    pattern += 3;
                } else {
                    if (*str == *pattern) {
                        matched = 1;
                    }
                    pattern++;
                }
            }
            
            if (*pattern == ']') pattern++;
            
            if (negated ? matched : !matched) return 0;
            str++;
            
        } else {
            /* Literal character match */
            if (*pattern != *str) return 0;
            pattern++;
            str++;
        }
    }
    
    /* Skip trailing stars */
    while (*pattern == '*') pattern++;
    
    return !*pattern && !*str;
}

int glob_match(const char* pattern, const char* str) {
    if (!pattern || !str) return 0;
    return match_pattern(pattern, str);
}

/* Compare function for sorting */
static int compare_strings(const void* a, const void* b) {
    return strcmp(*(const char**)a, *(const char**)b);
}

glob_result_t* glob_expand(const char* pattern) {
    if (!pattern) return NULL;
    
    glob_result_t* result = glob_create();
    if (!result) return NULL;
    
    /* Find directory and pattern parts */
    char* pattern_copy = strdup(pattern);
    char* last_slash = strrchr(pattern_copy, '/');
    
    char* dir_path;
    char* file_pattern;
    
    if (last_slash) {
        *last_slash = '\0';
        dir_path = pattern_copy[0] ? pattern_copy : "/";
        file_pattern = last_slash + 1;
    } else {
        dir_path = ".";
        file_pattern = pattern_copy;
    }
    
    /* Check if pattern has glob chars */
    if (!has_glob_chars(file_pattern)) {
        /* No glob chars - just check if file exists */
        struct stat st;
        if (stat(pattern, &st) == 0) {
            glob_add_match(result, pattern);
        }
        free(pattern_copy);
        return result;
    }
    
    /* Open directory and match files */
    DIR* dir = opendir(dir_path);
    if (!dir) {
        free(pattern_copy);
        return result;
    }
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        /* Skip hidden files unless pattern starts with . */
        if (entry->d_name[0] == '.' && file_pattern[0] != '.') {
            continue;
        }
        
        if (glob_match(file_pattern, entry->d_name)) {
            /* Build full path */
            char full_path[4096];
            if (last_slash) {
                snprintf(full_path, sizeof(full_path), "%s/%s", 
                         pattern_copy[0] ? pattern_copy : "", entry->d_name);
            } else {
                snprintf(full_path, sizeof(full_path), "%s", entry->d_name);
            }
            glob_add_match(result, full_path);
        }
    }
    
    closedir(dir);
    free(pattern_copy);
    
    /* Sort results */
    if (result->count > 1) {
        qsort(result->matches, result->count, sizeof(char*), compare_strings);
    }
    
    return result;
}

char** expand_glob_args(char** args, int* argc) {
    if (!args || !argc || *argc == 0) return args;
    
    /* Calculate maximum possible size */
    int max_args = *argc * 100;  /* Assume max 100 matches per pattern */
    char** new_args = malloc(max_args * sizeof(char*));
    if (!new_args) return args;
    
    int new_argc = 0;
    
    for (int i = 0; i < *argc && new_argc < max_args; i++) {
        if (has_glob_chars(args[i])) {
            glob_result_t* matches = glob_expand(args[i]);
            
            if (matches && matches->count > 0) {
                /* Add all matches */
                for (int j = 0; j < matches->count && new_argc < max_args; j++) {
                    new_args[new_argc++] = strdup(matches->matches[j]);
                }
            } else {
                /* No matches - keep original (or could error) */
                new_args[new_argc++] = strdup(args[i]);
            }
            
            glob_free(matches);
        } else {
            new_args[new_argc++] = strdup(args[i]);
        }
    }
    
    new_args[new_argc] = NULL;
    *argc = new_argc;
    
    return new_args;
}
