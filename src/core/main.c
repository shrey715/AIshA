#include "shell.h"
#include "parser.h"
#include "execute.h"
#include "builtins.h"
#include "background.h"
#include "log.h"
#include "signals.h"
#include "variables.h"
#include "alias.h"
#include "readline.h"
#include "colors.h"
#include "ai.h"
#include <limits.h>
#include <errno.h>

/* Global variables */
char* g_home_directory = NULL;
char* g_username = NULL;
char* g_system_name = NULL;
char* g_shell_name = NULL;
char* g_ps1 = NULL;
char* g_ps2 = NULL;
int g_interactive = 0;

/* Forward declarations */
static void setup_default_aliases(void);
static void load_rc_file(const char* path);
static void print_welcome(void);

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    /* Initialize all subsystems */
    shell_init();
    variables_init();
    alias_init();
    readline_init();
    setup_signal_handlers();
    
    /* Set up default aliases for backward compatibility */
    setup_default_aliases();
    
    /* Load configuration file */
    shell_load_config();
    
    /* Set interactive flag */
    g_interactive = isatty(STDIN_FILENO);
    
    /* Print welcome message for interactive sessions */
    if (g_interactive) {
        print_welcome();
    }
    
    /* Main shell loop */
    while (1) {
        check_background_jobs();
        
        char* input_str = NULL;
        
        if (g_interactive) {
            /* Interactive mode: use readline */
            char* prompt = shell_generate_prompt(g_ps1);
            input_str = shell_readline(prompt);
            free(prompt);
        } else {
            /* Non-interactive: simple read */
            input_str = shell_read_input();
        }
        
        if (input_str == NULL) {
            /* EOF */
            if (g_interactive) {
                printf("\nlogout\n");
            }
            break;
        }
        
        if (strlen(input_str) == 0) {
            free(input_str);
            continue;
        }
        
        /* Add to history */
        history_add(input_str);
        
        /* Pre-process: expand aliases and variables */
        char* processed = preprocess_input(input_str);
        if (!processed) {
            free(input_str);
            continue;
        }
        
        /* Tokenize - heap allocated to avoid stack overflow */
        token_t* tokens = malloc(MAX_TOKENS * sizeof(token_t));
        if (!tokens) {
            free(processed);
            free(input_str);
            continue;
        }
        int token_count = tokenize_input(processed, tokens, MAX_TOKENS);
        
        /* Check for log/history command (don't add to persistent history) */
        int should_log = 1;
        for (int i = 0; i < token_count; i++) {
            if (tokens[i].type == TOKEN_WORD &&
                (strcmp(tokens[i].value, "log") == 0 ||
                 strcmp(tokens[i].value, "history") == 0 ||
                 strcmp(tokens[i].value, "activities") == 0 ||
                 strcmp(tokens[i].value, "jobs") == 0 ||
                 strcmp(tokens[i].value, "ping") == 0)) {
                should_log = 0;
                break;
            }
        }
        
        /* Execute */
        if (token_count > 0) {
            execute_shell_command_with_operators(tokens, token_count);
        }
        
        /* Add to persistent log */
        if (should_log) {
            log_add_command(input_str);
        }
        
        free(tokens);
        free(processed);
        free(input_str);
    }
    
    /* Cleanup */
    cleanup_background_jobs();
    readline_cleanup();
    alias_cleanup();
    variables_cleanup();
    shell_cleanup();
    
    return g_last_exit_status;
}

static void setup_default_aliases(void) {
    /* These ensure original command names work alongside standard names */
    /* The builtins table already has both, but these are for user-visible alias output */
}

static void load_rc_file(const char* path) {
    FILE* file = fopen(path, "r");
    if (!file) return;
    
    char line[SHELL_MAX_INPUT_LENGTH];
    
    while (fgets(line, sizeof(line), file)) {
        /* Skip empty lines and comments */
        char* p = line;
        while (*p && isspace(*p)) p++;
        if (!*p || *p == '#') continue;
        
        /* Remove trailing newline */
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }
        
        /* Pre-process and execute */
        char* processed = preprocess_input(line);
        if (processed) {
            token_t* tokens = malloc(MAX_TOKENS * sizeof(token_t));
            if (tokens) {
                int token_count = tokenize_input(processed, tokens, MAX_TOKENS);
                if (token_count > 0) {
                    execute_shell_command_with_operators(tokens, token_count);
                }
                free(tokens);
            }
            free(processed);
        }
    }
    
    fclose(file);
}

void shell_load_config(void) {
    if (!g_home_directory) return;
    
    char rc_path[SHELL_MAX_PATH_LENGTH];
    snprintf(rc_path, sizeof(rc_path), "%s/.aisharc", g_home_directory);
    load_rc_file(rc_path);
}

void shell_save_config(void) {
    /* TODO: Save shell state if needed */
}

static void print_welcome(void) {
    printf("\n");
    printf("  %sAIshA%s v%s\n", COLOR_BOLD_CYAN, COLOR_RESET, SHELL_VERSION);
    printf("  %sAdvanced Intelligent Shell Assistant%s\n\n", COLOR_DIM, COLOR_RESET);
    
    if (ai_available()) {
        printf("  %s[AI Ready]%s Type %sask%s followed by what you want to do\n", 
               COLOR_GREEN, COLOR_RESET, COLOR_BOLD, COLOR_RESET);
    } else {
        printf("  %s[AI Offline]%s Run %saikey YOUR_KEY%s to enable AI features\n",
               COLOR_YELLOW, COLOR_RESET, COLOR_BOLD, COLOR_RESET);
    }
    printf("  Type %shelp%s for available commands\n\n", COLOR_BOLD, COLOR_RESET);
}
