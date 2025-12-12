#include "variables.h"
#include "shell.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>

/* Global special variables */
int g_last_exit_status = 0;
pid_t g_shell_pid = 0;
pid_t g_last_background_pid = 0;
int g_arg_count = 0;
char** g_positional_args = NULL;

/* Saved positional args for function scope */
static int saved_arg_count = 0;
static char** saved_positional_args = NULL;

/* Variable hash table */
static shell_var_t* variables[MAX_VARIABLES];
static int variable_count = 0;

/* Simple hash function for variable names */
static unsigned int hash_name(const char* name) {
    unsigned int hash = 5381;
    int c;
    while ((c = *name++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash % MAX_VARIABLES;
}

/* Find a variable by name */
static shell_var_t* find_variable(const char* name) {
    unsigned int index = hash_name(name);
    unsigned int start = index;
    
    while (variables[index] != NULL) {
        if (strcmp(variables[index]->name, name) == 0) {
            return variables[index];
        }
        index = (index + 1) % MAX_VARIABLES;
        if (index == start) break;  /* Full loop */
    }
    return NULL;
}

void variables_init(void) {
    memset(variables, 0, sizeof(variables));
    variable_count = 0;
    g_shell_pid = getpid();
    
    /* Sync from environment */
    sync_from_environment();
    
    /* Set up $0 */
    g_positional_args = malloc(sizeof(char*));
    g_positional_args[0] = strdup("cshell");
    g_arg_count = 0;
}

void variables_cleanup(void) {
    for (int i = 0; i < MAX_VARIABLES; i++) {
        if (variables[i]) {
            free(variables[i]->name);
            free(variables[i]->value);
            free(variables[i]);
            variables[i] = NULL;
        }
    }
    variable_count = 0;
    
    if (g_positional_args) {
        for (int i = 0; i <= g_arg_count; i++) {
            free(g_positional_args[i]);
        }
        free(g_positional_args);
        g_positional_args = NULL;
    }
}

const char* get_variable(const char* name) {
    if (!name) return NULL;
    
    /* Handle special variables */
    static char buf[64];
    
    if (strcmp(name, "?") == 0) {
        snprintf(buf, sizeof(buf), "%d", g_last_exit_status);
        return buf;
    }
    if (strcmp(name, "$") == 0) {
        snprintf(buf, sizeof(buf), "%d", g_shell_pid);
        return buf;
    }
    if (strcmp(name, "!") == 0) {
        snprintf(buf, sizeof(buf), "%d", g_last_background_pid);
        return buf;
    }
    if (strcmp(name, "#") == 0) {
        snprintf(buf, sizeof(buf), "%d", g_arg_count);
        return buf;
    }
    if (strcmp(name, "0") == 0) {
        return g_positional_args ? g_positional_args[0] : "cshell";
    }
    
    /* Handle positional parameters $1, $2, etc. */
    if (isdigit(name[0]) && name[1] == '\0') {
        int index = name[0] - '0';
        if (index > 0 && index <= g_arg_count && g_positional_args) {
            return g_positional_args[index];
        }
        return "";
    }
    
    /* Handle $@ and $* */
    if (strcmp(name, "@") == 0 || strcmp(name, "*") == 0) {
        /* TODO: Return all positional args joined */
        return "";
    }
    
    /* Check shell variables first */
    shell_var_t* var = find_variable(name);
    if (var) {
        return var->value;
    }
    
    /* Fall back to environment */
    return getenv(name);
}

int set_variable(const char* name, const char* value, unsigned int flags) {
    if (!name || !value) return -1;
    
    /* Check if readonly */
    shell_var_t* existing = find_variable(name);
    if (existing && (existing->flags & VAR_FLAG_READONLY)) {
        fprintf(stderr, "%s: readonly variable\n", name);
        return -1;
    }
    
    if (existing) {
        /* Update existing variable */
        free(existing->value);
        existing->value = strdup(value);
        existing->flags |= flags;
    } else {
        /* Create new variable */
        if (variable_count >= MAX_VARIABLES) {
            fprintf(stderr, "Too many variables\n");
            return -1;
        }
        
        shell_var_t* var = malloc(sizeof(shell_var_t));
        if (!var) return -1;
        
        var->name = strdup(name);
        var->value = strdup(value);
        var->flags = flags;
        
        /* Insert into hash table */
        unsigned int index = hash_name(name);
        while (variables[index] != NULL) {
            index = (index + 1) % MAX_VARIABLES;
        }
        variables[index] = var;
        variable_count++;
    }
    
    /* If exported, also set in environment */
    if (flags & VAR_FLAG_EXPORTED) {
        setenv(name, value, 1);
    }
    
    return 0;
}

int unset_variable(const char* name) {
    if (!name) return -1;
    
    shell_var_t* var = find_variable(name);
    if (!var) {
        /* Also try to unset from environment */
        unsetenv(name);
        return 0;
    }
    
    if (var->flags & VAR_FLAG_READONLY) {
        fprintf(stderr, "%s: readonly variable\n", name);
        return -1;
    }
    
    /* Find and remove from hash table */
    unsigned int index = hash_name(name);
    while (variables[index] != NULL) {
        if (strcmp(variables[index]->name, name) == 0) {
            free(variables[index]->name);
            free(variables[index]->value);
            free(variables[index]);
            variables[index] = NULL;
            variable_count--;
            break;
        }
        index = (index + 1) % MAX_VARIABLES;
    }
    
    unsetenv(name);
    return 0;
}

int export_variable(const char* name) {
    if (!name) return -1;
    
    shell_var_t* var = find_variable(name);
    if (var) {
        var->flags |= VAR_FLAG_EXPORTED;
        setenv(name, var->value, 1);
    } else {
        /* Export with empty value if doesn't exist */
        set_variable(name, "", VAR_FLAG_EXPORTED);
    }
    return 0;
}

int is_variable_exported(const char* name) {
    shell_var_t* var = find_variable(name);
    return var ? (var->flags & VAR_FLAG_EXPORTED) : 0;
}

int is_variable_readonly(const char* name) {
    shell_var_t* var = find_variable(name);
    return var ? (var->flags & VAR_FLAG_READONLY) : 0;
}

int variable_exists(const char* name) {
    return find_variable(name) != NULL || getenv(name) != NULL;
}

void list_variables(int exported_only) {
    for (int i = 0; i < MAX_VARIABLES; i++) {
        if (variables[i]) {
            if (!exported_only || (variables[i]->flags & VAR_FLAG_EXPORTED)) {
                if (variables[i]->flags & VAR_FLAG_EXPORTED) {
                    printf("export ");
                }
                printf("%s=\"%s\"\n", variables[i]->name, variables[i]->value);
            }
        }
    }
}

/* Helper to check if character can be part of a variable name */
static int is_varname_char(char c) {
    return isalnum(c) || c == '_';
}

char* expand_variables(const char* input) {
    if (!input) return NULL;
    
    size_t input_len = strlen(input);
    size_t result_size = input_len * 2 + 1;  /* Initial size */
    char* result = malloc(result_size);
    if (!result) return NULL;
    
    size_t result_pos = 0;
    size_t i = 0;
    
    while (i < input_len) {
        if (input[i] == '\\' && i + 1 < input_len) {
            /* Escaped character - copy as is */
            if (result_pos + 2 >= result_size) {
                result_size *= 2;
                result = realloc(result, result_size);
            }
            result[result_pos++] = input[i++];
            result[result_pos++] = input[i++];
        } else if (input[i] == '$') {
            size_t consumed;
            char* expanded = expand_variable_reference(input + i, &consumed);
            if (expanded) {
                size_t exp_len = strlen(expanded);
                while (result_pos + exp_len >= result_size) {
                    result_size *= 2;
                    result = realloc(result, result_size);
                }
                strcpy(result + result_pos, expanded);
                result_pos += exp_len;
                free(expanded);
                i += consumed;
            } else {
                result[result_pos++] = input[i++];
            }
        } else {
            if (result_pos + 1 >= result_size) {
                result_size *= 2;
                result = realloc(result, result_size);
            }
            result[result_pos++] = input[i++];
        }
    }
    
    result[result_pos] = '\0';
    return result;
}

char* expand_variable_reference(const char* ref, size_t* consumed) {
    if (!ref || ref[0] != '$') {
        *consumed = 0;
        return NULL;
    }
    
    const char* start = ref + 1;  /* Skip $ */
    char varname[MAX_VAR_NAME_LENGTH] = {0};
    char* default_value = NULL;
    int use_default = 0;
    int assign_default = 0;
    
    if (*start == '{') {
        /* ${VAR} or ${VAR:-default} or ${VAR:=default} or ${#VAR} */
        start++;
        const char* end = strchr(start, '}');
        if (!end) {
            *consumed = 1;
            return strdup("$");
        }
        
        /* Check for ${#VAR} - length */
        int get_length = 0;
        if (*start == '#') {
            get_length = 1;
            start++;
        }
        
        /* Find variable name end or modifier */
        const char* p = start;
        while (p < end && is_varname_char(*p)) p++;
        
        size_t namelen = p - start;
        if (namelen >= MAX_VAR_NAME_LENGTH) namelen = MAX_VAR_NAME_LENGTH - 1;
        if (namelen > 0) memcpy(varname, start, namelen);
        varname[namelen] = '\0';
        
        /* Check for modifiers */
        if (p < end && *p == ':') {
            p++;
            if (*p == '-') {
                use_default = 1;
                p++;
                size_t deflen = end - p;
                default_value = malloc(deflen + 1);
                strncpy(default_value, p, deflen);
                default_value[deflen] = '\0';
            } else if (*p == '=') {
                assign_default = 1;
                p++;
                size_t deflen = end - p;
                default_value = malloc(deflen + 1);
                strncpy(default_value, p, deflen);
                default_value[deflen] = '\0';
            }
        }
        
        *consumed = (end - ref) + 1;  /* Include closing } */
        
        const char* value = get_variable(varname);
        
        if (get_length) {
            char lenbuf[32];
            snprintf(lenbuf, sizeof(lenbuf), "%zu", value ? strlen(value) : 0);
            if (default_value) free(default_value);
            return strdup(lenbuf);
        }
        
        if (!value || *value == '\0') {
            if (assign_default && default_value) {
                set_variable(varname, default_value, 0);
                char* ret = strdup(default_value);
                free(default_value);
                return ret;
            }
            if (use_default && default_value) {
                char* ret = strdup(default_value);
                free(default_value);
                return ret;
            }
            if (default_value) free(default_value);
            return strdup("");
        }
        
        if (default_value) free(default_value);
        return strdup(value);
        
    } else if (*start == '?' || *start == '$' || *start == '!' || 
               *start == '#' || *start == '@' || *start == '*' ||
               *start == '0') {
        /* Special single-character variables */
        varname[0] = *start;
        varname[1] = '\0';
        *consumed = 2;
        const char* value = get_variable(varname);
        return strdup(value ? value : "");
        
    } else if (isdigit(*start)) {
        /* Positional parameter $1, $2, etc. */
        varname[0] = *start;
        varname[1] = '\0';
        *consumed = 2;
        const char* value = get_variable(varname);
        return strdup(value ? value : "");
        
    } else if (is_varname_char(*start)) {
        /* Simple $VAR */
        const char* p = start;
        while (is_varname_char(*p)) p++;
        
        size_t namelen = p - start;
        if (namelen >= MAX_VAR_NAME_LENGTH) namelen = MAX_VAR_NAME_LENGTH - 1;
        strncpy(varname, start, namelen);
        varname[namelen] = '\0';
        
        *consumed = namelen + 1;  /* +1 for $ */
        
        const char* value = get_variable(varname);
        return strdup(value ? value : "");
    }
    
    /* Just a $ by itself */
    *consumed = 1;
    return strdup("$");
}

void set_positional_args(int argc, char** argv) {
    /* Free existing positional args */
    if (g_positional_args) {
        for (int i = 0; i <= g_arg_count; i++) {
            free(g_positional_args[i]);
        }
        free(g_positional_args);
    }
    
    g_arg_count = argc - 1;  /* $# doesn't include $0 */
    g_positional_args = malloc((argc + 1) * sizeof(char*));
    for (int i = 0; i < argc; i++) {
        g_positional_args[i] = strdup(argv[i]);
    }
    g_positional_args[argc] = NULL;
}

void save_positional_args(void) {
    saved_arg_count = g_arg_count;
    saved_positional_args = g_positional_args;
    g_positional_args = NULL;
    g_arg_count = 0;
}

void restore_positional_args(void) {
    if (g_positional_args) {
        for (int i = 0; i <= g_arg_count; i++) {
            free(g_positional_args[i]);
        }
        free(g_positional_args);
    }
    g_positional_args = saved_positional_args;
    g_arg_count = saved_arg_count;
    saved_positional_args = NULL;
    saved_arg_count = 0;
}

void update_exit_status(int status) {
    g_last_exit_status = status;
}

void update_last_background_pid(pid_t pid) {
    g_last_background_pid = pid;
}

void sync_from_environment(void) {
    extern char** environ;
    for (char** env = environ; *env; env++) {
        char* eq = strchr(*env, '=');
        if (eq) {
            size_t namelen = eq - *env;
            char* name = malloc(namelen + 1);
            strncpy(name, *env, namelen);
            name[namelen] = '\0';
            set_variable(name, eq + 1, VAR_FLAG_EXPORTED);
            free(name);
        }
    }
}

void sync_to_environment(void) {
    for (int i = 0; i < MAX_VARIABLES; i++) {
        if (variables[i] && (variables[i]->flags & VAR_FLAG_EXPORTED)) {
            setenv(variables[i]->name, variables[i]->value, 1);
        }
    }
}
