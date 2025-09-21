// user/sixfive.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"

static const char *seps = " -\r\t\n./,";

static int issep(char c) {
  for (const char *p = seps; *p; p++) if (*p == c) return 1;
  return 0;
}

static void process(int fd) {
  char buf[512];
  char numbuf[64];
  int nbuf = 0;
  int in_token = 0;
  int valid = 0;

  for (;;) {
    int n = read(fd, buf, sizeof(buf));
    if (n < 0) {
      fprintf(2, "sixfive: read error\n");
      return;
    }
    if (n == 0) {
      if (in_token && valid && nbuf > 0) {
        numbuf[nbuf] = 0;
        int v = atoi(numbuf);
        if (v % 5 == 0 || v % 6 == 0)
          printf("%d\n", v);
      }
      break;
    }

    for (int i = 0; i < n; i++) {
      char c = buf[i];
      if (issep(c)) {
        if (in_token) {
          if (valid && nbuf > 0) {
            numbuf[nbuf] = 0;
            int v = atoi(numbuf);
            if (v % 5 == 0 || v % 6 == 0)
              printf("%d\n", v);
          }
          in_token = 0;
          valid = 0;
          nbuf = 0;
        }
      } else {
        if (!in_token) {
          in_token = 1;
          valid = 1;
          nbuf = 0;
        }
        if (c >= '0' && c <= '9') {
          if (nbuf + 1 < (int)sizeof(numbuf))
            numbuf[nbuf++] = c;
          else
            valid = 0;
        } else {
          valid = 0;
        }
      }
    }
  }
}

int
main(int argc, char *argv[])
{
  if (argc < 2) {
    fprintf(2, "usage: sixfive file...\n");
    exit(1);
  }

  for (int i = 1; i < argc; i++) {
    int fd = open(argv[i], O_RDONLY);
    if (fd < 0) {
      fprintf(2, "sixfive: cannot open %s\n", argv[i]);
      continue;
    }
    process(fd);
    close(fd);
  }
  exit(0);
}
