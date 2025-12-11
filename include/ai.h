/**
 * @file ai.h
 * @brief AI integration module for AIshA (Advanced Intelligent Shell Assistant)
 * 
 * Provides AI-powered features using Google Gemini API:
 * - Natural language command translation
 * - Command explanation
 * - Error diagnosis and suggestions
 * - Interactive AI chat
 * 
 * API key should be set via:
 * - Environment variable: GEMINI_API_KEY
 * - Config file: ~/.aisharc
 */

#ifndef AI_H
#define AI_H

#include <stdlib.h>

/*============================================================================
 * Constants
 *============================================================================*/

/** Gemini API endpoint */
#define GEMINI_API_URL "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent"

/** Maximum response buffer size */
#define AI_MAX_RESPONSE_SIZE (64 * 1024)

/** Maximum prompt length */
#define AI_MAX_PROMPT_SIZE 4096

/*============================================================================
 * AI Request Types
 *============================================================================*/

typedef enum {
    AI_REQUEST_TRANSLATE,    /**< Translate natural language to shell command */
    AI_REQUEST_EXPLAIN,      /**< Explain what a command does */
    AI_REQUEST_FIX,          /**< Suggest fix for an error */
    AI_REQUEST_CHAT,         /**< General AI chat */
    AI_REQUEST_SUGGEST       /**< Suggest next command based on context */
} ai_request_type_t;

/*============================================================================
 * AI Response Structure
 *============================================================================*/

typedef struct {
    char* text;              /**< Response text */
    int success;             /**< Whether request succeeded */
    char* error;             /**< Error message if failed */
} ai_response_t;

/*============================================================================
 * Initialization and Cleanup
 *============================================================================*/

/**
 * Initialize AI module
 * 
 * Loads API key from environment (GEMINI_API_KEY) or config file (~/.aisharc).
 * 
 * @return 0 on success, -1 if API key not found
 */
int ai_init(void);

/**
 * Check if AI features are available
 * 
 * @return 1 if API key is set and AI is ready, 0 otherwise
 */
int ai_available(void);

/**
 * Cleanup AI module
 */
void ai_cleanup(void);

/*============================================================================
 * AI Operations
 *============================================================================*/

/**
 * Send a request to the AI
 * 
 * @param type Type of request (translate, explain, fix, chat)
 * @param input User input or command to process
 * @return AI response (caller must free with ai_response_free)
 */
ai_response_t* ai_request(ai_request_type_t type, const char* input);

/**
 * Translate natural language to shell command
 * 
 * Example: "list all files including hidden" -> "ls -la"
 * 
 * @param natural_language The natural language description
 * @return Shell command string (caller must free)
 */
char* ai_translate(const char* natural_language);

/**
 * Explain what a command does
 * 
 * @param command Shell command to explain
 * @return Explanation text (caller must free)
 */
char* ai_explain(const char* command);

/**
 * Suggest a fix for the last error
 * 
 * @param error_message The error message to analyze
 * @param command The command that caused the error
 * @return Suggested fix (caller must free)
 */
char* ai_fix(const char* error_message, const char* command);

/**
 * Interactive AI chat
 * 
 * @param message User message
 * @return AI response (caller must free)
 */
char* ai_chat(const char* message);

/*============================================================================
 * Utility Functions
 *============================================================================*/

/**
 * Free an AI response
 */
void ai_response_free(ai_response_t* response);

/**
 * Get the current API key (masked for display)
 */
const char* ai_get_masked_key(void);

#endif /* AI_H */
