/**
 * @file builtins_jobs.c
 * @brief Job control builtin commands
 * 
 * Implements: activities/jobs, ping/kill, fg, bg
 */

#include "builtins.h"
#include "background.h"
#include "signals.h"
#include "colors.h"
#include <signal.h>

/**
 * activities/jobs - List background jobs
 */
int builtin_activities(char** args, int argc) {
    (void)args;
    if (argc != 1) {
        print_error("activities: too many arguments\n");
        return 1;
    }
    list_activities();
    return 0;
}

int builtin_jobs(char** args, int argc) {
    return builtin_activities(args, argc);
}

/**
 * ping - Send signal to process (original syntax)
 * 
 * Usage: ping PID SIGNAL
 */
int builtin_ping(char** args, int argc) {
    if (argc != 3) {
        print_error("ping: usage: ping PID SIGNAL\n");
        return 1;
    }

    pid_t pid = (pid_t)atoi(args[1]);
    int signal = atoi(args[2]) % 32;
    int res = ping_process(pid, signal);

    if (res == 0) {
        printf("Sent signal %d to process with pid %d\n", signal, pid);
    } else if (res == JOB_NOT_FOUND) {
        print_error("ping: (%d) - No such process\n", pid);
        return 1;
    } else {
        print_error("ping: invalid signal or process\n");
        return 1;
    }

    return 0;
}

/**
 * kill - Send signal to process (standard syntax)
 * 
 * Usage: kill [-SIGNAL] PID...
 */
int builtin_kill(char** args, int argc) {
    if (argc < 2) {
        print_error("kill: usage: kill [-SIGNAL] PID...\n");
        return 1;
    }
    
    int signal = SIGTERM;
    int start = 1;
    
    if (args[1][0] == '-') {
        if (isdigit(args[1][1])) {
            signal = atoi(args[1] + 1);
        } else {
            print_error("kill: %s: invalid signal specification\n", args[1]);
            return 1;
        }
        start = 2;
    }
    
    int ret = 0;
    for (int i = start; i < argc; i++) {
        pid_t pid = (pid_t)atoi(args[i]);
        if (kill(pid, signal) != 0) {
            print_error("kill: (%d) - %s\n", pid, strerror(errno));
            ret = 1;
        }
    }
    return ret;
}

/**
 * fg - Bring job to foreground
 * 
 * Usage: fg JOB_ID
 */
int builtin_fg(char** args, int argc) {
    background_job_t* job = NULL;
    
    if (argc == 1) {
        print_error("fg: usage: fg JOB_ID\n");
        return 1;
    } else if (argc == 2) {
        char* endptr;
        long job_id = strtol(args[1], &endptr, 10);
        
        if (*endptr != '\0' || endptr == args[1] || job_id <= 0) {
            print_error("fg: %s: no such job\n", args[1]);
            return 1;
        }
        
        job = find_job_by_id((int)job_id);
        if (!job) {
            print_error("fg: %s: no such job\n", args[1]);
            return 1;
        }
    } else {
        print_error("fg: too many arguments\n");
        return 1;
    }
    
    printf("%s\n", job->command);
    
    if (job->status == PROCESS_STOPPED) {
        if (kill(job->pid, SIGCONT) == -1) {
            if (errno == ESRCH) {
                print_error("fg: job has terminated\n");
                remove_job_by_pid(job->pid);
                return 1;
            }
            print_error("fg: %s\n", strerror(errno));
            return 1;
        }
    }
    
    g_foreground_pid = job->pid;
    remove_job_by_pid(job->pid);
    
    int status;
    if (waitpid(job->pid, &status, WUNTRACED) == -1) {
        print_error("fg: waitpid: %s\n", strerror(errno));
        g_foreground_pid = -1;
        return 1;
    }
    
    g_foreground_pid = -1;
    
    if (WIFSTOPPED(status)) {
        int jid = add_background_job(job->pid, job->command, PROCESS_STOPPED);
        printf("\n[%d] Stopped                 %s\n", jid, job->command);
    }
    
    return 0;
}

/**
 * bg - Resume job in background
 * 
 * Usage: bg JOB_ID
 */
int builtin_bg(char** args, int argc) {
    background_job_t* job = NULL;
    
    if (argc == 1) {
        print_error("bg: usage: bg JOB_ID\n");
        return 1;
    } else if (argc == 2) {
        char* endptr;
        long job_id = strtol(args[1], &endptr, 10);
        
        if (*endptr != '\0' || endptr == args[1] || job_id <= 0) {
            print_error("bg: %s: no such job\n", args[1]);
            return 1;
        }
        
        job = find_job_by_id((int)job_id);
        if (!job) {
            print_error("bg: %s: no such job\n", args[1]);
            return 1;
        }
    } else {
        print_error("bg: too many arguments\n");
        return 1;
    }
    
    if (job->status == PROCESS_RUNNING) {
        print_error("bg: job %d already in background\n", job->job_id);
        return 0;
    }
    
    if (kill(job->pid, SIGCONT) == -1) {
        if (errno == ESRCH) {
            print_error("bg: job has terminated\n");
            remove_job_by_pid(job->pid);
            return 1;
        }
        print_error("bg: %s\n", strerror(errno));
        return 1;
    }
    
    job->status = PROCESS_RUNNING;
    printf("[%d] %s &\n", job->job_id, job->command);
        
    return 0;
}
