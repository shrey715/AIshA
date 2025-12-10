#include "shell.h"
#include "builtins.h"
#include "background.h"
#include "log.h"

void shell_init(void){
    struct passwd *pw = getpwuid(getuid());
    if(pw){
        g_username = strdup(pw->pw_name);
        g_home_directory = strdup(pw->pw_dir);
    }
    
    char hostname[SHELL_MAX_PATH_LENGTH];
    if(gethostname(hostname, sizeof(hostname)) == 0){
        g_system_name = strdup(hostname);
    }
    
    // fallbacks in case environment variables are not set
    if(!g_home_directory){
        g_home_directory = strdup("/");
    }
    if(!g_username){
        g_username = strdup("unknown_user");
    }
    if(!g_system_name){
        g_system_name = strdup("unknown_host");
    }

    log_load_history();
}

void shell_cleanup(void){
    cleanup_hop();
    cleanup_log();
    cleanup_background_jobs();
    
    if(g_home_directory){
        free(g_home_directory);
        g_home_directory = NULL;
    }
    if(g_system_name){
        free(g_system_name);
        g_system_name = NULL;
    }
    if(g_username){
        free(g_username);
        g_username = NULL;
    }
}
