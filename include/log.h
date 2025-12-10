#ifndef LOG_H
#define LOG_H

#define LOG_MAX_ENTRIES 15
#define LOG_MAX_COMMAND_LENGTH 4096

// Log structure
typedef struct {
    char commands[LOG_MAX_ENTRIES][LOG_MAX_COMMAND_LENGTH];
    int count;
    int head; // circular buffer head
} command_log_t;

// Global command log variable
extern command_log_t g_command_log;

// Log functions
void log_add_command(const char* command);
void log_load_history(void);
void log_save_history(void);
void cleanup_log(void);

#endif
