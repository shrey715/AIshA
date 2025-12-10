#include "background.h"
#include "shell.h"
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>

static background_job_t* background_jobs = NULL;
static int next_job_id = 1;

int get_next_job_id(void) {
    return next_job_id++;
}

int add_background_job(pid_t pid, const char* command, int status) {
    background_job_t* new_job = malloc(sizeof(background_job_t));
    if (!new_job) return -1;

    new_job->pid = pid;
    new_job->job_id = get_next_job_id();
    new_job->command = strdup(command);
    new_job->status = status == 0 ? PROCESS_RUNNING : PROCESS_STOPPED;
    new_job->next = background_jobs;
    background_jobs = new_job;
    
    printf("[%d] %d\n", new_job->job_id, pid);
    return new_job->job_id;
}

void check_background_jobs(void) {
    background_job_t** current = &background_jobs;
    
    while (*current) {
        background_job_t* job = *current;
        int status;
        pid_t result = waitpid(job->pid, &status, WNOHANG | WUNTRACED);

        if (result == job->pid) {
            if (WIFEXITED(status)) {
                if (WEXITSTATUS(status) == 0) {
                    printf("%s with pid %d exited normally\n", job->command, job->pid);
                } else {
                    printf("%s with pid %d exited abnormally\n", job->command, job->pid);
                }
                
                *current = job->next;
                free(job->command);
                free(job);
            } else if (WIFSIGNALED(status)) {
                *current = job->next;
                free(job->command);
                free(job);
            } else if (WIFSTOPPED(status)) {
                job->status = PROCESS_STOPPED;
                current = &(job->next);
            } else if (WIFCONTINUED(status)) {
                job->status = PROCESS_RUNNING;
                current = &(job->next);
            } else {
                current = &(job->next);
            }
        } else if (result == -1) {
            // Error occurred, remove job
            *current = job->next;
            free(job->command);
            free(job);
        } else {
            // Process still running
            current = &(job->next);
        }
    }
}

static int compare_activities(const void* a, const void* b) {
    const background_job_t* job_a = *(const background_job_t**)a;
    const background_job_t* job_b = *(const background_job_t**)b;
    return strcmp(job_a->command, job_b->command);
}

void list_activities(void) {
    if (!background_jobs) {
        return;
    }

    int count = 0;
    for (background_job_t* job = background_jobs; job; job = job->next) {
        count++;
    }

    background_job_t** jobs_array = malloc(count * sizeof(background_job_t*));
    if (!jobs_array) return;

    int index = 0;
    for (background_job_t* job = background_jobs; job; job = job->next) {
        jobs_array[index++] = job;
    }

    qsort(jobs_array, count, sizeof(background_job_t*), compare_activities);

    for (int i = 0; i < count; i++) {
        background_job_t* job = jobs_array[i];
        printf("[%d] %s: %s\n", job->pid, job->command, job->status == PROCESS_RUNNING ? "Running" : "Stopped");
    }

    free(jobs_array);
}

background_job_t* find_job_by_pid(pid_t pid) {
    for (background_job_t* job = background_jobs; job; job = job->next) {
        if (job->pid == pid) {
            return job;
        }
    }
    return NULL;
}

background_job_t* find_job_by_id(int job_id) {
    for (background_job_t* job = background_jobs; job; job = job->next) {
        if (job->job_id == job_id) {
            return job;
        }
    }
    return NULL;
}

int remove_job_by_pid(pid_t pid) {
    background_job_t** current = &background_jobs;
    
    while (*current) {
        if ((*current)->pid == pid) {
            background_job_t* to_remove = *current;
            *current = (*current)->next;
            free(to_remove->command);
            free(to_remove);
            return 0;
        }
        current = &((*current)->next);
    }
    return -1;
}

int ping_process(pid_t pid, int signal) {
    background_job_t* job = find_job_by_pid(pid);
    if (!job) {
        return JOB_NOT_FOUND;
    }
    if (kill(pid, signal) == -1) {
        if (errno == ESRCH) {
            return JOB_NOT_FOUND;
        } else if (errno == EINVAL) {
            return INVALID_SIGNAL;
        } else {
            return PING_ERROR; 
        }
    }
    return 0;
}

void cleanup_background_jobs(void) {
    while (background_jobs) {
        background_job_t* temp = background_jobs;
        background_jobs = background_jobs->next;
        free(temp->command);
        free(temp);
    }
}
