#include "shell.h"
#include "parser.h"
#include "execute.h"
#include "builtins.h"
#include "background.h"
#include "log.h"
#include "signals.h"

char* g_home_directory = NULL;
char* g_username = NULL;
char* g_system_name = NULL;

int main(){
    shell_init();
    setup_signal_handlers();

    while(1){
        check_background_jobs();
        shell_show_prompt();

        char* input_str = shell_read_input();
        if (input_str == NULL){
            printf("\nlogout\n");
            cleanup_background_jobs();
            shell_cleanup();
            return SHELL_SUCCESS;
        }
        if (strlen(input_str) == 0) {
            free(input_str);
            continue;
        }

        int should_log = 1;
        token_t tokens[MAX_TOKENS];
        int token_count = tokenize_input(input_str, tokens, MAX_TOKENS);
        
        for (int i = 0; i < token_count; i++) {
            if (tokens[i].type == TOKEN_WORD && 
                (strcmp(tokens[i].value, "log") == 0 || 
                 strcmp(tokens[i].value, "activities") == 0 ||
                 strcmp(tokens[i].value, "ping") == 0)) {
                should_log = 0;
                break;
            }
        }
        
        if(token_count > 0){
            execute_shell_command_with_operators(tokens, token_count);
        }

        if(should_log) {
            log_add_command(input_str);
        }
       
        free(input_str);
    }

    cleanup_background_jobs();
    shell_cleanup();
    return SHELL_SUCCESS;
}
