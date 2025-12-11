#ifndef GLOB_H
#define GLOB_H

/* Glob expansion results */
typedef struct {
    char** matches;
    int count;
    int capacity;
} glob_result_t;

/* Initialize and cleanup glob results */
glob_result_t* glob_create(void);
void glob_free(glob_result_t* result);
void glob_add_match(glob_result_t* result, const char* match);

/* Expand a glob pattern, returns matches */
glob_result_t* glob_expand(const char* pattern);

/* Check if a string contains glob characters */
int has_glob_chars(const char* str);

/* Match a pattern against a string (returns 1 if match, 0 otherwise) */
int glob_match(const char* pattern, const char* str);

/* Expand all glob patterns in an argument list */
char** expand_glob_args(char** args, int* argc);

#endif /* GLOB_H */
