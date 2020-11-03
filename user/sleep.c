#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

extern int sleep(int);

int
main(int argc, char *argv[])
{
  if(argc != 2){
    printf("sleep need one parameter\n");
    exit();
  }

	int time = atoi(argv[1]);
	sleep(time);
  
  exit();
}