#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/param.h"

int
main(int argc, char **argv)
{
  if (argc < 2) {
    printf("xargs: need more parameters\n");
    exit();
  }

  char *all_args[MAXARG];
  char buf[512];

  int index = 0;

  // get the programme name
  for (;index < argc - 1; ++index) {
    all_args[index] = malloc(64);
    strcpy(all_args[index], argv[index + 1]);
  }

  // get the programme's parameters
  while (index < MAXARG) {
    int ret = read(0, buf, 512);
    if (ret == 0) {
      all_args[index] = 0;
      break;
    }

    int i = 0;

    while (i++ < ret) {
      if (buf[i] == '\n') {
        buf[i] = 0;
      }
    }

    i = 0;
    while (1) {
      if (buf[i] != 0 && buf[i] != '\n') {
        all_args[index] = malloc(64);
        strcpy(all_args[index], buf);
        index++;
      }

      while (i < ret && buf[i] != 0) i++; // jump over the current parm
      while (i < ret && buf[i] == 0) i++; // jump to the next head of the parm
      if (i >= ret)
        break;
    }
  }

  if (index >= MAXARG) {
    printf("xargs: too many parameters\n");
    exit();
  }

  exec(argv[1], all_args);
  
  exit();
}
