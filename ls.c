#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"

char*
fmtname(char *path)
{
  static char buf[DIRSIZ+1];
  char *p;

  // Find first character after last slash.
  for(p=path+strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;

  // Return blank-padded name.
  if(strlen(p) >= DIRSIZ)
    return p;
  memmove(buf, p, strlen(p));
  memset(buf+strlen(p), ' ', DIRSIZ-strlen(p));
  return buf;
}

void
fmtmode(int mode, char *buf)
{
  strcpy(buf, "---------");
  if(mode & T_DIR) buf[0] = 'd';
  if(mode & 0400) buf[1] = 'r';
  if(mode & 0200) buf[2] = 'w';
  if(mode & 0100) buf[3] = 'x';
  if(mode & 0040) buf[4] = 'r';
  if(mode & 0020) buf[5] = 'w';
  if(mode & 0010) buf[6] = 'x';
  if(mode & 0004) buf[7] = 'r';
  if(mode & 0002) buf[8] = 'w';
  if(mode & 0001) buf[9] = 'x';
}

void
ls(char *path)
{
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;
  char modebuf[11];

  if((fd = open(path, 0)) < 0){
    printf(2, "ls: cannot open %s\n", path);
    return;
  }

  if(fstat(fd, &st) < 0){
    printf(2, "ls: cannot stat %s\n", path);
    close(fd);
    return;
  }

  switch(st.type){
  case T_FILE:
    fmtmode(st.mode, modebuf);
    printf(1, "%s %s %d %d %d %d\n", fmtname(path), modebuf, st.nlink, st.uid, st.gid, st.size);
    break;

  case T_DIR:
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
      printf(1, "ls: path too long\n");
      break;
    }
    strcpy(buf, path);
    p = buf+strlen(buf);
    *p++ = '/';
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
      if(de.inum == 0)
        continue;
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
      if(stat(buf, &st) < 0){
        printf(1, "ls: cannot stat %s\n", buf);
        continue;
      }
      fmtmode(st.mode, modebuf);
      printf(1, "%s %s %d %d %d %d\n", fmtname(buf), modebuf, st.nlink, st.uid, st.gid, st.size);
    }
    break;
  }
  close(fd);
}

int
main(int argc, char *argv[])
{
  int i;

  if(argc < 2){
    ls(".");
    exit();
  }
  for(i=1; i<argc; i++)
    ls(argv[i]);
  exit();
}
