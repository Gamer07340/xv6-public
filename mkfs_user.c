#include "types.h"
#include "stat.h"
#include "user.h"
#include "param.h"
#include "fs.h"
#include "fcntl.h"

#define NINODES 200

// Disk layout:
// [ boot block | sb block | log | inode blocks | free bit map | data blocks ]

int nbitmap = FSSIZE/(BSIZE*8) + 1;
int ninodeblocks = NINODES / IPB + 1;
int nlog = LOGSIZE;
int nmeta;    // Number of meta blocks (boot, sb, nlog, inode, bitmap)
int nblocks;  // Number of data blocks

int fsfd;
struct superblock sb;
char zeroes[BSIZE];
uint freeinode = 1;
uint freeblock;

void balloc(int);
void wsect(uint, void*);
void winode(uint, struct dinode*);
void rinode(uint inum, struct dinode *ip);
void rsect(uint sec, void *buf);
uint ialloc(ushort type);
void iappend(uint inum, void *p, int n);

// convert to intel byte order
ushort
xshort(ushort x)
{
  return x;
}

uint
xint(uint x)
{
  return x;
}

int
main(int argc, char *argv[])
{
  int i, cc, fd;
  uint rootino, inum, off;
  struct dirent de;
  char buf[BSIZE];
  struct dinode din;

  if(argc < 2){
    printf(2, "Usage: mkfs fs.img files...\n");
    exit();
  }

  fsfd = open(argv[1], O_RDWR);
  if(fsfd < 0){
    printf(2, "mkfs: cannot open %s\n", argv[1]);
    exit();
  }

  // 1 fs block = 1 disk sector
  nmeta = 2 + nlog + ninodeblocks + nbitmap;
  nblocks = FSSIZE - nmeta;

  sb.size = xint(FSSIZE);
  sb.nblocks = xint(nblocks);
  sb.ninodes = xint(NINODES);
  sb.nlog = xint(nlog);
  sb.logstart = xint(2);
  sb.inodestart = xint(2+nlog);
  sb.bmapstart = xint(2+nlog+ninodeblocks);

  printf(1, "nmeta %d (boot, super, log blocks %u inode blocks %u, bitmap blocks %u) blocks %d total %d\n",
         nmeta, nlog, ninodeblocks, nbitmap, nblocks, FSSIZE);

  freeblock = nmeta;     // the first free block that we can allocate

  for(i = 0; i < FSSIZE; i++)
    wsect(i, zeroes);

  memset(buf, 0, sizeof(buf));
  memmove(buf, &sb, sizeof(sb));
  wsect(1, buf);

  rootino = ialloc(T_DIR);

  memset(&de, 0, sizeof(de));
  de.inum = xshort(rootino);
  strcpy(de.name, ".");
  iappend(rootino, &de, sizeof(de));

  memset(&de, 0, sizeof(de));
  de.inum = xshort(rootino);
  strcpy(de.name, "..");
  iappend(rootino, &de, sizeof(de));

  // Create /bin
  uint binino = ialloc(T_DIR);
  memset(&de, 0, sizeof(de));
  de.inum = xshort(binino);
  strcpy(de.name, ".");
  iappend(binino, &de, sizeof(de));

  memset(&de, 0, sizeof(de));
  de.inum = xshort(rootino);
  strcpy(de.name, "..");
  iappend(binino, &de, sizeof(de));

  // Add bin to root
  memset(&de, 0, sizeof(de));
  de.inum = xshort(binino);
  strcpy(de.name, "bin");
  iappend(rootino, &de, sizeof(de));

  // Create /log
  uint logino = ialloc(T_DIR);
  memset(&de, 0, sizeof(de));
  de.inum = xshort(logino);
  strcpy(de.name, ".");
  iappend(logino, &de, sizeof(de));

  memset(&de, 0, sizeof(de));
  de.inum = xshort(rootino);
  strcpy(de.name, "..");
  iappend(logino, &de, sizeof(de));

  // Add log to root
  memset(&de, 0, sizeof(de));
  de.inum = xshort(logino);
  strcpy(de.name, "log");
  iappend(rootino, &de, sizeof(de));

  // Create /dev
  uint devino = ialloc(T_DIR);
  memset(&de, 0, sizeof(de));
  de.inum = xshort(devino);
  strcpy(de.name, ".");
  iappend(devino, &de, sizeof(de));

  memset(&de, 0, sizeof(de));
  de.inum = xshort(rootino);
  strcpy(de.name, "..");
  iappend(devino, &de, sizeof(de));

  // Add dev to root
  memset(&de, 0, sizeof(de));
  de.inum = xshort(devino);
  strcpy(de.name, "dev");
  iappend(rootino, &de, sizeof(de));

  // Create /dev/console
  uint consoleino = ialloc(T_DEV);
  rinode(consoleino, &din);
  din.major = xshort(1);
  din.minor = xshort(1);
  winode(consoleino, &din);

  memset(&de, 0, sizeof(de));
  de.inum = xshort(consoleino);
  strcpy(de.name, "console");
  iappend(devino, &de, sizeof(de));

  // Create /dev/hd0
  uint hd0ino = ialloc(T_DEV);
  rinode(hd0ino, &din);
  din.major = xshort(2);
  din.minor = xshort(0);
  winode(hd0ino, &din);

  memset(&de, 0, sizeof(de));
  de.inum = xshort(hd0ino);
  strcpy(de.name, "hd0");
  iappend(devino, &de, sizeof(de));

  // Create /dev/hd1
  uint hd1ino = ialloc(T_DEV);
  rinode(hd1ino, &din);
  din.major = xshort(2);
  din.minor = xshort(1);
  winode(hd1ino, &din);

  memset(&de, 0, sizeof(de));
  de.inum = xshort(hd1ino);
  strcpy(de.name, "hd1");
  iappend(devino, &de, sizeof(de));

  for(i = 2; i < argc; i++){
    if((fd = open(argv[i], 0)) < 0){
      printf(2, "mkfs: cannot open %s\n", argv[i]);
      exit();
    }

    // Skip leading _ in name when writing to file system.
    // The binaries are named _rm, _cat, etc. to keep the
    // build operating system from trying to execute them
    // in place of system binaries like rm and cat.
    char *name = argv[i];
    uint target_ino = rootino;
    if(name[0] == '_'){
      name++;
      target_ino = binino;
    }

    inum = ialloc(T_FILE);

    memset(&de, 0, sizeof(de));
    de.inum = xshort(inum);
    strcpy(de.name, name);
    iappend(target_ino, &de, sizeof(de));

    while((cc = read(fd, buf, sizeof(buf))) > 0)
      iappend(inum, buf, cc);

    close(fd);
  }

  // fix size of root inode dir
  rinode(rootino, &din);
  off = xint(din.size);
  off = ((off/BSIZE) + 1) * BSIZE;
  din.size = xint(off);
  winode(rootino, &din);

  // fix size of bin inode dir
  rinode(binino, &din);
  off = xint(din.size);
  off = ((off/BSIZE) + 1) * BSIZE;
  din.size = xint(off);
  winode(binino, &din);

  // fix size of log inode dir
  rinode(logino, &din);
  off = xint(din.size);
  off = ((off/BSIZE) + 1) * BSIZE;
  din.size = xint(off);
  winode(logino, &din);

  balloc(freeblock);

  exit();
}

void
wsect(uint sec, void *buf)
{
  if(lseek(fsfd, sec * BSIZE, 0) != sec * BSIZE){
    printf(2, "lseek failed\n");
    exit();
  }
  if(write(fsfd, buf, BSIZE) != BSIZE){
    printf(2, "write failed\n");
    exit();
  }
}

void
winode(uint inum, struct dinode *ip)
{
  char buf[BSIZE];
  uint bn;
  struct dinode *dip;

  bn = IBLOCK(inum, sb);
  rsect(bn, buf);
  dip = ((struct dinode*)buf) + (inum % IPB);
  *dip = *ip;
  wsect(bn, buf);
}

void
rinode(uint inum, struct dinode *ip)
{
  char buf[BSIZE];
  uint bn;
  struct dinode *dip;

  bn = IBLOCK(inum, sb);
  rsect(bn, buf);
  dip = ((struct dinode*)buf) + (inum % IPB);
  *ip = *dip;
}

void
rsect(uint sec, void *buf)
{
  if(lseek(fsfd, sec * BSIZE, 0) != sec * BSIZE){
    printf(2, "lseek failed\n");
    exit();
  }
  if(read(fsfd, buf, BSIZE) != BSIZE){
    printf(2, "read failed\n");
    exit();
  }
}

uint
ialloc(ushort type)
{
  uint inum = freeinode++;
  struct dinode din;

  memset(&din, 0, sizeof(din));
  din.type = xshort(type);
  din.nlink = xshort(1);
  din.size = xint(0);
  winode(inum, &din);
  return inum;
}

void
balloc(int used)
{
  uchar buf[BSIZE];
  int i;

  printf(1, "balloc: first %d blocks have been allocated\n", used);
  memset(buf, 0, BSIZE);
  for(i = 0; i < used; i++){
    buf[i/8] = buf[i/8] | (0x1 << (i%8));
  }
  printf(1, "balloc: write bitmap block at sector %d\n", sb.bmapstart);
  wsect(sb.bmapstart, buf);
}

#define min(a, b) ((a) < (b) ? (a) : (b))

void
iappend(uint inum, void *xp, int n)
{
  char *p = (char*)xp;
  uint fbn, off, n1;
  struct dinode din;
  char buf[BSIZE];
  uint indirect[NINDIRECT];
  uint x;

  rinode(inum, &din);
  off = xint(din.size);
  while(n > 0){
    fbn = off / BSIZE;
    if(fbn < NDIRECT){
      if(xint(din.addrs[fbn]) == 0){
        din.addrs[fbn] = xint(freeblock++);
      }
      x = xint(din.addrs[fbn]);
    } else if(fbn < NDIRECT + NINDIRECT){
      if(xint(din.addrs[NDIRECT]) == 0){
        din.addrs[NDIRECT] = xint(freeblock++);
      }
      rsect(xint(din.addrs[NDIRECT]), (char*)indirect);
      if(indirect[fbn - NDIRECT] == 0){
        indirect[fbn - NDIRECT] = xint(freeblock++);
        wsect(xint(din.addrs[NDIRECT]), (char*)indirect);
      }
      x = xint(indirect[fbn-NDIRECT]);
    } else {
      if(xint(din.addrs[NDIRECT+1]) == 0){
        din.addrs[NDIRECT+1] = xint(freeblock++);
      }
      rsect(xint(din.addrs[NDIRECT+1]), (char*)indirect);
      if(indirect[(fbn - NDIRECT - NINDIRECT) / NINDIRECT] == 0){
        indirect[(fbn - NDIRECT - NINDIRECT) / NINDIRECT] = xint(freeblock++);
        wsect(xint(din.addrs[NDIRECT+1]), (char*)indirect);
      }
      x = xint(indirect[(fbn - NDIRECT - NINDIRECT) / NINDIRECT]);
      rsect(x, (char*)indirect);
      if(indirect[(fbn - NDIRECT - NINDIRECT) % NINDIRECT] == 0){
        indirect[(fbn - NDIRECT - NINDIRECT) % NINDIRECT] = xint(freeblock++);
        wsect(x, (char*)indirect);
      }
      x = xint(indirect[(fbn - NDIRECT - NINDIRECT) % NINDIRECT]);
    }
    n1 = min(n, (fbn + 1) * BSIZE - off);
    rsect(x, buf);
    memmove(buf + off - (fbn * BSIZE), p, n1);
    wsect(x, buf);
    n -= n1;
    off += n1;
    p += n1;
  }
  din.size = xint(off);
  winode(inum, &din);
}
