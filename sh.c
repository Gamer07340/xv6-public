// Shell.

#include "types.h"
#include "user.h"
#include "fcntl.h"

// Parsed command representation
#define EXEC  1
#define REDIR 2
#define PIPE  3
#define LIST  4
#define BACK  5

#define MAXARGS 10

struct cmd {
  int type;
};

struct execcmd {
  int type;
  char *argv[MAXARGS];
  char *eargv[MAXARGS];
};

struct redircmd {
  int type;
  struct cmd *cmd;
  char *file;
  char *efile;
  int mode;
  int fd;
};

struct pipecmd {
  int type;
  struct cmd *left;
  struct cmd *right;
};

struct listcmd {
  int type;
  struct cmd *left;
  struct cmd *right;
};

struct backcmd {
  int type;
  struct cmd *cmd;
};

int fork1(void);  // Fork but panics on failure.
void panic(char*);
struct cmd *parsecmd(char*);

// Execute cmd.  Never returns.
void
runcmd(struct cmd *cmd)
{
  int p[2];
  struct backcmd *bcmd;
  struct execcmd *ecmd;
  struct listcmd *lcmd;
  struct pipecmd *pcmd;
  struct redircmd *rcmd;

  if(cmd == 0)
    exit();

  switch(cmd->type){
  default:
    panic("runcmd");

  case EXEC:
    ecmd = (struct execcmd*)cmd;
    if(ecmd->argv[0] == 0)
      exit();
    exec(ecmd->argv[0], ecmd->argv);
    printf(2, "exec %s failed\n", ecmd->argv[0]);
    break;

  case REDIR:
    rcmd = (struct redircmd*)cmd;
    close(rcmd->fd);
    if(open(rcmd->file, rcmd->mode) < 0){
      printf(2, "open %s failed\n", rcmd->file);
      exit();
    }
    runcmd(rcmd->cmd);
    break;

  case LIST:
    lcmd = (struct listcmd*)cmd;
    if(fork1() == 0)
      runcmd(lcmd->left);
    wait();
    runcmd(lcmd->right);
    break;

  case PIPE:
    pcmd = (struct pipecmd*)cmd;
    if(pipe(p) < 0)
      panic("pipe");
    if(fork1() == 0){
      close(1);
      dup(p[1]);
      close(p[0]);
      close(p[1]);
      runcmd(pcmd->left);
    }
    if(fork1() == 0){
      close(0);
      dup(p[0]);
      close(p[0]);
      close(p[1]);
      runcmd(pcmd->right);
    }
    close(p[0]);
    close(p[1]);
    wait();
    wait();
    break;

  case BACK:
    bcmd = (struct backcmd*)cmd;
    if(fork1() == 0){
      char logpath[32];
      // We don't have sprintf, so we construct the string manually or use a simple helper if available.
      // Since we are in the shell (user space), we can't use kernel cprintf.
      // We'll rely on the fact that we can just open the file.
      // But wait, we need the PID of the *child* to name the file.
      // The child knows its own PID via getpid().
      
      int pid = getpid();
      // Construct "/log/PID"
      // Simple integer to string conversion
      char pid_str[16];
      int i = 0;
      int n = pid;
      if(n == 0) pid_str[i++] = '0';
      else {
        while(n > 0){
          pid_str[i++] = (n % 10) + '0';
          n /= 10;
        }
      }
      pid_str[i] = 0;
      // Reverse the string
      int start = 0, end = i - 1;
      while(start < end){
        char temp = pid_str[start];
        pid_str[start] = pid_str[end];
        pid_str[end] = temp;
        start++; end--;
      }

      strcpy(logpath, "/log/");
      strcpy(logpath + 5, pid_str);

      close(1);
      if(open(logpath, O_CREATE|O_WRONLY) < 0){
        printf(2, "failed to open log file %s\n", logpath);
        exit();
      }
      runcmd(bcmd->cmd);
    }
    break;
  }
  exit();
}

#include "fs.h"

#define KEY_UP 0xE2
#define KEY_DOWN 0xE3
#define KEY_LEFT 0xE4
#define KEY_RIGHT 0xE5
#define BACKSPACE 127
#define CTRL_D 4
#define TAB 9

#define MAX_HISTORY 10
char history[MAX_HISTORY][100];
int history_count = 0;

void
history_add(char *cmd)
{
  if(cmd[0] == 0) return;
  // Simple append, no rotation for now (or rotate if full)
  if(history_count < MAX_HISTORY){
    strcpy(history[history_count], cmd);
    history_count++;
  } else {
    // Rotate
    int i;
    for(i = 0; i < MAX_HISTORY - 1; i++){
      strcpy(history[i], history[i+1]);
    }
    strcpy(history[MAX_HISTORY-1], cmd);
  }
}

void
autocomplete(char *buf, int *pos, int *len)
{
  // 1. Find start of current word
  int word_start = *pos;
  while(word_start > 0 && buf[word_start-1] != ' '){
    word_start--;
  }
  
  // 2. Determine if it's the first word
  int is_first_word = 1;
  int i;
  for(i = 0; i < word_start; i++){
    if(buf[i] != ' '){
      is_first_word = 0;
      break;
    }
  }
  
  // 3. Get prefix
  char prefix[100];
  int prefix_len = *pos - word_start;
  if(prefix_len == 0) return; // Nothing to complete
  memmove(prefix, buf + word_start, prefix_len);
  prefix[prefix_len] = 0;
  
  // 4. Open directory
  int fd;
  if(is_first_word){
    fd = open("/bin", 0); // O_RDONLY
    if(fd < 0) fd = open(".", 0); // Fallback
  } else {
    fd = open(".", 0);
  }
  if(fd < 0) return;
  
  // 5. Search
  struct dirent de;
  while(read(fd, &de, sizeof(de)) == sizeof(de)){
    if(de.inum == 0) continue;
    
    char name[DIRSIZ+1];
    memmove(name, de.name, DIRSIZ);
    name[DIRSIZ] = 0;
    
    // Check prefix match
    int match = 1;
    for(i = 0; i < prefix_len; i++){
      if(name[i] != prefix[i]){
        match = 0;
        break;
      }
    }
    
    if(match){
      // Found a match!
      char *suffix = name + prefix_len;
      int suffix_len = strlen(suffix);
      
      if(suffix_len > 0 && *len + suffix_len < 100 - 1){
        // Shift right
        for(i = *len; i >= *pos; i--){
           buf[i + suffix_len] = buf[i];
        }
        
        // Copy suffix
        memmove(buf + *pos, suffix, suffix_len);
        
        // Update len and pos
        *len += suffix_len;
        *pos += suffix_len;
        
        // Print suffix
        for(i = 0; i < suffix_len; i++) printf(1, "%c", suffix[i]);
        
        // Print rest of line
        int rest_len = *len - *pos;
        for(i = 0; i < rest_len; i++) printf(1, "%c", buf[*pos + i]);
        
        // Move cursor back
        for(i = 0; i < rest_len; i++) printf(1, "\b");
      }
      break; // Stop after first match
    }
  }
  
  close(fd);
}

int
getcmd(char *buf, int nbuf)
{
  printf(2, "$ ");
  memset(buf, 0, nbuf);
  
  setconsolemode(1);
  
  int pos = 0;
  int len = 0;
  int hist_pos = history_count;
  uchar c;
  int i;
  
  while(1){
    if(read(0, &c, 1) != 1) break;
    
    if(c == 0) continue;
    
    if(c == KEY_UP){
      if(hist_pos > 0){
        // Erase current line
        while(pos > 0){
          printf(1, "\b");
          pos--;
        }
        for(i = 0; i < len; i++) printf(1, " ");
        for(i = 0; i < len; i++) printf(1, "\b");
        
        hist_pos--;
        strcpy(buf, history[hist_pos]);
        len = strlen(buf);
        pos = len;
        printf(1, "%s", buf);
      }
    } else if(c == KEY_DOWN){
      if(hist_pos < history_count){
        // Erase current line
        while(pos > 0){
          printf(1, "\b");
          pos--;
        }
        for(i = 0; i < len; i++) printf(1, " ");
        for(i = 0; i < len; i++) printf(1, "\b");
        
        hist_pos++;
        if(hist_pos == history_count){
          buf[0] = 0;
          len = 0;
          pos = 0;
        } else {
          strcpy(buf, history[hist_pos]);
          len = strlen(buf);
          pos = len;
          printf(1, "%s", buf);
        }
      }
    } else if(c == KEY_LEFT){
      if(pos > 0){
        printf(1, "\b");
        pos--;
      }
    } else if(c == KEY_RIGHT){
      if(pos < len){
        printf(1, "%c", buf[pos]);
        pos++;
      }
    } else if(c == BACKSPACE || c == '\b'){
      if(pos > 0){
        // Shift left
        for(i = pos; i < len; i++){
          buf[i-1] = buf[i];
        }
        len--;
        pos--;
        buf[len] = 0;
        
        // Update screen
        printf(1, "\b"); // Move back
        for(i = pos; i < len; i++){
          printf(1, "%c", buf[i]);
        }
        printf(1, " "); // Erase last char
        // Move cursor back to pos
        for(i = pos; i <= len; i++){ // +1 for the space we just printed
          printf(1, "\b");
        }
      }
    } else if(c == TAB){
      autocomplete(buf, &pos, &len);
    } else if(c == '\r' || c == '\n'){
      printf(1, "\n");
      buf[len] = 0;
      history_add(buf);
      break;
    } else if(c == CTRL_D){
      if(len == 0){
        setconsolemode(0);
        return -1;
      }
    } else if(c >= 32 && c <= 126){
      if(len < nbuf - 1){
        // Shift right
        for(i = len; i > pos; i--){
          buf[i] = buf[i-1];
        }
        buf[pos] = c;
        len++;
        pos++;
        buf[len] = 0;
        
        // Update screen
        printf(1, "%c", c);
        for(i = pos; i < len; i++){
          printf(1, "%c", buf[i]);
        }
        // Move cursor back
        for(i = pos; i < len; i++){
          printf(1, "\b");
        }
      }
    }
  }
  
  setconsolemode(0);
  return 0;
}

int
main(void)
{
  static char buf[100];
  int fd;

  // Ensure that three file descriptors are open.
  while((fd = open("console", O_RDWR)) >= 0){
    if(fd >= 3){
      close(fd);
      break;
    }
  }

  // Read and run input commands.
  while(getcmd(buf, sizeof(buf)) >= 0){
    if(buf[0] == 'c' && buf[1] == 'd' && buf[2] == ' '){
      // Chdir must be called by the parent, not the child.
      if(chdir(buf+3) < 0)
        printf(2, "cannot cd %s\n", buf+3);
      continue;
    }
    if(fork1() == 0)
      runcmd(parsecmd(buf));
    wait();
  }
  exit();
}

void
panic(char *s)
{
  printf(2, "%s\n", s);
  exit();
}

int
fork1(void)
{
  int pid;

  pid = fork();
  if(pid == -1)
    panic("fork");
  return pid;
}

//PAGEBREAK!
// Constructors

struct cmd*
execcmd(void)
{
  struct execcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = EXEC;
  return (struct cmd*)cmd;
}

struct cmd*
redircmd(struct cmd *subcmd, char *file, char *efile, int mode, int fd)
{
  struct redircmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = REDIR;
  cmd->cmd = subcmd;
  cmd->file = file;
  cmd->efile = efile;
  cmd->mode = mode;
  cmd->fd = fd;
  return (struct cmd*)cmd;
}

struct cmd*
pipecmd(struct cmd *left, struct cmd *right)
{
  struct pipecmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = PIPE;
  cmd->left = left;
  cmd->right = right;
  return (struct cmd*)cmd;
}

struct cmd*
listcmd(struct cmd *left, struct cmd *right)
{
  struct listcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = LIST;
  cmd->left = left;
  cmd->right = right;
  return (struct cmd*)cmd;
}

struct cmd*
backcmd(struct cmd *subcmd)
{
  struct backcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = BACK;
  cmd->cmd = subcmd;
  return (struct cmd*)cmd;
}
//PAGEBREAK!
// Parsing

char whitespace[] = " \t\r\n\v";
char symbols[] = "<|>&;()";

int
gettoken(char **ps, char *es, char **q, char **eq)
{
  char *s;
  int ret;

  s = *ps;
  while(s < es && strchr(whitespace, *s))
    s++;
  if(q)
    *q = s;
  ret = *s;
  switch(*s){
  case 0:
    break;
  case '|':
  case '(':
  case ')':
  case ';':
  case '&':
  case '<':
    s++;
    break;
  case '>':
    s++;
    if(*s == '>'){
      ret = '+';
      s++;
    }
    break;
  default:
    ret = 'a';
    while(s < es && !strchr(whitespace, *s) && !strchr(symbols, *s))
      s++;
    break;
  }
  if(eq)
    *eq = s;

  while(s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  return ret;
}

int
peek(char **ps, char *es, char *toks)
{
  char *s;

  s = *ps;
  while(s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  return *s && strchr(toks, *s);
}

struct cmd *parseline(char**, char*);
struct cmd *parsepipe(char**, char*);
struct cmd *parseexec(char**, char*);
struct cmd *nulterminate(struct cmd*);

struct cmd*
parsecmd(char *s)
{
  char *es;
  struct cmd *cmd;

  es = s + strlen(s);
  cmd = parseline(&s, es);
  peek(&s, es, "");
  if(s != es){
    printf(2, "leftovers: %s\n", s);
    panic("syntax");
  }
  nulterminate(cmd);
  return cmd;
}

struct cmd*
parseline(char **ps, char *es)
{
  struct cmd *cmd;

  cmd = parsepipe(ps, es);
  while(peek(ps, es, "&")){
    gettoken(ps, es, 0, 0);
    cmd = backcmd(cmd);
  }
  if(peek(ps, es, ";")){
    gettoken(ps, es, 0, 0);
    cmd = listcmd(cmd, parseline(ps, es));
  }
  return cmd;
}

struct cmd*
parsepipe(char **ps, char *es)
{
  struct cmd *cmd;

  cmd = parseexec(ps, es);
  if(peek(ps, es, "|")){
    gettoken(ps, es, 0, 0);
    cmd = pipecmd(cmd, parsepipe(ps, es));
  }
  return cmd;
}

struct cmd*
parseredirs(struct cmd *cmd, char **ps, char *es)
{
  int tok;
  char *q, *eq;

  while(peek(ps, es, "<>")){
    tok = gettoken(ps, es, 0, 0);
    if(gettoken(ps, es, &q, &eq) != 'a')
      panic("missing file for redirection");
    switch(tok){
    case '<':
      cmd = redircmd(cmd, q, eq, O_RDONLY, 0);
      break;
    case '>':
      cmd = redircmd(cmd, q, eq, O_WRONLY|O_CREATE, 1);
      break;
    case '+':  // >>
      cmd = redircmd(cmd, q, eq, O_WRONLY|O_CREATE, 1);
      break;
    }
  }
  return cmd;
}

struct cmd*
parseblock(char **ps, char *es)
{
  struct cmd *cmd;

  if(!peek(ps, es, "("))
    panic("parseblock");
  gettoken(ps, es, 0, 0);
  cmd = parseline(ps, es);
  if(!peek(ps, es, ")"))
    panic("syntax - missing )");
  gettoken(ps, es, 0, 0);
  cmd = parseredirs(cmd, ps, es);
  return cmd;
}

struct cmd*
parseexec(char **ps, char *es)
{
  char *q, *eq;
  int tok, argc;
  struct execcmd *cmd;
  struct cmd *ret;

  if(peek(ps, es, "("))
    return parseblock(ps, es);

  ret = execcmd();
  cmd = (struct execcmd*)ret;

  argc = 0;
  ret = parseredirs(ret, ps, es);
  while(!peek(ps, es, "|)&;")){
    if((tok=gettoken(ps, es, &q, &eq)) == 0)
      break;
    if(tok != 'a')
      panic("syntax");
    cmd->argv[argc] = q;
    cmd->eargv[argc] = eq;
    argc++;
    if(argc >= MAXARGS)
      panic("too many args");
    ret = parseredirs(ret, ps, es);
  }
  cmd->argv[argc] = 0;
  cmd->eargv[argc] = 0;
  return ret;
}

// NUL-terminate all the counted strings.
struct cmd*
nulterminate(struct cmd *cmd)
{
  int i;
  struct backcmd *bcmd;
  struct execcmd *ecmd;
  struct listcmd *lcmd;
  struct pipecmd *pcmd;
  struct redircmd *rcmd;

  if(cmd == 0)
    return 0;

  switch(cmd->type){
  case EXEC:
    ecmd = (struct execcmd*)cmd;
    for(i=0; ecmd->argv[i]; i++)
      *ecmd->eargv[i] = 0;
    break;

  case REDIR:
    rcmd = (struct redircmd*)cmd;
    nulterminate(rcmd->cmd);
    *rcmd->efile = 0;
    break;

  case PIPE:
    pcmd = (struct pipecmd*)cmd;
    nulterminate(pcmd->left);
    nulterminate(pcmd->right);
    break;

  case LIST:
    lcmd = (struct listcmd*)cmd;
    nulterminate(lcmd->left);
    nulterminate(lcmd->right);
    break;

  case BACK:
    bcmd = (struct backcmd*)cmd;
    nulterminate(bcmd->cmd);
    break;
  }
  return cmd;
}
