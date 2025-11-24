#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"
#include "fs.h"

char *sh_argv[] = { "sh", 0 };
int sh_pid = 0;

void
get_service_name(char *path, char *name)
{
  char *p = path + strlen(path);
  while(p >= path && *p != '/')
    p--;
  p++;
  strcpy(name, p);
}

void
start_service(char *path)
{
  int pid = fork();
  if(pid < 0){
    printf(1, "init: fork failed\n");
    return;
  }
  if(pid == 0){
    char name[64];
    get_service_name(path, name);
    char logpath[128];
    strcpy(logpath, "/log/");
    strcpy(logpath + 5, name);
    
    close(1);
    close(2);
    if(open(logpath, O_CREATE|O_WRONLY) < 0){
       mknod(logpath, 1, 1);
       open(logpath, O_CREATE|O_WRONLY);
    }
    dup(1); 
    
    exec(path, (char*[]){path, 0});
    printf(1, "init: exec %s failed\n", path);
    exit();
  }
  
  char name[64];
  get_service_name(path, name);
  char pidpath[128];
  strcpy(pidpath, "/var/run/");
  strcpy(pidpath + 9, name);
  strcpy(pidpath + 9 + strlen(name), ".pid");
  
  int fd = open(pidpath, O_CREATE|O_WRONLY);
  if(fd >= 0){
    printf(fd, "%d", pid);
    close(fd);
  }
}

void
stop_service(char *path)
{
  char name[64];
  get_service_name(path, name);
  char pidpath[128];
  strcpy(pidpath, "/var/run/");
  strcpy(pidpath + 9, name);
  strcpy(pidpath + 9 + strlen(name), ".pid");
  
  int fd = open(pidpath, O_RDONLY);
  if(fd < 0){
    printf(1, "Service %s not running\n", name);
    return;
  }
  char buf[16];
  int n = read(fd, buf, sizeof(buf)-1);
  close(fd);
  if(n > 0){
    buf[n] = 0;
    int pid = atoi(buf);
    kill(pid);
  }
}

void
enable_service(char *path)
{
  int fd = open("/etc/services", O_RDWR|O_CREATE);
  if(fd < 0) return;
  
  char buf[1024];
  int n;
  char line[128];
  int line_idx = 0;
  
  // Check if already enabled
  while((n = read(fd, buf, sizeof(buf))) > 0){
    for(int i=0; i<n; i++){
      if(buf[i] == '\n'){
        line[line_idx] = 0;
        if(strcmp(line, path) == 0){
            close(fd);
            return;
        }
        line_idx = 0;
      } else {
        if(line_idx < sizeof(line)-1)
          line[line_idx++] = buf[i];
      }
    }
  }

  write(fd, path, strlen(path));
  write(fd, "\n", 1);
  close(fd);
}

void
disable_service(char *path)
{
  int fd = open("/etc/services", O_RDONLY);
  if(fd < 0) return;
  
  int tfd = open("/etc/services.tmp", O_WRONLY|O_CREATE);
  if(tfd < 0) { close(fd); return; }
  
  char buf[1024];
  int n;
  char line[128];
  int line_idx = 0;
  
  while((n = read(fd, buf, sizeof(buf))) > 0){
    for(int i=0; i<n; i++){
      if(buf[i] == '\n'){
        line[line_idx] = 0;
        if(strcmp(line, path) != 0){
          write(tfd, line, line_idx);
          write(tfd, "\n", 1);
        }
        line_idx = 0;
      } else {
        if(line_idx < sizeof(line)-1)
          line[line_idx++] = buf[i];
      }
    }
  }
  close(fd);
  close(tfd);
  
  unlink("/etc/services");
  link("/etc/services.tmp", "/etc/services");
  unlink("/etc/services.tmp");
}

void
list_services()
{
  int fd = open("/etc/services", O_RDONLY);
  if(fd < 0) return;
  
  char buf[1024];
  int n;
  char line[128];
  int line_idx = 0;
  
  printf(1, "Services:\n");
  while((n = read(fd, buf, sizeof(buf))) > 0){
    for(int i=0; i<n; i++){
      if(buf[i] == '\n'){
        line[line_idx] = 0;
        if(line_idx > 0) {
            char name[64];
            get_service_name(line, name);
            char pidpath[128];
            strcpy(pidpath, "/var/run/");
            strcpy(pidpath + 9, name);
            strcpy(pidpath + 9 + strlen(name), ".pid");
            
            int pfd = open(pidpath, O_RDONLY);
            if(pfd >= 0){
                printf(1, "%s: Running\n", line);
                close(pfd);
            } else {
                printf(1, "%s: Stopped\n", line);
            }
        }
        line_idx = 0;
      } else {
        if(line_idx < sizeof(line)-1)
          line[line_idx++] = buf[i];
      }
    }
  }
  close(fd);
}

void
start_sh()
{
  printf(1, "init: starting sh\n");
  sh_pid = fork();
  if(sh_pid < 0){
    printf(1, "init: fork failed\n");
    exit();
  }
  if(sh_pid == 0){
    exec("sh", sh_argv);
    printf(1, "init: exec sh failed\n");
    exit();
  }
}

int
main(int argc, char *argv[])
{
  if(argc > 1){
    if(strcmp(argv[1], "enable") == 0 && argc > 2){
      enable_service(argv[2]);
    } else if(strcmp(argv[1], "disable") == 0 && argc > 2){
      disable_service(argv[2]);
    } else if(strcmp(argv[1], "start") == 0 && argc > 2){
      start_service(argv[2]);
    } else if(strcmp(argv[1], "stop") == 0 && argc > 2){
      stop_service(argv[2]);
    } else if(strcmp(argv[1], "list") == 0){
      list_services();
    } else {
      printf(1, "Usage: init {enable|disable|start|stop|list} [arg]\n");
    }
    exit();
  }

  if(open("console", O_RDWR) < 0){
    mknod("console", 1, 1);
    open("console", O_RDWR);
  }
  dup(0);  // stdout
  dup(0);  // stderr

  int fd = open("/etc/services", O_RDONLY);
  if(fd >= 0){
      char buf[1024];
      int n;
      char line[128];
      int line_idx = 0;
      while((n = read(fd, buf, sizeof(buf))) > 0){
        for(int i=0; i<n; i++){
          if(buf[i] == '\n'){
            line[line_idx] = 0;
            if(line_idx > 0) start_service(line);
            line_idx = 0;
          } else {
            if(line_idx < sizeof(line)-1)
              line[line_idx++] = buf[i];
          }
        }
      }
      close(fd);
  }

  start_sh();

  for(;;){
    int wpid = wait();
    if(wpid == sh_pid){
      start_sh();
    } else if(wpid > 0){
      int dfd = open("/var/run", O_RDONLY);
      if(dfd >= 0){
        struct dirent de;
        while(read(dfd, &de, sizeof(de)) == sizeof(de)){
          if(de.inum == 0) continue;
          if(strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0) continue;
          
          char pidpath[128];
          strcpy(pidpath, "/var/run/");
          strcpy(pidpath + 9, de.name);
          
          int pfd = open(pidpath, O_RDONLY);
          if(pfd >= 0){
             char buf[16];
             int n = read(pfd, buf, sizeof(buf)-1);
             close(pfd);
             if(n > 0){
               buf[n] = 0;
               if(atoi(buf) == wpid){
                 unlink(pidpath);
                 break; 
               }
             }
          }
        }
        close(dfd);
      }
    }
  }
}
