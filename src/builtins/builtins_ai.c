/**
 * @file builtins_ai.c
 * @brief AI-powered builtin commands for AIshA
 * 
 * Implements:
 *   ai <message>     - Chat with AI
 *   ask <query>      - Translate natural language to shell command
 *   explain <cmd>    - Explain what a command does
 *   aifix            - Get AI suggestion for last error
 *   aiconfig         - Show AI configuration status
 *   aikey            - Set API key
 */

#include "builtins.h"
#include "ai.h"
#include "colors.h"
#include "shell.h"
#include <string.h>

/* Store last command and error for aifix */
static char g_last_command[4096] = "";
static char g_last_error[4096] = "";

void ai_set_last_command(const char* cmd) {
    if (cmd) {
        strncpy(g_last_command, cmd, sizeof(g_last_command) - 1);
        g_last_command[sizeof(g_last_command) - 1] = '\0';
    }
}

void ai_set_last_error(const char* err) {
    if (err) {
        strncpy(g_last_error, err, sizeof(g_last_error) - 1);
        g_last_error[sizeof(g_last_error) - 1] = '\0';
    }
}

/*============================================================================
 * Helper Functions
 *============================================================================*/

/* Print a nice separator line */
static void print_separator(void) {
    printf("%s────────────────────────────────────────%s\n", COLOR_DIM, COLOR_RESET);
}

/* Trim whitespace from a string in-place, return pointer to start */
static char* trim_string(char* str) {
    if (!str) return str;
    
    /* Trim leading */
    while (*str && (*str == ' ' || *str == '\n' || *str == '\t')) str++;
    
    /* Trim trailing */
    size_t len = strlen(str);
    while (len > 0 && (str[len-1] == ' ' || str[len-1] == '\n' || str[len-1] == '\t')) {
        str[--len] = '\0';
    }
    
    return str;
}

/* Print status indicator */
static void print_status(const char* msg) {
    printf("%s[*]%s %s\n", COLOR_CYAN, COLOR_RESET, msg);
}

/*============================================================================
 * AI Builtin Commands
 *============================================================================*/

/**
 * ai - Interactive AI chat
 */
int builtin_ai(char** args, int argc) {
    if (argc < 2) {
        printf("Usage: %sai%s <message>\n", COLOR_BOLD, COLOR_RESET);
        printf("       Chat with AIshA (Advanced Intelligent Shell Assistant)\n");
        return 1;
    }
    
    if (!ai_available()) {
        print_error("AI not configured. Run 'aikey <YOUR_KEY>' to set up.\n");
        return 1;
    }
    
    /* Concatenate all arguments */
    char message[4096] = "";
    for (int i = 1; i < argc; i++) {
        if (i > 1) strcat(message, " ");
        strncat(message, args[i], sizeof(message) - strlen(message) - 1);
    }
    
    print_status("Thinking...");
    
    char* response = ai_chat(message);
    if (response) {
        printf("\n%s\n\n", response);
        free(response);
        return 0;
    }
    
    print_error("Failed to get AI response\n");
    return 1;
}

/**
 * ask - Translate natural language to shell command
 */
int builtin_ask(char** args, int argc) {
    if (argc < 2) {
        printf("Usage: %sask%s <what you want to do>\n", COLOR_BOLD, COLOR_RESET);
        printf("       Translates natural language to a shell command\n\n");
        printf("Examples:\n");
        printf("  ask list all python files\n");
        printf("  ask find files larger than 10MB\n");
        printf("  ask show disk usage sorted by size\n");
        return 1;
    }
    
    if (!ai_available()) {
        print_error("AI not configured. Run 'aikey <YOUR_KEY>' to set up.\n");
        return 1;
    }
    
    /* Concatenate arguments */
    char query[4096] = "";
    for (int i = 1; i < argc; i++) {
        if (i > 1) strcat(query, " ");
        strncat(query, args[i], sizeof(query) - strlen(query) - 1);
    }
    
    print_status("Translating...");
    
    char* command = ai_translate(query);
    if (command) {
        char* trimmed = trim_string(command);
        
        /* Check for error response */
        if (strncmp(trimmed, "ERROR:", 6) == 0) {
            print_error("%s\n", trimmed + 7);
            free(command);
            return 1;
        }
        
        /* Display the suggested command */
        printf("\n");
        print_separator();
        printf("  %s$%s %s%s%s\n", COLOR_GREEN, COLOR_RESET, COLOR_BOLD, trimmed, COLOR_RESET);
        print_separator();
        printf("\n");
        
        /* Prompt for action */
        printf("Execute? [%sY%s]es / [%sn%s]o / [%se%s]dit: ", 
               COLOR_BOLD, COLOR_RESET, 
               COLOR_DIM, COLOR_RESET,
               COLOR_DIM, COLOR_RESET);
        fflush(stdout);
        
        char response[16];
        if (fgets(response, sizeof(response), stdin)) {
            char r = response[0];
            
            if (r == 'y' || r == 'Y' || r == '\n') {
                printf("\n");
                int result = system(trimmed);
                free(command);
                return WEXITSTATUS(result);
            } else if (r == 'e' || r == 'E') {
                /* Copy to clipboard hint */
                printf("\nCommand: %s\n", trimmed);
                printf("(Copy and modify as needed)\n\n");
            } else {
                printf("Cancelled.\n");
            }
        }
        
        free(command);
        return 0;
    }
    
    print_error("Failed to translate request\n");
    return 1;
}

/**
 * explain - Explain what a shell command does
 */
int builtin_explain(char** args, int argc) {
    if (argc < 2) {
        printf("Usage: %sexplain%s <command>\n", COLOR_BOLD, COLOR_RESET);
        printf("       Explains what a shell command does\n");
        return 1;
    }
    
    if (!ai_available()) {
        print_error("AI not configured. Run 'aikey <YOUR_KEY>' to set up.\n");
        return 1;
    }
    
    /* Concatenate command */
    char command[4096] = "";
    for (int i = 1; i < argc; i++) {
        if (i > 1) strcat(command, " ");
        strncat(command, args[i], sizeof(command) - strlen(command) - 1);
    }
    
    print_status("Analyzing...");
    
    char* explanation = ai_explain(command);
    if (explanation) {
        printf("\n");
        print_separator();
        printf("  %s$%s %s\n", COLOR_GREEN, COLOR_RESET, command);
        print_separator();
        printf("\n%s\n", explanation);
        free(explanation);
        return 0;
    }
    
    print_error("Failed to explain command\n");
    return 1;
}

/**
 * aifix - Suggest fix for the last error
 */
int builtin_aifix(char** args, int argc) {
    (void)args;
    (void)argc;
    
    if (!ai_available()) {
        print_error("AI not configured. Run 'aikey <YOUR_KEY>' to set up.\n");
        return 1;
    }
    
    if (g_last_error[0] == '\0') {
        printf("No recent error to analyze.\n");
        return 0;
    }
    
    print_status("Analyzing error...");
    
    char* fix = ai_fix(g_last_error, g_last_command);
    if (fix) {
        printf("\n");
        print_separator();
        printf("  %sLast command:%s %s\n", COLOR_DIM, COLOR_RESET, g_last_command);
        printf("  %sError:%s %s\n", COLOR_RED, COLOR_RESET, g_last_error);
        print_separator();
        printf("\n%s\n", fix);
        free(fix);
        return 0;
    }
    
    print_error("Failed to analyze error\n");
    return 1;
}

/**
 * aiconfig - Show AI configuration status
 */
int builtin_aiconfig(char** args, int argc) {
    (void)args;
    (void)argc;
    
    printf("\n");
    printf("  %sAIshA%s - Advanced Intelligent Shell Assistant\n", COLOR_BOLD, COLOR_RESET);
    printf("  Version %s\n\n", SHELL_VERSION);
    
    printf("  %-12s %s%s%s\n", "Status:", 
           ai_available() ? COLOR_GREEN : COLOR_RED,
           ai_available() ? "Ready" : "Not configured",
           COLOR_RESET);
    printf("  %-12s %s\n", "API Key:", ai_get_masked_key());
    printf("  %-12s %s\n", "Model:", "gemini-2.5-flash");
    printf("  %-12s %s\n", "Config:", "~/.aisharc");
    printf("\n");
    
    if (!ai_available()) {
        printf("  %sTo enable AI features:%s\n", COLOR_DIM, COLOR_RESET);
        printf("    aikey YOUR_API_KEY        Set key for this session\n");
        printf("    aikey -s YOUR_API_KEY     Save key to ~/.aisharc\n\n");
    }
    
    return 0;
}

/**
 * aikey - Set the Gemini API key
 */
int builtin_aikey(char** args, int argc) {
    if (argc < 2) {
        printf("Usage: %saikey%s [-s] <API_KEY>\n", COLOR_BOLD, COLOR_RESET);
        printf("       -s    Save key to ~/.aisharc\n");
        return 1;
    }
    
    int save_to_config = 0;
    const char* key = NULL;
    
    if (strcmp(args[1], "-s") == 0) {
        if (argc < 3) {
            print_error("Missing API key\n");
            return 1;
        }
        save_to_config = 1;
        key = args[2];
    } else {
        key = args[1];
    }
    
    /* Set in environment */
    if (setenv("GEMINI_API_KEY", key, 1) != 0) {
        print_error("Failed to set environment variable\n");
        return 1;
    }
    
    /* Re-initialize AI */
    ai_cleanup();
    if (ai_init() == 0) {
        print_success("API key configured successfully\n");
    } else {
        print_error("Failed to initialize AI\n");
        return 1;
    }
    
    /* Save to config if requested */
    if (save_to_config) {
        char config_path[1024];
        snprintf(config_path, sizeof(config_path), "%s/.aisharc", g_home_directory);
        
        FILE* f = fopen(config_path, "a");
        if (f) {
            fprintf(f, "GEMINI_API_KEY=%s\n", key);
            fclose(f);
            printf("Saved to %s\n", config_path);
        } else {
            print_warning("Could not save to config file\n");
        }
    }
    
    return 0;
}
