#ifndef ALIAS_H
#define ALIAS_H

#include <stdlib.h>

/* Maximum number of aliases */
#define MAX_ALIASES 256
#define MAX_ALIAS_NAME_LENGTH 64
#define MAX_ALIAS_VALUE_LENGTH 1024

/* Alias entry structure */
typedef struct {
    char* name;
    char* value;
} alias_t;

/* Initialize alias system */
void alias_init(void);
void alias_cleanup(void);

/* Alias management */
int set_alias(const char* name, const char* value);
int unset_alias(const char* name);
const char* get_alias(const char* name);
int alias_exists(const char* name);

/* List all aliases */
void list_aliases(void);

/* Expand aliases in input (returns newly allocated string) */
char* expand_aliases(const char* input);

#endif /* ALIAS_H */
