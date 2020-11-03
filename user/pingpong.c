#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int fd_pipe[2];

  if (pipe(fd_pipe) != 0) {
    printf("pipe() failed\n");
    exit();
  }

  int pid = fork();

  char psend[6] = "Child";
  char csend[7] = "Parent";
  char recv[10];

  if (pid > 0) {
    // parent
    write(fd_pipe[1], psend, 6);
    sleep(1);
    read(fd_pipe[0], recv, 7);
    printf("%d: %s\n", getpid(), recv);
  } else {
    // child
    read(fd_pipe[0], recv, 6);
    printf("%d: %s\n", getpid(), recv);
    write(fd_pipe[1], csend, 7);
    wait();
  }
  
  exit();
}
