#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Token validation helpers
static int is_token_type(const token_t* token, token_type_t type) {
    return token->type == type;
}

static int is_at_end(const token_t tokens[], int pos, int token_count) {
    return pos >= token_count || tokens[pos].type == TOKEN_EOF;
}

// Grammar validation functions
static int validate_name(const token_t tokens[], int* pos, int token_count) {
    // !Rule: name -> r"[^|&><;]+"
    if (is_at_end(tokens, *pos, token_count)) {
        return PARSE_SYNTAX_ERROR;
    }
    
    const token_t* token = &tokens[*pos];
    if (!is_token_type(token, TOKEN_WORD)) {
        return PARSE_SYNTAX_ERROR;
    }
    
    for (int i = 0; token->value[i] != '\0'; i++) {
        if (strchr("|&><;", token->value[i]) != NULL) {
            return PARSE_SYNTAX_ERROR;
        }
    }
    
    (*pos)++;
    return PARSE_SUCCESS;
}

static int validate_input(const token_t tokens[], int* pos, int token_count) {
    // !Rule: input -> < name
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
    // !Rule: output -> > name | >> name
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
    // !Rule: atomic -> name (name | input | output)*
    if (is_at_end(tokens, *pos, token_count)) {
        return PARSE_SYNTAX_ERROR;
    }
    
    if (validate_name(tokens, pos, token_count) != PARSE_SUCCESS) {
        return PARSE_SYNTAX_ERROR;
    }
    
    if (is_at_end(tokens, *pos, token_count)) {
        return PARSE_SUCCESS;
    }
    
    const token_t* next_token = &tokens[*pos];
    
    if (is_token_type(next_token, TOKEN_PIPE) || 
        is_token_type(next_token, TOKEN_SEMICOLON) || 
        is_token_type(next_token, TOKEN_AMPERSAND) ||
        is_token_type(next_token, TOKEN_EOF)) {
        return PARSE_SUCCESS;
    }
    
    while (!is_at_end(tokens, *pos, token_count)) {
        const token_t* token = &tokens[*pos];
        
        if (is_token_type(token, TOKEN_PIPE) || 
            is_token_type(token, TOKEN_SEMICOLON) || 
            is_token_type(token, TOKEN_AMPERSAND) ||
            is_token_type(token, TOKEN_EOF)) {
            break;
        }
        
        if (is_token_type(token, TOKEN_INPUT_REDIRECT)) {
            if (validate_input(tokens, pos, token_count) != PARSE_SUCCESS) {
                return PARSE_SYNTAX_ERROR;
            }
        } else if (is_token_type(token, TOKEN_OUTPUT_REDIRECT) || is_token_type(token, TOKEN_OUTPUT_APPEND)) {
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

static int validate_cmd_group(const token_t tokens[], int* pos, int token_count) {
    // !Rule: cmd_group -> atomic (\| atomic)*
    
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
            
            if (is_at_end(tokens, *pos, token_count) || !is_token_type(&tokens[*pos], TOKEN_WORD)) {
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

static int validate_shell_cmd(const token_t tokens[], int token_count) {
    // !Rule: shell_cmd -> cmd_group ((&; | ;) cmd_group)* &?
    int pos = 0;
    
    if (is_at_end(tokens, pos, token_count)) {
        return PARSE_SUCCESS;
    }
    
    if (validate_cmd_group(tokens, &pos, token_count) != PARSE_SUCCESS) {
        return PARSE_SYNTAX_ERROR;
    }
    
    while (!is_at_end(tokens, pos, token_count)) {
        const token_t* token = &tokens[pos];
        
        if (is_token_type(token, TOKEN_AMPERSAND) || 
            is_token_type(token, TOKEN_SEMICOLON)) {
            pos++; // consume separator
            
            if (is_at_end(tokens, pos, token_count)) {
                return PARSE_SUCCESS;
            }
            
            if (validate_cmd_group(tokens, &pos, token_count) != PARSE_SUCCESS) {
                return PARSE_SYNTAX_ERROR;
            }
        } else {
            return PARSE_SYNTAX_ERROR;
        }
    }
    
    return PARSE_SUCCESS;
}

// Tokenizer implementation
int tokenize_input(const char* input, token_t tokens[], int max_tokens) {
    int token_cnt = 0;
    const char* curr = input;
    
    while (*curr != '\0' && token_cnt < max_tokens - 1) {
        while (isspace(*curr)) curr++;
        
        if (*curr == '\0') break;
        if (*curr == '|') {
            tokens[token_cnt].type = TOKEN_PIPE;
            snprintf(tokens[token_cnt].value, MAX_TOKEN_LENGTH, "|");
            token_cnt++;
            curr++;
        } else if (*curr == '&') {
            tokens[token_cnt].type = TOKEN_AMPERSAND;
            snprintf(tokens[token_cnt].value, MAX_TOKEN_LENGTH, "&");
            token_cnt++;
            curr++;
        } else if (*curr == ';') {
            tokens[token_cnt].type = TOKEN_SEMICOLON;
            snprintf(tokens[token_cnt].value, MAX_TOKEN_LENGTH, ";");
            token_cnt++;
            curr++;
        } else if (*curr == '<') {
            tokens[token_cnt].type = TOKEN_INPUT_REDIRECT;
            snprintf(tokens[token_cnt].value, MAX_TOKEN_LENGTH, "<");
            token_cnt++;
            curr++;
        } else if (*curr == '>') {
            if (*(curr + 1) == '>') {
                tokens[token_cnt].type = TOKEN_OUTPUT_APPEND;
                snprintf(tokens[token_cnt].value, MAX_TOKEN_LENGTH, ">>");
                token_cnt++;
                curr += 2;
            } else {
                tokens[token_cnt].type = TOKEN_OUTPUT_REDIRECT;
                snprintf(tokens[token_cnt].value, MAX_TOKEN_LENGTH, ">");
                token_cnt++;
                curr++;
            }
        } else if (!isspace(*curr)) {
            const char* start = curr;
            int len = 0;
            
            while (*curr != '\0' && !isspace(*curr) && len < MAX_TOKEN_LENGTH - 1 && *curr != '|' && *curr != '&' && *curr != ';') {
                curr++;
                len++;
            }
            
            int has_redirect = 0;
            for (int i = 0; i < len; i++) {
                if (start[i] == '<' || start[i] == '>') {
                    has_redirect = 1;
                    break;
                }
            }
            if (has_redirect) {
                tokens[token_cnt].type = TOKEN_WORD;
                strncpy(tokens[token_cnt].value, start, len);
                tokens[token_cnt].value[len] = '\0';
                token_cnt++;
            } else {
                tokens[token_cnt].type = TOKEN_WORD;
                strncpy(tokens[token_cnt].value, start, len);
                tokens[token_cnt].value[len] = '\0';
                token_cnt++;
            }
        }
    }
    
    if (token_cnt < max_tokens) {
        tokens[token_cnt].type = TOKEN_EOF;
        tokens[token_cnt].value[0] = '\0';
        token_cnt++;
    }
    
    return token_cnt;
}

// Validation entry point
int shell_validate_syntax(const char* input) {
    token_t tokens[MAX_TOKENS];
    int token_count = tokenize_input(input, tokens, MAX_TOKENS);
    
    if (token_count < 0) {
        fprintf(stderr, "Tokenization error\n");
        return PARSE_SYNTAX_ERROR;
    }
    
    if (token_count >= MAX_TOKENS) {
        fprintf(stderr, "Too many tokens\n");
        return PARSE_TOO_MANY_TOKENS;
    }
    
    return validate_shell_cmd(tokens, token_count);
}
