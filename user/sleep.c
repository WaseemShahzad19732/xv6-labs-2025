#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  if(argc != 2){
    fprintf(2, "usage: sleep ticks\n");
    exit(1);
  }
  int n = atoi(argv[1]);
  if(n < 0) n = 0;

  // xv6 util lab: user-level sleep is called pause()
  if(pause(n) < 0){
    fprintf(2, "sleep: pause failed\n");
    exit(1);
  }
  exit(0);
}
