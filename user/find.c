// user/find.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "kernel/param.h"   // MAXARG
#include "user/user.h"

static char*
fmtname(char *path)
{
  static char buf[DIRSIZ+1];
  char *p;

  // Find first character after last slash.
  for(p = path+strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;

  // Return blank-padded name.
  if(strlen(p) >= DIRSIZ)
    return p;
  memmove(buf, p, strlen(p));
  memset(buf+strlen(p), 0, sizeof(buf)-strlen(p));
  return buf;
}

static void run_exec(char *cmd, char **args, int nargs, char *file)
{
  // Build argv for exec: [cmd, args..., file, 0]
  char *argv[MAXARG];
  int i = 0;

  argv[i++] = cmd;
  for(int j = 0; j < nargs && i < MAXARG-2; j++)
    argv[i++] = args[j];

  // append the found file path
  if(i < MAXARG-1)
    argv[i++] = file;

  argv[i] = 0;

  int pid = fork();
  if(pid < 0){
    fprintf(2, "find: fork failed\n");
    return;
  }
  if(pid == 0){
    exec(cmd, argv);
    // If exec returns, it's an error.
    fprintf(2, "find: exec %s failed\n", cmd);
    exit(1);
  } else {
    wait(0);
  }
}

static void do_find(char *path, char *target, int use_exec, char *cmd, char **cmd_args, int cmd_nargs)
{
  char buf[512], *p;
  int fd;
  struct stat st;

  if((fd = open(path, 0)) < 0){
    fprintf(2, "find: cannot open %s\n", path);
    return;
  }
  if(fstat(fd, &st) < 0){
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }

  switch(st.type){
  case T_FILE: {
    char *name = fmtname(path);
    if(strcmp(name, target) == 0){
      if(use_exec)
        run_exec(cmd, cmd_args, cmd_nargs, path);
      else
        printf("%s\n", path);
    }
    break;
  }

  case T_DIR: {
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof(buf)){
      fprintf(2, "find: path too long\n");
      break;
    }
    strcpy(buf, path);
    p = buf + strlen(buf);
    *p++ = '/';

    struct dirent de;
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
      if(de.inum == 0) continue;
      if(strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0) continue;

      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;

      // Recurse
      do_find(buf, target, use_exec, cmd, cmd_args, cmd_nargs);
    }
    break;
  }
  }

  close(fd);
}

int
main(int argc, char *argv[])
{
  if(argc < 3){
    fprintf(2, "usage: find <path> <name> [-exec cmd [args...]]\n");
    exit(1);
  }

  char *start = argv[1];
  char *target = argv[2];

  int use_exec = 0;
  char *cmd = 0;
  char *cmd_args[MAXARG];
  int cmd_nargs = 0;

  // Look for "-exec" starting at argv[3]
  for(int i = 3; i < argc; i++){
    if(strcmp(argv[i], "-exec") == 0){
      if(i + 1 >= argc){
        fprintf(2, "find: -exec needs a command\n");
        exit(1);
      }
      use_exec = 1;
      cmd = argv[i+1];
      // Any remaining tokens after cmd are its args
      for(int j = i+2; j < argc && cmd_nargs < MAXARG-2; j++){
        cmd_args[cmd_nargs++] = argv[j];
      }
      break;
    }
  }

  do_find(start, target, use_exec, cmd, cmd_args, cmd_nargs);
  exit(0);
}

