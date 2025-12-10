#ifndef BACKGROUND_H
#define BACKGROUND_H

#include <sys/types.h>

#define JOB_NOT_FOUND -1
#define INVALID_SIGNAL -2
#define PING_ERROR -3

typedef enum {
    PROCESS_RUNNING,
    PROCESS_STOPPED
} process_status_t;

// Background job structure
typedef struct background_job {
    pid_t pid;
    int job_id;
    char* command;
    process_status_t status;
    struct background_job* next;
} background_job_t;

// Background job management
int add_background_job(pid_t pid, const char* command, int status);
void check_background_jobs(void);
void cleanup_background_jobs(void);
int get_next_job_id(void);

// Activities functionality
void list_activities(void);
background_job_t* get_background_jobs(void);

// Job control functions
background_job_t* find_job_by_pid(pid_t pid); 
background_job_t* find_job_by_id(int job_id);
background_job_t* get_most_recent_job(void);
int remove_job_by_pid(pid_t pid);

int ping_process(pid_t pid, int signal);

#endif
