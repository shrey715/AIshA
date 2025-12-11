/**
 * @file parser.h
 * @brief Lexical analysis and parsing for shell commands
 * 
 * Provides tokenization of command input and syntax validation.
 * Supports operators: pipe (|), sequential (;), background (&),
 * logical AND (&&), logical OR (||), and I/O redirection.
 */

#ifndef PARSER_H
#define PARSER_H

#include "shell.h"
#include <ctype.h>

/*============================================================================
 * Constants
 *============================================================================*/

/** Maximum number of tokens in a command */
#define MAX_TOKENS 1024

/** Maximum length of a single token */
#define MAX_TOKEN_LENGTH 4096

/*============================================================================
 * Token Types
 *============================================================================*/

/**
 * Token type enumeration
 */
typedef enum {
    TOKEN_WORD,              /**< Regular word/argument */
    TOKEN_PIPE,              /**<  | - pipe output to next command */
    TOKEN_SEMICOLON,         /**<  ; - sequential command separator */
    TOKEN_AMPERSAND,         /**<  & - run in background */
    TOKEN_AND,               /**< && - logical AND */
    TOKEN_OR,                /**< || - logical OR */
    TOKEN_INPUT_REDIRECT,    /**<  < - input redirection */
    TOKEN_OUTPUT_REDIRECT,   /**<  > - output redirection (truncate) */
    TOKEN_OUTPUT_APPEND,     /**< >> - output redirection (append) */
    TOKEN_HEREDOC,           /**< << - here document */
    TOKEN_HERESTRING,        /**< <<< - here string */
    TOKEN_LPAREN,            /**<  ( - subshell start */
    TOKEN_RPAREN,            /**<  ) - subshell end */
    TOKEN_NEWLINE,           /**< Explicit newline (for multiline) */
    TOKEN_EOF                /**< End of input */
} token_type_t;

/*============================================================================
 * Token Structure
 *============================================================================*/

/**
 * Token structure representing a lexical unit
 */
typedef struct {
    token_type_t type;              /**< Type of this token */
    char value[MAX_TOKEN_LENGTH];   /**< Token value/content */
    int quoted;                     /**< Was this token in quotes? */
} token_t;

/*============================================================================
 * Parse Result Codes
 *============================================================================*/

/**
 * Parsing result codes
 */
typedef enum {
    PARSE_SUCCESS = 0,           /**< Parsing succeeded */
    PARSE_SYNTAX_ERROR = 1,      /**< Invalid syntax */
    PARSE_TOO_MANY_TOKENS = 2,   /**< Token limit exceeded */
    PARSE_UNTERMINATED_QUOTE = 3 /**< Missing closing quote */
} parse_result_t;

/*============================================================================
 * Tokenizer Functions
 *============================================================================*/

/**
 * Tokenize an input string into tokens
 * 
 * Handles:
 * - Quoted strings (single and double quotes)
 * - Escape sequences within double quotes
 * - Operators and special characters
 * - Comments (# to end of line)
 * 
 * @param input The input string to tokenize
 * @param tokens Array to store resulting tokens
 * @param max_tokens Maximum number of tokens to produce
 * @return Number of tokens produced (includes TOKEN_EOF)
 */
int tokenize_input(const char* input, token_t tokens[], int max_tokens);

/*============================================================================
 * Pre-processing
 *============================================================================*/

/**
 * Pre-process input before tokenization
 * 
 * Applies alias expansion and variable expansion.
 * 
 * @param input Original input string
 * @return Newly allocated string with expansions applied.
 *         Caller must free.
 */
char* preprocess_input(const char* input);

/*============================================================================
 * Validation Functions
 *============================================================================*/

/**
 * Validate command syntax
 * 
 * Performs syntax validation using recursive descent parsing.
 * Checks for proper operator placement and balanced constructs.
 * 
 * @param input Input string to validate
 * @return PARSE_SUCCESS if valid, error code otherwise
 */
int shell_validate_syntax(const char* input);

/*============================================================================
 * Helper Functions
 *============================================================================*/

/**
 * Check if token type is an operator
 * 
 * @param type Token type to check
 * @return Non-zero if type is |, ;, &, &&, or ||
 */
int is_operator_token(token_type_t type);

/**
 * Check if token type is a redirection operator
 * 
 * @param type Token type to check
 * @return Non-zero if type is <, >, >>, <<, or <<<
 */
int is_redirect_token(token_type_t type);

/**
 * Get string representation of token type
 * 
 * @param type Token type
 * @return Static string naming the token type
 */
const char* token_type_name(token_type_t type);

#endif /* PARSER_H */
