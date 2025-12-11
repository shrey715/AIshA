#include "alias.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>

static alias_t* aliases[MAX_ALIASES];
static int alias_count = 0;

/* Hash function for alias names */
static unsigned int hash_alias(const char* name) {
    unsigned int hash = 5381;
    int c;
    while ((c = *name++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash % MAX_ALIASES;
}

/* Find an alias by name */
static alias_t* find_alias(const char* name) {
    unsigned int index = hash_alias(name);
    unsigned int start = index;
    
    while (aliases[index] != NULL) {
        if (strcmp(aliases[index]->name, name) == 0) {
            return aliases[index];
        }
        index = (index + 1) % MAX_ALIASES;
        if (index == start) break;
    }
    return NULL;
}

void alias_init(void) {
    memset(aliases, 0, sizeof(aliases));
    alias_count = 0;
}

void alias_cleanup(void) {
    for (int i = 0; i < MAX_ALIASES; i++) {
        if (aliases[i]) {
            free(aliases[i]->name);
            free(aliases[i]->value);
            free(aliases[i]);
            aliases[i] = NULL;
        }
    }
    alias_count = 0;
}

int set_alias(const char* name, const char* value) {
    if (!name || !value) return -1;
    
    /* Check for existing alias */
    alias_t* existing = find_alias(name);
    if (existing) {
        free(existing->value);
        existing->value = strdup(value);
        return 0;
    }
    
    /* Create new alias */
    if (alias_count >= MAX_ALIASES) {
        fprintf(stderr, "alias: too many aliases\n");
        return -1;
    }
    
    alias_t* alias = malloc(sizeof(alias_t));
    if (!alias) return -1;
    
    alias->name = strdup(name);
    alias->value = strdup(value);
    
    /* Insert into hash table */
    unsigned int index = hash_alias(name);
    while (aliases[index] != NULL) {
        index = (index + 1) % MAX_ALIASES;
    }
    aliases[index] = alias;
    alias_count++;
    
    return 0;
}

int unset_alias(const char* name) {
    if (!name) return -1;
    
    unsigned int index = hash_alias(name);
    unsigned int start = index;
    
    while (aliases[index] != NULL) {
        if (strcmp(aliases[index]->name, name) == 0) {
            free(aliases[index]->name);
            free(aliases[index]->value);
            free(aliases[index]);
            aliases[index] = NULL;
            alias_count--;
            return 0;
        }
        index = (index + 1) % MAX_ALIASES;
        if (index == start) break;
    }
    
    return -1;
}

const char* get_alias(const char* name) {
    alias_t* alias = find_alias(name);
    return alias ? alias->value : NULL;
}

int alias_exists(const char* name) {
    return find_alias(name) != NULL;
}

void list_aliases(void) {
    for (int i = 0; i < MAX_ALIASES; i++) {
        if (aliases[i]) {
            printf("alias %s='%s'\n", aliases[i]->name, aliases[i]->value);
        }
    }
}

char* expand_aliases(const char* input) {
    if (!input) return NULL;
    
    /* Copy input to work with */
    char* result = strdup(input);
    if (!result) return NULL;
    
    /* Skip leading whitespace */
    char* cmd_start = result;
    while (*cmd_start && isspace(*cmd_start)) cmd_start++;
    
    if (!*cmd_start) return result;
    
    /* Find end of first word (command) */
    char* cmd_end = cmd_start;
    while (*cmd_end && !isspace(*cmd_end)) cmd_end++;
    
    /* Extract command name */
    size_t cmd_len = cmd_end - cmd_start;
    char* cmd = malloc(cmd_len + 1);
    strncpy(cmd, cmd_start, cmd_len);
    cmd[cmd_len] = '\0';
    
    /* Check for alias */
    const char* alias_value = get_alias(cmd);
    if (alias_value) {
        /* Build new command with alias expanded */
        size_t prefix_len = cmd_start - result;
        size_t suffix_len = strlen(cmd_end);
        size_t alias_len = strlen(alias_value);
        
        char* new_result = malloc(prefix_len + alias_len + suffix_len + 1);
        if (new_result) {
            strncpy(new_result, result, prefix_len);
            strcpy(new_result + prefix_len, alias_value);
            strcpy(new_result + prefix_len + alias_len, cmd_end);
            free(result);
            result = new_result;
        }
    }
    
    free(cmd);
    return result;
}
