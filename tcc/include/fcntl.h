#ifndef _FCNTL_H
#define _FCNTL_H
#define O_RDONLY  0x000
#define O_WRONLY  0x001
#define O_RDWR    0x002
#define O_CREAT   0x200
#define O_TRUNC   0x400 // Not in xv6, will need to handle
#define O_APPEND  0x004 // Not in xv6, will need to handle
#define O_BINARY  0
#endif
