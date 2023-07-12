#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  if(argc < 2){ // 第一个参数是sleep 第二个参数是数字
    fprintf(2, "Usage: sleep [time]\n");
    exit(1);
  }

  int time = atoi(argv[1]);
  sleep(time);
  exit(0);
}
