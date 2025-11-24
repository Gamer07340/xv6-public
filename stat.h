#define T_DIR  1   // Directory
#define T_FILE 2   // File
#define T_DEV  3   // Device

struct stat {
  // xv7 fields (must stay first, unchanged)
  short type;      // Type of file
  int   dev;       // File system's disk device
  uint  ino;       // Inode number
  short nlink;     // Number of links to file
  uint  size;      // Size of file in bytes
  uint  mode;      // POSIX-like file mode

  // POSIX extensions for compatibility
  uint  uid;       // Owner user ID
  uint  gid;       // Owner group ID
  uint  rdev;      // Device ID (if special file)

  uint  blksize;   // Block size for I/O
  uint  blocks;    // Number of blocks allocated

  uint  atime;     // Last access time
  uint  mtime;     // Last modification time
  uint  ctime;     // Last status change time
};

