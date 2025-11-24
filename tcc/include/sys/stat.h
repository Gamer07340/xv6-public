#ifndef _SYS_STAT_H
#define _SYS_STAT_H
struct stat {
  short type;  // Type of file
  int dev;     // File system's disk device
  unsigned int ino; // Inode number
  short nlink; // Number of links to file
  unsigned int size; // Size of file in bytes
};
#define S_IFMT  0170000
#define S_IFDIR 0040000
#define S_IFREG 0100000
#define S_ISDIR(m)      (((m) & S_IFMT) == S_IFDIR)
#endif
