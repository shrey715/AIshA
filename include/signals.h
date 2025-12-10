#ifndef SIGNALS_H
#define SIGNALS_H

#include <signal.h>
#include <sys/types.h>

extern volatile sig_atomic_t g_foreground_pid;

// Signal handlers
void sigint_handler(int signo);
void sigtstp_handler(int signo);

// Setup function
void setup_signal_handlers(void);

#endif
