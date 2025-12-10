#include "shell.h"

void shell_show_prompt(void) {
    char display_path[SHELL_MAX_PATH_LENGTH];
    char* current_dir = getcwd(NULL, 0); // works only on linux though according to StackOverflow (link: https://stackoverflow.com/questions/13047366/getcwd-returns-null-after-new-pointer-is-created)

    if(current_dir == NULL){
        fprintf(stderr, "ERROR: Current directory could not be initialized!");
    } else if(g_home_directory != NULL && strncmp(current_dir, g_home_directory, strlen(g_home_directory)) == 0){
        snprintf(display_path, sizeof(display_path), "~%s", current_dir + strlen(g_home_directory));
    } else {
        snprintf(display_path, sizeof(display_path), "%s", current_dir);
    }

    printf("<%s@%s:%s> ", g_username, g_system_name, display_path);
    fflush(stdout);
    if(current_dir) free(current_dir);
}
