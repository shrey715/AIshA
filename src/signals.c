#include "signals.h"
#include "background.h"
#include "shell.h"
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

volatile sig_atomic_t g_foreground_pid = -1;

void sigint_handler(int signo) {
    (void)signo;
    
    if (g_foreground_pid > 0) {
        kill(g_foreground_pid, SIGINT);
    }
    write(STDOUT_FILENO, "\n", 1);
}

void sigtstp_handler(int signo) {
    (void)signo;
    
    if (g_foreground_pid > 0) {
        /* Send SIGTSTP to foreground process */
        kill(g_foreground_pid, SIGTSTP);
    }
    write(STDOUT_FILENO, "\n", 1);
}

void setup_signal_handlers(void) {
    struct sigaction sa_int;
    struct sigaction sa_tstp;

    /* Setup SIGINT handler (Ctrl+C) */
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_handler = sigint_handler;
    sa_int.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa_int, NULL);

    /* Setup SIGTSTP handler (Ctrl+Z) */
    sigemptyset(&sa_tstp.sa_mask);
    sa_tstp.sa_handler = sigtstp_handler;
    sa_tstp.sa_flags = SA_RESTART;
    sigaction(SIGTSTP, &sa_tstp, NULL);

    signal(SIGQUIT, SIG_IGN);
}
