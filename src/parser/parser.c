#include "parser.h"
#include "variables.h"
#include "alias.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Token type to string conversion */
const char* token_type_name(token_type_t type) {
    switch (type) {
        case TOKEN_WORD: return "WORD";
        case TOKEN_PIPE: return "PIPE";
        case TOKEN_SEMICOLON: return "SEMICOLON";
        case TOKEN_AMPERSAND: return "AMPERSAND";
        case TOKEN_AND: return "AND";
        case TOKEN_OR: return "OR";
        case TOKEN_INPUT_REDIRECT: return "INPUT_REDIRECT";
        case TOKEN_OUTPUT_REDIRECT: return "OUTPUT_REDIRECT";
        case TOKEN_OUTPUT_APPEND: return "OUTPUT_APPEND";
        case TOKEN_HEREDOC: return "HEREDOC";
        case TOKEN_HERESTRING: return "HERESTRING";
        case TOKEN_LPAREN: return "LPAREN";
        case TOKEN_RPAREN: return "RPAREN";
        case TOKEN_NEWLINE: return "NEWLINE";
        case TOKEN_EOF: return "EOF";
        default: return "UNKNOWN";
    }
}

int is_operator_token(token_type_t type) {
    return type == TOKEN_PIPE || type == TOKEN_SEMICOLON ||
           type == TOKEN_AMPERSAND || type == TOKEN_AND || type == TOKEN_OR;
}

int is_redirect_token(token_type_t type) {
    return type == TOKEN_INPUT_REDIRECT || type == TOKEN_OUTPUT_REDIRECT ||
           type == TOKEN_OUTPUT_APPEND || type == TOKEN_HEREDOC ||
           type == TOKEN_HERESTRING;
}

/* Pre-process input: expand aliases and variables */
char* preprocess_input(const char* input) {
    if (!input) return NULL;
    
    /* First expand aliases */
    char* alias_expanded = expand_aliases(input);
    if (!alias_expanded) return strdup(input);
    
    /* Then expand variables */
    char* var_expanded = expand_variables(alias_expanded);
    free(alias_expanded);
    
    return var_expanded ? var_expanded : strdup(input);
}

/* Tokenizer implementation with quote handling */
int tokenize_input(const char* input, token_t tokens[], int max_tokens) {
    int token_cnt = 0;
    const char* curr = input;
    
    while (*curr != '\0' && token_cnt < max_tokens - 1) {
        /* Skip whitespace */
        while (isspace(*curr) && *curr != '\n') curr++;
        
        if (*curr == '\0') break;
        
        /* Handle newline as token (for multiline commands) */
        if (*curr == '\n') {
            tokens[token_cnt].type = TOKEN_NEWLINE;
            snprintf(tokens[token_cnt].value, MAX_TOKEN_LENGTH, "\\n");
            tokens[token_cnt].quoted = 0;
            token_cnt++;
            curr++;
            continue;
        }
        
        /* Handle comments */
        if (*curr == '#') {
            while (*curr && *curr != '\n') curr++;
            continue;
        }
        
        /* Handle operators and special characters */
        if (*curr == '|') {
            if (*(curr + 1) == '|') {
                tokens[token_cnt].type = TOKEN_OR;
                snprintf(tokens[token_cnt].value, MAX_TOKEN_LENGTH, "||");
                curr += 2;
            } else {
                tokens[token_cnt].type = TOKEN_PIPE;
                snprintf(tokens[token_cnt].value, MAX_TOKEN_LENGTH, "|");
                curr++;
            }
            tokens[token_cnt].quoted = 0;
            token_cnt++;
        } else if (*curr == '&') {
            if (*(curr + 1) == '&') {
                tokens[token_cnt].type = TOKEN_AND;
                snprintf(tokens[token_cnt].value, MAX_TOKEN_LENGTH, "&&");
                curr += 2;
            } else {
                tokens[token_cnt].type = TOKEN_AMPERSAND;
                snprintf(tokens[token_cnt].value, MAX_TOKEN_LENGTH, "&");
                curr++;
            }
            tokens[token_cnt].quoted = 0;
            token_cnt++;
        } else if (*curr == ';') {
            tokens[token_cnt].type = TOKEN_SEMICOLON;
            snprintf(tokens[token_cnt].value, MAX_TOKEN_LENGTH, ";");
            tokens[token_cnt].quoted = 0;
            token_cnt++;
            curr++;
        } else if (*curr == '(') {
            tokens[token_cnt].type = TOKEN_LPAREN;
            snprintf(tokens[token_cnt].value, MAX_TOKEN_LENGTH, "(");
            tokens[token_cnt].quoted = 0;
            token_cnt++;
            curr++;
        } else if (*curr == ')') {
            tokens[token_cnt].type = TOKEN_RPAREN;
            snprintf(tokens[token_cnt].value, MAX_TOKEN_LENGTH, ")");
            tokens[token_cnt].quoted = 0;
            token_cnt++;
            curr++;
        } else if (*curr == '<') {
            if (*(curr + 1) == '<') {
                if (*(curr + 2) == '<') {
                    tokens[token_cnt].type = TOKEN_HERESTRING;
                    snprintf(tokens[token_cnt].value, MAX_TOKEN_LENGTH, "<<<");
                    curr += 3;
                } else {
                    tokens[token_cnt].type = TOKEN_HEREDOC;
                    snprintf(tokens[token_cnt].value, MAX_TOKEN_LENGTH, "<<");
                    curr += 2;
                }
            } else {
                tokens[token_cnt].type = TOKEN_INPUT_REDIRECT;
                snprintf(tokens[token_cnt].value, MAX_TOKEN_LENGTH, "<");
                curr++;
            }
            tokens[token_cnt].quoted = 0;
            token_cnt++;
        } else if (*curr == '>') {
            if (*(curr + 1) == '>') {
                tokens[token_cnt].type = TOKEN_OUTPUT_APPEND;
                snprintf(tokens[token_cnt].value, MAX_TOKEN_LENGTH, ">>");
                curr += 2;
            } else {
                tokens[token_cnt].type = TOKEN_OUTPUT_REDIRECT;
                snprintf(tokens[token_cnt].value, MAX_TOKEN_LENGTH, ">");
                curr++;
            }
            tokens[token_cnt].quoted = 0;
            token_cnt++;
        } else if (*curr == '"' || *curr == '\'') {
            /* Quoted string */
            char quote = *curr++;
            int len = 0;
            int is_double = (quote == '"');
            
            while (*curr && *curr != quote && len < MAX_TOKEN_LENGTH - 1) {
                if (*curr == '\\' && is_double && *(curr + 1)) {
                    curr++;
                    switch (*curr) {
                        case 'n': tokens[token_cnt].value[len++] = '\n'; break;
                        case 't': tokens[token_cnt].value[len++] = '\t'; break;
                        case 'r': tokens[token_cnt].value[len++] = '\r'; break;
                        case '\\': tokens[token_cnt].value[len++] = '\\'; break;
                        case '"': tokens[token_cnt].value[len++] = '"'; break;
                        case '$': tokens[token_cnt].value[len++] = '$'; break;
                        case '`': tokens[token_cnt].value[len++] = '`'; break;
                        default:
                            tokens[token_cnt].value[len++] = '\\';
                            tokens[token_cnt].value[len++] = *curr;
                            break;
                    }
                    curr++;
                } else {
                    tokens[token_cnt].value[len++] = *curr++;
                }
            }
            
            tokens[token_cnt].value[len] = '\0';
            tokens[token_cnt].type = TOKEN_WORD;
            tokens[token_cnt].quoted = 1;
            token_cnt++;
            
            if (*curr == quote) curr++;  /* Skip closing quote */
        } else if (!isspace(*curr)) {
            /* Regular word */
            int len = 0;
            
            while (*curr != '\0' && !isspace(*curr) && len < MAX_TOKEN_LENGTH - 1) {
                /* Stop at operators */
                if (*curr == '|' || *curr == '&' || *curr == ';' ||
                    *curr == '<' || *curr == '>' || *curr == '(' || 
                    *curr == ')' || *curr == '#') {
                    break;
                }
                
                /* Handle backslash escaping in unquoted context */
                if (*curr == '\\' && *(curr + 1)) {
                    curr++;
                    tokens[token_cnt].value[len++] = *curr++;
                } else {
                    tokens[token_cnt].value[len++] = *curr++;
                }
            }
            
            tokens[token_cnt].value[len] = '\0';
            tokens[token_cnt].type = TOKEN_WORD;
            tokens[token_cnt].quoted = 0;
            token_cnt++;
        }
    }
    
    /* Add EOF token */
    if (token_cnt < max_tokens) {
        tokens[token_cnt].type = TOKEN_EOF;
        tokens[token_cnt].value[0] = '\0';
        tokens[token_cnt].quoted = 0;
        token_cnt++;
    }
    
    return token_cnt;
}

/* Token validation helpers */
static int is_token_type(const token_t* token, token_type_t type) {
    return token->type == type;
}

static int is_at_end(const token_t tokens[], int pos, int token_count) {
    return pos >= token_count || tokens[pos].type == TOKEN_EOF || 
           tokens[pos].type == TOKEN_NEWLINE;
}

/* Grammar validation functions */
static int validate_name(const token_t tokens[], int* pos, int token_count) {
    if (is_at_end(tokens, *pos, token_count)) {
        return PARSE_SYNTAX_ERROR;
    }
    
    const token_t* token = &tokens[*pos];
    if (!is_token_type(token, TOKEN_WORD)) {
        return PARSE_SYNTAX_ERROR;
    }
    
    (*pos)++;
    return PARSE_SUCCESS;
}

static int validate_input(const token_t tokens[], int* pos, int token_count) {
    if (is_at_end(tokens, *pos, token_count)) {
        return PARSE_SYNTAX_ERROR;
    }
    
    const token_t* token = &tokens[*pos];
    if (!is_token_type(token, TOKEN_INPUT_REDIRECT)) {
        return PARSE_SYNTAX_ERROR;
    }
    
    (*pos)++;
    return validate_name(tokens, pos, token_count);
}

static int validate_output(const token_t tokens[], int* pos, int token_count) {
    if (is_at_end(tokens, *pos, token_count)) {
        return PARSE_SYNTAX_ERROR;
    }
    
    const token_t* token = &tokens[*pos];
    if (!is_token_type(token, TOKEN_OUTPUT_REDIRECT) && 
        !is_token_type(token, TOKEN_OUTPUT_APPEND)) {
        return PARSE_SYNTAX_ERROR;
    }
    
    (*pos)++;
    return validate_name(tokens, pos, token_count);
}

static int validate_atomic(const token_t tokens[], int* pos, int token_count) {
    if (is_at_end(tokens, *pos, token_count)) {
        return PARSE_SYNTAX_ERROR;
    }
    
    if (validate_name(tokens, pos, token_count) != PARSE_SUCCESS) {
        return PARSE_SYNTAX_ERROR;
    }
    
    while (!is_at_end(tokens, *pos, token_count)) {
        const token_t* token = &tokens[*pos];
        
        if (is_operator_token(token->type) || 
            is_token_type(token, TOKEN_LPAREN) ||
            is_token_type(token, TOKEN_RPAREN)) {
            break;
        }
        
        if (is_token_type(token, TOKEN_INPUT_REDIRECT)) {
            if (validate_input(tokens, pos, token_count) != PARSE_SUCCESS) {
                return PARSE_SYNTAX_ERROR;
            }
        } else if (is_token_type(token, TOKEN_OUTPUT_REDIRECT) || 
                   is_token_type(token, TOKEN_OUTPUT_APPEND)) {
            if (validate_output(tokens, pos, token_count) != PARSE_SUCCESS) {
                return PARSE_SYNTAX_ERROR;
            }
        } else if (is_token_type(token, TOKEN_WORD)) {
            if (validate_name(tokens, pos, token_count) != PARSE_SUCCESS) {
                return PARSE_SYNTAX_ERROR;
            }
        } else {
            return PARSE_SYNTAX_ERROR;
        }
    }
    
    return PARSE_SUCCESS;
}

static int validate_pipeline(const token_t tokens[], int* pos, int token_count) {
    if (!is_at_end(tokens, *pos, token_count) && 
        is_token_type(&tokens[*pos], TOKEN_PIPE)) {
        return PARSE_SYNTAX_ERROR;
    }
    
    if (validate_atomic(tokens, pos, token_count) != PARSE_SUCCESS) {
        return PARSE_SYNTAX_ERROR;
    }
    
    while (!is_at_end(tokens, *pos, token_count)) {
        const token_t* token = &tokens[*pos];
        
        if (is_token_type(token, TOKEN_PIPE)) {
            (*pos)++;
            
            if (is_at_end(tokens, *pos, token_count) || 
                !is_token_type(&tokens[*pos], TOKEN_WORD)) {
                return PARSE_SYNTAX_ERROR;
            }
            
            if (validate_atomic(tokens, pos, token_count) != PARSE_SUCCESS) {
                return PARSE_SYNTAX_ERROR;
            }
        } else {
            break;
        }
    }
    
    return PARSE_SUCCESS;
}

static int validate_and_or(const token_t tokens[], int* pos, int token_count) {
    if (validate_pipeline(tokens, pos, token_count) != PARSE_SUCCESS) {
        return PARSE_SYNTAX_ERROR;
    }
    
    while (!is_at_end(tokens, *pos, token_count)) {
        const token_t* token = &tokens[*pos];
        
        if (is_token_type(token, TOKEN_AND) || is_token_type(token, TOKEN_OR)) {
            (*pos)++;
            
            if (is_at_end(tokens, *pos, token_count)) {
                return PARSE_SYNTAX_ERROR;
            }
            
            if (validate_pipeline(tokens, pos, token_count) != PARSE_SUCCESS) {
                return PARSE_SYNTAX_ERROR;
            }
        } else {
            break;
        }
    }
    
    return PARSE_SUCCESS;
}

static int validate_shell_cmd(const token_t tokens[], int token_count) {
    int pos = 0;
    
    if (is_at_end(tokens, pos, token_count)) {
        return PARSE_SUCCESS;
    }
    
    if (validate_and_or(tokens, &pos, token_count) != PARSE_SUCCESS) {
        return PARSE_SYNTAX_ERROR;
    }
    
    while (!is_at_end(tokens, pos, token_count)) {
        const token_t* token = &tokens[pos];
        
        if (is_token_type(token, TOKEN_AMPERSAND) || 
            is_token_type(token, TOKEN_SEMICOLON)) {
            pos++;
            
            if (is_at_end(tokens, pos, token_count)) {
                return PARSE_SUCCESS;
            }
            
            if (validate_and_or(tokens, &pos, token_count) != PARSE_SUCCESS) {
                return PARSE_SYNTAX_ERROR;
            }
        } else {
            return PARSE_SYNTAX_ERROR;
        }
    }
    
    return PARSE_SUCCESS;
}

int shell_validate_syntax(const char* input) {
    token_t* tokens = malloc(MAX_TOKENS * sizeof(token_t));
    if (!tokens) {
        fprintf(stderr, "Memory allocation error\n");
        return PARSE_SYNTAX_ERROR;
    }
    
    int token_count = tokenize_input(input, tokens, MAX_TOKENS);
    
    if (token_count < 0) {
        fprintf(stderr, "Tokenization error\n");
        free(tokens);
        return PARSE_SYNTAX_ERROR;
    }
    
    if (token_count >= MAX_TOKENS) {
        fprintf(stderr, "Too many tokens\n");
        free(tokens);
        return PARSE_TOO_MANY_TOKENS;
    }
    
    int result = validate_shell_cmd(tokens, token_count);
    free(tokens);
    return result;
}
