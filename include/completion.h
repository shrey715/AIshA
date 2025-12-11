#ifndef COMPLETION_H
#define COMPLETION_H

/* Completion types */
typedef enum {
    COMPLETE_COMMAND,     /* Command name completion (first word) */
    COMPLETE_FILE,        /* File/directory completion */
    COMPLETE_VARIABLE,    /* Variable name completion (after $)*/
    COMPLETE_HOSTNAME,    /* Hostname completion (after @) */
    COMPLETE_USERNAME     /* Username completion (after ~) */
} completion_type_t;

/* Completion result */
typedef struct {
    char** completions;
    int count;
    int capacity;
    char* common_prefix;   /* Common prefix of all completions */
} completion_result_t;

/* Initialize tab completion system */
void completion_init(void);
void completion_cleanup(void);

/* Get completions for a partial word */
completion_result_t* get_completions(const char* line, int cursor_pos);

/* Free completion results */
void completion_free(completion_result_t* result);

/* Perform completion on readline buffer (integrates with readline) */
int complete_line(char* buffer, int* length, int* cursor);

#endif /* COMPLETION_H */
