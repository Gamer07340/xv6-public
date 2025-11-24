#ifndef _UNISTD_H
#define _UNISTD_H
#include <fcntl.h>
#include <stddef.h>
typedef int ssize_t;
int open(const char *pathname, int flags, ...);
int close(int fd);
int read(int fd, void *buf, size_t count);
int write(int fd, const void *buf, size_t count);
int lseek(int fd, int offset, int whence);
int unlink(const char *pathname);
char *getcwd(char *buf, size_t size);
int execvp(const char *file, char *const argv[]);
#endif
