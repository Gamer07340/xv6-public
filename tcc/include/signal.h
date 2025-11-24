#ifndef _SIGNAL_H
#define _SIGNAL_H
typedef int sig_atomic_t;
#define SIGINT 2
#define SIGILL 4
#define SIGFPE 8
#define SIGSEGV 11
#define SIGTERM 15
#define SIGABRT 6
typedef void (*__sighandler_t)(int);
#define SIG_DFL ((__sighandler_t)0)
#define SIG_ERR ((__sighandler_t)-1)
#define SIG_IGN ((__sighandler_t)1)
void (*signal(int signum, void (*handler)(int)))(int);
#endif
