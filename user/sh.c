// Shell.
#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"
#include "kernel/fs.h"
#include "kernel/stat.h"

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
void runcmd(struct cmd*) __attribute__((noreturn));

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
    exit(1);

  switch(cmd->type){
  default:
    panic("runcmd");

case EXEC:
    ecmd = (struct execcmd*)cmd;
    if(ecmd->argv[0] == 0)
      exit(1);

    // built-in: wait
    if(strcmp(ecmd->argv[0], "wait") == 0){
      while (wait(0) >= 0) { }
      exit(0);
    }

    exec(ecmd->argv[0], ecmd->argv);
    fprintf(2, "exec %s failed\n", ecmd->argv[0]);
    break;

  case REDIR:
    rcmd = (struct redircmd*)cmd;
    close(rcmd->fd);
    if(open(rcmd->file, rcmd->mode) < 0){
      fprintf(2, "open %s failed\n", rcmd->file);
      exit(1);
    }
    runcmd(rcmd->cmd);
    break;

  case LIST:
    lcmd = (struct listcmd*)cmd;
    if(fork1() == 0)
      runcmd(lcmd->left);
    wait(0);
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
    wait(0);
    wait(0);
    break;

  case BACK:
    bcmd = (struct backcmd*)cmd;
    if(fork1() == 0)
      runcmd(bcmd->cmd);
    break;
  }
  exit(0);
}
// ---------- TAB COMPLETION HELPERS ----------
// --- TAB COMPLETION HELPERS ---


// Returns new cursor position `cur` after completion (may be unchanged).
// Return new cursor position after completion (or unchanged if no match)



// Read a line with Tab completion. Returns -1 on EOF, 0 otherwise.
static int startswith(const char *s, const char *p) {
  int i = 0;
  while (p[i]) {
    if (s[i] != p[i]) return 0;
    i++;
  }
  return 1;
}

static void redraw_prompt_and_buf(const char *buf) {
  struct stat st;
  if (fstat(0, &st) >= 0 && st.type == T_DEVICE) {
    write(2, "$ ", 2);
  }
  write(1, buf, strlen(buf));
}

static int complete_current_token(char *buf, int cur, int max) {
  int start = cur - 1;
  while (start >= 0 && buf[start] != ' ' && buf[start] != '\t')
    start--;
  start++;

  char prefix[128];
  int plen = cur - start;
  if (plen >= (int)sizeof(prefix)) plen = sizeof(prefix) - 1;
  if (plen < 0) plen = 0;
  memmove(prefix, &buf[start], plen);
  prefix[plen] = 0;

  int fd = open(".", 0);
  if (fd < 0) return cur;

  struct dirent de;
  char matches[32][DIRSIZ+1];
  int mcount = 0;

  while (read(fd, &de, sizeof(de)) == sizeof(de)) {
    if (de.inum == 0) continue;
    char name[DIRSIZ+1];
    memmove(name, de.name, DIRSIZ);
    name[DIRSIZ] = 0;
    if (startswith(name, prefix)) {
      if (mcount < 32) {
        int L = strlen(name);
        if (L > DIRSIZ) L = DIRSIZ;
        memmove(matches[mcount], name, L+1);
        mcount++;
      }
    }
  }
  close(fd);

  if (mcount == 0) {
    return cur;
  } else if (mcount == 1) {
    const char *full = matches[0];
    int fl = strlen(full);
    int tail = cur - start;
    int add = fl - tail + 1;
    if (cur + add >= max) add = max - cur - 1;
    if (add < 0) add = 0;

    memmove(&buf[start], full, fl);
    if (start + fl < max - 1) {
      buf[start + fl] = ' ';
      buf[start + fl + 1] = 0;
      cur = start + fl + 1;
    } else {
      buf[start + fl] = 0;
      cur = start + fl;
    }
    write(1, &buf[start + tail], strlen(&buf[start + tail]));
    return cur;
  } else {
    write(1, "\n", 1);
    for (int i = 0; i < mcount; i++) {
      write(1, matches[i], strlen(matches[i]));
      write(1, "  ", 2);
    }
    write(1, "\n", 1);
    redraw_prompt_and_buf(buf);
    return cur;
  }
}


// ---------- HISTORY HELPERS (BEGIN) ----------

// number of saved commands and max length per line
#define HIST_MAX 32
#define LINE_MAX 128

static char history[HIST_MAX][LINE_MAX];
static int  hist_cnt   = 0;   // how many valid entries (<= HIST_MAX)
static int  hist_head  = 0;   // circular insert index (next slot)
static int  hist_view  = -1;  // -1 = editing current line; 0 = most recent; 1 = older; ...

static void
history_add(const char *line) {
  if (line[0] == 0) return; // ignore empty lines
  int n = strlen(line);
  if (n >= LINE_MAX) n = LINE_MAX - 1;

  memset(history[hist_head], 0, LINE_MAX);
  memmove(history[hist_head], line, n);
  history[hist_head][n] = 0;

  hist_head = (hist_head + 1) % HIST_MAX;
  if (hist_cnt < HIST_MAX) hist_cnt++;
  hist_view = -1; // reset browsing on new entry
}

// map logical view index (0 = newest) to physical circular index
static int
history_phys_index(int view) {
  int phys = hist_head - 1 - view;
  while (phys < 0) phys += HIST_MAX;
  return phys % HIST_MAX;
}

static int
history_up(char *buf, int max) {
  if (hist_cnt == 0) return 0;
  if (hist_view == -1) {
    hist_view = 0; // start at newest
  } else if (hist_view < hist_cnt - 1) {
    hist_view++;   // go older
  }
  int phys = history_phys_index(hist_view);
  int n = strlen(history[phys]);
  if (n >= max) n = max - 1;
  memmove(buf, history[phys], n);
  buf[n] = 0;
  return n;
}

static int
history_down(char *buf, int max) {
  if (hist_cnt == 0 || hist_view == -1) return strlen(buf);
  if (hist_view > 0) {
    hist_view--;  // go newer
    int phys = history_phys_index(hist_view);
    int n = strlen(history[phys]);
    if (n >= max) n = max - 1;
    memmove(buf, history[phys], n);
    buf[n] = 0;
    return n;
  } else {
    // leave history back to editing current (empty) line
    hist_view = -1;
    buf[0] = 0;
    return 0;
  }
}

// Clear the current terminal line and redraw prompt + buffer.
// Crude but effective: overwrite with spaces then carriage return.
static void
clear_line_and_redraw(const char *buf) {
  write(1, "\r", 1);
  // 80 spaces should be plenty for our tiny console
  write(1, "                                                                                ", 80);
  write(1, "\r", 1);
  redraw_prompt_and_buf(buf);
}

// ---------- HISTORY HELPERS (END) ----------




static int
read_line_with_completion(char *buf, int nbuf) {
  int cur = 0;
  buf[0] = 0;

  struct stat st;
  if (fstat(0, &st) >= 0 && st.type == T_DEVICE) {
    write(2, "$ ", 2);
  }

  for (;;) {
    char c;
    int n = read(0, &c, 1);
    if (n < 1) return -1;

    if (c == '\r') c = '\n';

    if (c == '\n') {
      write(1, "\n", 1);
      buf[cur] = 0;
      // store non-empty command in history
      if (buf[0] != 0) history_add(buf);
      return 0;
    } else if (c == '\t') {
      // Tab completion
      cur = complete_current_token(buf, cur, nbuf);
    } else if (c == 0x1b) {
      // ESC sequence: expect "[A" (up) or "[B" (down)
      char seq[2];
      int n1 = read(0, &seq[0], 1);
      int n2 = read(0, &seq[1], 1);
      if (n1 == 1 && n2 == 1 && seq[0] == '[') {
        if (seq[1] == 'A') {
          // Up: older commands
          cur = history_up(buf, nbuf);
          clear_line_and_redraw(buf);
        } else if (seq[1] == 'B') {
          // Down: newer commands
          cur = history_down(buf, nbuf);
          clear_line_and_redraw(buf);
        }
      }
    } else if (c == 0x7f || c == '\b') {
      // Backspace/DEL
      if (cur > 0) {
        cur--;
        buf[cur] = 0;
        write(1, "\b \b", 3);
      }
    } else {
      if (cur < nbuf - 1) {
        buf[cur++] = c;
        buf[cur] = 0;
        write(1, &c, 1);
      }
    }
  }
}





int
getcmd(char *buf, int nbuf)
{
  return read_line_with_completion(buf, nbuf);
}

// ---------- END HELPERS ----------









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
    char *cmd = buf;
    while (*cmd == ' ' || *cmd == '\t')
      cmd++;
    if (*cmd == '\n') // is a blank command
      continue;
    if(cmd[0] == 'c' && cmd[1] == 'd' && cmd[2] == ' '){
      // Chdir must be called by the parent, not the child.
      cmd[strlen(cmd)-1] = 0;  // chop \n
      if(chdir(cmd+3) < 0)
        fprintf(2, "cannot cd %s\n", cmd+3);
    } else {
      if(fork1() == 0)
        runcmd(parsecmd(cmd));
      wait(0);
    }
  }
  exit(0);
}

void
panic(char *s)
{
  fprintf(2, "%s\n", s);
  exit(1);
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
    fprintf(2, "leftovers: %s\n", s);
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
      cmd = redircmd(cmd, q, eq, O_WRONLY|O_CREATE|O_TRUNC, 1);
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
