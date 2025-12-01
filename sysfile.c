//
// File-system system calls.
// Mostly argument checking, since we don't trust
// user code, and calls into file.c and fs.c.
//

#include "types.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "mmu.h"
#include "proc.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"
#include "fcntl.h"
#include "memlayout.h"

// Fetch the nth word-sized system call argument as a file descriptor
// and return both the descriptor and the corresponding struct file.

// Check if the current process has permission to access inode ip
// with the given mode (O_RDONLY, O_WRONLY, O_RDWR, or 1 for exec).
// Returns 0 on success, -1 on failure.
int
checkperm(struct inode *ip, int mode)
{
  struct proc *curproc = myproc();
  if(curproc->uid == 0)
    return 0; // Root can do anything

  int needed = 0;
  if(mode & O_WRONLY) needed |= 2;
  else if(mode & O_RDWR) needed |= 6;
  else if(mode & 1) needed |= 1; // Execute
  else needed |= 4; // O_RDONLY

  int perms = 0;
  if(curproc->uid == ip->uid)
    perms = (ip->mode >> 6) & 7;
  else if(curproc->gid == ip->gid)
    perms = (ip->mode >> 3) & 7;
  else
    perms = ip->mode & 7;

  if((needed & perms) == needed)
    return 0;
  return -1;
}

static int
argfd(int n, int *pfd, struct file **pf)
{
  int fd;
  struct file *f;

  if(argint(n, &fd) < 0)
    return -1;
  if(fd < 0 || fd >= NOFILE || (f=myproc()->ofile[fd]) == 0)
    return -1;
  if(pfd)
    *pfd = fd;
  if(pf)
    *pf = f;
  return 0;
}

// Allocate a file descriptor for the given file.
// Takes over file reference from caller on success.
static int
fdalloc(struct file *f)
{
  int fd;
  struct proc *curproc = myproc();

  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd] == 0){
      curproc->ofile[fd] = f;
      return fd;
    }
  }
  return -1;
}

int
sys_dup(void)
{
  struct file *f;
  int fd;

  if(argfd(0, 0, &f) < 0)
    return -1;
  if((fd=fdalloc(f)) < 0)
    return -1;
  filedup(f);
  return fd;
}

int
sys_read(void)
{
  struct file *f;
  int n;
  char *p;

  if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argptr(1, &p, n) < 0)
    return -1;
  return fileread(f, p, n);
}

int
sys_write(void)
{
  struct file *f;
  int n;
  char *p;

  if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argptr(1, &p, n) < 0)
    return -1;
  return filewrite(f, p, n);
}

int
sys_close(void)
{
  int fd;
  struct file *f;

  if(argfd(0, &fd, &f) < 0)
    return -1;
  myproc()->ofile[fd] = 0;
  fileclose(f);
  return 0;
}

int
sys_fstat(void)
{
  struct file *f;
  struct stat *st;

  if(argfd(0, 0, &f) < 0 || argptr(1, (void*)&st, sizeof(*st)) < 0)
    return -1;
  return filestat(f, st);
}

// Create the path new as a link to the same inode as old.
int
sys_link(void)
{
  char name[DIRSIZ], *new, *old;
  struct inode *dp, *ip;

  if(argstr(0, &old) < 0 || argstr(1, &new) < 0)
    return -1;

  begin_op();
  if((ip = namei(old)) == 0){
    end_op();
    return -1;
  }

  ilock(ip);
  if(ip->type == T_DIR){
    iunlockput(ip);
    end_op();
    return -1;
  }

  ip->nlink++;
  iupdate(ip);
  iunlock(ip);

  if((dp = nameiparent(new, name)) == 0)
    goto bad;
  ilock(dp);
  if(dp->dev != ip->dev || dirlink(dp, name, ip->inum) < 0){
    iunlockput(dp);
    goto bad;
  }
  if(checkperm(dp, O_WRONLY) < 0){
    iunlockput(dp);
    goto bad;
  }
  iunlockput(dp);
  iput(ip);

  end_op();

  return 0;

bad:
  ilock(ip);
  ip->nlink--;
  iupdate(ip);
  iunlockput(ip);
  end_op();
  return -1;
}

// Is the directory dp empty except for "." and ".." ?
static int
isdirempty(struct inode *dp)
{
  int off;
  struct dirent de;

  for(off=2*sizeof(de); off<dp->size; off+=sizeof(de)){
    if(readi(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
      panic("isdirempty: readi");
    if(de.inum != 0)
      return 0;
  }
  return 1;
}

//PAGEBREAK!
int
sys_unlink(void)
{
  struct inode *ip, *dp;
  struct dirent de;
  char name[DIRSIZ], *path;
  uint off;

  if(argstr(0, &path) < 0)
    return -1;

  begin_op();
  if((dp = nameiparent(path, name)) == 0){
    end_op();
    return -1;
  }

  ilock(dp);

  if(checkperm(dp, O_WRONLY) < 0){
    iunlockput(dp);
    end_op();
    return -1;
  }

  // Cannot unlink "." or "..".
  if(namecmp(name, ".") == 0 || namecmp(name, "..") == 0)
    goto bad;

  if((ip = dirlookup(dp, name, &off)) == 0)
    goto bad;
  ilock(ip);

  if(ip->nlink < 1)
    panic("unlink: nlink < 1");
  if(ip->type == T_DIR && !isdirempty(ip)){
    iunlockput(ip);
    goto bad;
  }

  memset(&de, 0, sizeof(de));
  if(writei(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
    panic("unlink: writei");
  if(ip->type == T_DIR){
    dp->nlink--;
    iupdate(dp);
  }
  iunlockput(dp);

  ip->nlink--;
  iupdate(ip);
  iunlockput(ip);

  end_op();

  return 0;

bad:
  iunlockput(dp);
  end_op();
  return -1;
}

static struct inode*
create(char *path, short type, short major, short minor)
{
  struct inode *ip, *dp;
  char name[DIRSIZ];

  if((dp = nameiparent(path, name)) == 0)
    return 0;
  ilock(dp);
  if(checkperm(dp, O_WRONLY) < 0){
    iunlockput(dp);
    return 0;
  }

  if((ip = dirlookup(dp, name, 0)) != 0){
    iunlockput(dp);
    ilock(ip);
    if(type == T_FILE && ip->type == T_FILE)
      return ip;
    iunlockput(ip);
    return 0;
  }

  if((ip = ialloc(dp->dev, type)) == 0)
    panic("create: ialloc");

  ilock(ip);
  ip->major = major;
  ip->minor = minor;
  ip->nlink = 1;
  iupdate(ip);

  if(type == T_DIR){  // Create . and .. entries.
    dp->nlink++;  // for ".."
    iupdate(dp);
    // No ip->nlink++ for ".": avoid cyclic ref count.
    if(dirlink(ip, ".", ip->inum) < 0 || dirlink(ip, "..", dp->inum) < 0)
      panic("create dots");
  }

  if(dirlink(dp, name, ip->inum) < 0)
    panic("create: dirlink");

  iunlockput(dp);

  return ip;
}

int
sys_open(void)
{
  char *path;
  int fd, omode;
  struct file *f;
  struct inode *ip;

  if(argstr(0, &path) < 0 || argint(1, &omode) < 0)
    return -1;

  begin_op();

  if(omode & O_CREATE){
    ip = create(path, T_FILE, 0, 0);
    if(ip == 0){
      end_op();
      return -1;
    }
  } else {
    if((ip = namei(path)) == 0){
      end_op();
      return -1;
    }
    ilock(ip);
    if(ip->type == T_DIR && omode != O_RDONLY){
      iunlockput(ip);
      end_op();
      return -1;
    }
    if(ip->type == T_DIR && omode != O_RDONLY){
      iunlockput(ip);
      end_op();
      return -1;
    }
    if(checkperm(ip, omode) < 0){
      iunlockput(ip);
      end_op();
      return -1;
    }
  }

  if((f = filealloc()) == 0 || (fd = fdalloc(f)) < 0){
    if(f)
      fileclose(f);
    iunlockput(ip);
    end_op();
    return -1;
  }
  iunlock(ip);
  end_op();

  f->type = FD_INODE;
  f->ip = ip;
  f->off = 0;
  f->readable = !(omode & O_WRONLY);
  f->writable = (omode & O_WRONLY) || (omode & O_RDWR);
  return fd;
}

int
sys_mkdir(void)
{
  char *path;
  struct inode *ip;

  begin_op();
  if(argstr(0, &path) < 0 || (ip = create(path, T_DIR, 0, 0)) == 0){
    end_op();
    return -1;
  }
  iunlockput(ip);
  end_op();
  return 0;
}

int
sys_mknod(void)
{
  struct inode *ip;
  char *path;
  int major, minor;

  begin_op();
  if((argstr(0, &path)) < 0 ||
     argint(1, &major) < 0 ||
     argint(2, &minor) < 0 ||
     (ip = create(path, T_DEV, major, minor)) == 0){
    end_op();
    return -1;
  }
  iunlockput(ip);
  end_op();
  return 0;
}

int
sys_chdir(void)
{
  char *path;
  struct inode *ip;
  struct proc *curproc = myproc();
  
  begin_op();
  if(argstr(0, &path) < 0 || (ip = namei(path)) == 0){
    end_op();
    return -1;
  }
  ilock(ip);
  if(ip->type != T_DIR){
    iunlockput(ip);
    end_op();
    return -1;
  }
  iunlock(ip);
  iput(curproc->cwd);
  end_op();
  curproc->cwd = ip;
  return 0;
}

int
sys_exec(void)
{
  char *path, *argv[MAXARG];
  int i;
  uint uargv, uarg;

  if(argstr(0, &path) < 0 || argint(1, (int*)&uargv) < 0){
    return -1;
  }
  memset(argv, 0, sizeof(argv));
  for(i=0;; i++){
    if(i >= NELEM(argv))
      return -1;
    if(fetchint(uargv+4*i, (int*)&uarg) < 0)
      return -1;
    if(uarg == 0){
      argv[i] = 0;
      break;
    }
    if(fetchstr(uarg, &argv[i]) < 0)
      return -1;
  }
  return exec(path, argv);
}

int
sys_pipe(void)
{
  int *fd;
  struct file *rf, *wf;
  int fd0, fd1;

  if(argptr(0, (void*)&fd, 2*sizeof(fd[0])) < 0)
    return -1;
  if(pipealloc(&rf, &wf) < 0)
    return -1;
  fd0 = -1;
  if((fd0 = fdalloc(rf)) < 0 || (fd1 = fdalloc(wf)) < 0){
    if(fd0 >= 0)
      myproc()->ofile[fd0] = 0;
    fileclose(rf);
    fileclose(wf);
    return -1;
  }
  fd[0] = fd0;
  fd[1] = fd1;
  return 0;
}

int
sys_setconsolemode(void)
{
  int mode;

  if(argint(0, &mode) < 0)
    return -1;
  
  console_setmode(mode);
  return 0;
}

int
sys_mount(void)
{
  char *dev, *dir;
  struct inode *ip, *devip;
  int minor;

  if(argstr(0, &dev) < 0 || argstr(1, &dir) < 0)
    return -1;

  begin_op();
  if((devip = namei(dev)) == 0){
    end_op();
    return -1;
  }
  ilock(devip);
  if(devip->type != T_DEV){
    iunlockput(devip);
    end_op();
    return -1;
  }
  minor = devip->minor;
  iunlockput(devip);

  if((ip = namei(dir)) == 0){
    end_op();
    return -1;
  }
  ilock(ip);
  if(ip->type != T_DIR){
    iunlockput(ip);
    end_op();
    return -1;
  }
  iunlock(ip);

  if(mount(ip, minor) < 0){
    iput(ip);
    end_op();
    return -1;
  }

  iput(ip);
  end_op();
  return 0;
}

int
sys_umount(void)
{
  char *path;
  struct inode *ip;

  if(argstr(0, &path) < 0)
    return -1;

  begin_op();
  if((ip = namei(path)) == 0){
    end_op();
    return -1;
  }

  if(umount(ip) < 0){
    iput(ip);
    end_op();
    return -1;
  }

  iput(ip);
  end_op();
  return 0;
}

int
sys_lseek(void)
{
  int fd;
  int offset;
  int whence;
  struct file *f;

  if(argfd(0, &fd, &f) < 0 || argint(1, &offset) < 0 || argint(2, &whence) < 0)
    return -1;

  if(f->type == FD_PIPE)
    return -1;

  if(whence == 0) // SEEK_SET
    f->off = offset;
  else if(whence == 1) // SEEK_CUR
    f->off += offset;
  else if(whence == 2) // SEEK_END
    f->off = f->ip->size + offset;
  else
    return -1;

  return f->off;
}

int
sys_setvideomode(void)
{
  int mode;
  if(argint(0, &mode) < 0)
    return -1;
  vga_set_mode(mode);
  return 0;
}

int
sys_mapvga(void)
{
  int va;
  if(argint(0, &va) < 0)
    return -1;
  if((uint)va >= KERNBASE || (uint)va + 64*1024 > KERNBASE)
    return -1;
  return mapvga(myproc()->pgdir, (uint)va);
}

int
sys_chown(void)
{
  char *path;
  int uid, gid;
  struct inode *ip;

  if(argstr(0, &path) < 0 || argint(1, &uid) < 0 || argint(2, &gid) < 0)
    return -1;

  begin_op();
  if((ip = namei(path)) == 0){
    end_op();
    return -1;
  }
  ilock(ip);
  if(myproc()->uid != 0 && myproc()->uid != ip->uid){
    iunlockput(ip);
    end_op();
    return -1;
  }
  ip->uid = uid;
  ip->gid = gid;
  iupdate(ip);
  iunlockput(ip);
  end_op();
  return 0;
}

int
sys_chmod(void)
{
  char *path;
  int mode;
  struct inode *ip;

  if(argstr(0, &path) < 0 || argint(1, &mode) < 0)
    return -1;

  begin_op();
  if((ip = namei(path)) == 0){
    end_op();
    return -1;
  }
  ilock(ip);
  if(myproc()->uid != 0 && myproc()->uid != ip->uid){
    iunlockput(ip);
    end_op();
    return -1;
  }
  // Preserve high bits (file type) if any
  ip->mode = (ip->mode & ~0777) | (mode & 0777);
  iupdate(ip);
  iunlockput(ip);
  end_op();
  return 0;
}

extern struct mount mounts[NMOUNT];
extern struct spinlock mount_lock;

static int
namebyinum(struct inode *dp, uint inum, char *name)
{
  struct dirent de;
  uint off;

  for(off = 0; off < dp->size; off += sizeof(de)){
    if(readi(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
      panic("namebyinum: readi");
    if(de.inum == inum){
      strncpy(name, de.name, DIRSIZ);
      name[DIRSIZ] = 0;
      return 0;
    }
  }
  return -1;
}

int
sys_getcwd(void)
{
  char *buf;
  int size;
  struct inode *ip, *pip;
  char name[DIRSIZ+1];
  struct proc *curproc = myproc();
  char temp[512];
  int pos = 512 - 1;
  int i;

  if(argptr(0, &buf, 1) < 0 || argint(1, &size) < 0)
    return -1;

  if(size < 2) return -1;

  ip = idup(curproc->cwd);
  temp[pos] = 0;

  begin_op();

  while(1){
    ilock(ip);
    pip = dirlookup(ip, "..", 0);
    if(pip == 0){
      iunlockput(ip);
      end_op();
      return -1;
    }
    
    if(ip->dev == pip->dev && ip->inum == pip->inum){
      // We are at a root. Check if it's a mount point.
      int found = 0;
      acquire(&mount_lock);
      for(i = 0; i < NMOUNT; i++){
        if(mounts[i].active && mounts[i].dev == ip->dev){
          // Found mount point
          struct inode *mnt_ip = mounts[i].ip;
          if(mnt_ip){
             // Switch to mount point inode
             iunlockput(pip); // Put the ".." (which is same as ip)
             iunlockput(ip);  // Put the current root
             ip = idup(mnt_ip); // Switch to mount point
             found = 1;
             break;
          }
        }
      }
      release(&mount_lock);
      
      if(found) continue; // Continue with new ip (mount point)
      
      // Real root
      iunlockput(pip);
      iunlockput(ip);
      break;
    }
    
    iunlock(ip);
    ilock(pip);
    
    if(namebyinum(pip, ip->inum, name) < 0){
      iunlockput(pip);
      iput(ip);
      end_op();
      return -1;
    }
    
    int len = strlen(name);
    if(pos - len - 1 < 0){
      iunlockput(pip);
      iput(ip);
      end_op();
      return -1;
    }
    
    pos -= len;
    memmove(temp + pos, name, len);
    pos--;
    temp[pos] = '/';
    
    iunlock(pip);
    iput(ip);
    ip = pip;
  }
  
  end_op();
  
  if(pos == 512 - 1) {
    if(pos - 1 < 0) return -1;
    pos--;
    temp[pos] = '/';
  }
  
  if(size < 512 - pos){
    return -1;
  }
  
  memmove(buf, temp + pos, 512 - pos);
  return 0;
}
